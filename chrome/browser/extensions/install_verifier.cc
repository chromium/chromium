// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/install_verifier.h"

#include <algorithm>
#include <string>
#include <utility>

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_macros.h"
#include "base/one_shot_event.h"
#include "base/ranges/algorithm.h"
#include "base/stl_util.h"
#include "base/trace_event/trace_event.h"
#include "base/values.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/extension_management.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/install_signer.h"
#include "chrome/browser/extensions/install_verifier_factory.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_switches.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/pref_names.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_url_handlers.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "ui/base/l10n/l10n_util.h"

namespace extensions {

namespace {

// This should only be set during tests.
ScopedInstallVerifierBypassForTest::ForceType* g_bypass_for_test = nullptr;

enum class VerifyStatus {
  NONE = 0,   // Do not request install signatures, and do not enforce them.
  BOOTSTRAP,  // Request install signatures, but do not enforce them.
  ENFORCE,    // Request install signatures, and enforce them.
  ENFORCE_STRICT,  // Same as ENFORCE, but hard fail if we can't fetch
                   // signatures.

  // This is used in histograms - do not remove or reorder entries above! Also
  // the "MAX" item below should always be the last element.
  VERIFY_STATUS_MAX
};

VerifyStatus GetExperimentStatus() {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING) && \
    (BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC))
  return VerifyStatus::ENFORCE;
#else
  return VerifyStatus::NONE;
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
}

VerifyStatus GetCommandLineStatus() {
  const base::CommandLine* cmdline = base::CommandLine::ForCurrentProcess();
  if (!InstallSigner::GetForcedNotFromWebstore().empty())
    return VerifyStatus::ENFORCE;

  if (cmdline->HasSwitch(::switches::kExtensionsInstallVerification)) {
    std::string value = cmdline->GetSwitchValueASCII(
        ::switches::kExtensionsInstallVerification);
    if (value == "bootstrap")
      return VerifyStatus::BOOTSTRAP;
    else if (value == "enforce_strict")
      return VerifyStatus::ENFORCE_STRICT;
    else
      return VerifyStatus::ENFORCE;
  }

  return VerifyStatus::NONE;
}

VerifyStatus GetStatus() {
  if (g_bypass_for_test) {
    switch (*g_bypass_for_test) {
      case ScopedInstallVerifierBypassForTest::kForceOn:
        return VerifyStatus::ENFORCE_STRICT;
      case ScopedInstallVerifierBypassForTest::kForceOff:
        return VerifyStatus::NONE;
    }
  }

  return std::max(GetExperimentStatus(), GetCommandLineStatus());
}

bool ShouldFetchSignature() {
  return GetStatus() >= VerifyStatus::BOOTSTRAP;
}

bool CanUseExtensionApis(const Extension& extension) {
  return extension.is_extension() || extension.is_legacy_packaged_app();
}

}  // namespace

InstallVerifier::InstallVerifier(ExtensionPrefs* prefs,
                                 content::BrowserContext* context)
    : prefs_(prefs), context_(context), bootstrap_check_complete_(false) {}

InstallVerifier::~InstallVerifier() = default;

// static
InstallVerifier* InstallVerifier::Get(
    content::BrowserContext* browser_context) {
  return InstallVerifierFactory::GetForBrowserContext(browser_context);
}

// static
bool InstallVerifier::ShouldEnforce() {
  return GetStatus() >= VerifyStatus::ENFORCE;
}

// static
bool InstallVerifier::NeedsVerification(const Extension& extension,
                                        content::BrowserContext* context) {
  return IsFromStore(extension, context) && CanUseExtensionApis(extension);
}

// static
bool InstallVerifier::IsFromStore(const Extension& extension,
                                  content::BrowserContext* context) {
  return extension.from_webstore() ||
         ExtensionManagementFactory::GetForBrowserContext(context)
             ->UpdatesFromWebstore(extension);
}

void InstallVerifier::Init() {
  TRACE_EVENT0("browser,startup", "extensions::InstallVerifier::Init");

  std::unique_ptr<InstallSignature> signature_from_prefs =
      InstallSignature::FromDict(prefs_->GetInstallSignature());
  if (signature_from_prefs.get()) {
    if (!InstallSigner::VerifySignature(*signature_from_prefs)) {
      DVLOG(1) << "Init - ignoring invalid signature";
    } else {
      signature_ = std::move(signature_from_prefs);
      GarbageCollect();
    }
  }

  ExtensionSystem::Get(context_)->ready().Post(
      FROM_HERE, base::BindOnce(&InstallVerifier::MaybeBootstrapSelf,
                                weak_factory_.GetWeakPtr()));
}

void InstallVerifier::VerifyAllExtensions() {
  AddMany(GetExtensionsToVerify(), ADD_ALL);
}

base::Time InstallVerifier::SignatureTimestamp() {
  if (signature_.get())
    return signature_->timestamp;
  else
    return base::Time();
}

bool InstallVerifier::IsKnownId(const std::string& id) const {
  return signature_.get() && (base::Contains(signature_->ids, id) ||
                              base::Contains(signature_->invalid_ids, id));
}

bool InstallVerifier::IsInvalid(const std::string& id) const {
  return ((signature_.get() && base::Contains(signature_->invalid_ids, id)));
}

void InstallVerifier::VerifyExtension(const std::string& extension_id) {
  ExtensionIdSet ids;
  ids.insert(extension_id);
  AddMany(ids, ADD_SINGLE);
}

void InstallVerifier::AddMany(const ExtensionIdSet& ids, OperationType type) {
  if (!ShouldFetchSignature()) {
    OnVerificationComplete(true, type);  // considered successful.
    return;
  }

  if (signature_.get()) {
    ExtensionIdSet not_allowed_yet =
        base::STLSetDifference<ExtensionIdSet>(ids, signature_->ids);
    if (not_allowed_yet.empty()) {
      OnVerificationComplete(true, type);  // considered successful.
      return;
    }
  }

  std::unique_ptr<InstallVerifier::PendingOperation> operation(
      new InstallVerifier::PendingOperation(type));
  operation->ids.insert(ids.begin(), ids.end());

  operation_queue_.push(std::move(operation));

  // If there are no ongoing pending requests, we need to kick one off.
  if (operation_queue_.size() == 1)
    BeginFetch();
}

void InstallVerifier::AddProvisional(const ExtensionIdSet& ids) {
  provisional_.insert(ids.begin(), ids.end());
  AddMany(ids, ADD_PROVISIONAL);
}

void InstallVerifier::Remove(const std::string& id) {
  ExtensionIdSet ids;
  ids.insert(id);
  RemoveMany(ids);
}

void InstallVerifier::RemoveMany(const ExtensionIdSet& ids) {
  if (!signature_.get() || !ShouldFetchSignature())
    return;

  if (base::ranges::any_of(ids, [this](const std::string& id) {
        return base::Contains(signature_->ids, id) ||
               base::Contains(signature_->invalid_ids, id);
      })) {
    return;
  }

  std::unique_ptr<InstallVerifier::PendingOperation> operation(
      new InstallVerifier::PendingOperation(InstallVerifier::REMOVE));
  operation->ids = ids;

  operation_queue_.push(std::move(operation));
  if (operation_queue_.size() == 1)
    BeginFetch();
}

bool InstallVerifier::AllowedByEnterprisePolicy(const std::string& id) const {
  return ExtensionManagementFactory::GetForBrowserContext(context_)
      ->IsInstallationExplicitlyAllowed(id);
}

std::string InstallVerifier::GetDebugPolicyProviderName() const {
  return std::string("InstallVerifier");
}

bool InstallVerifier::MustRemainDisabled(const Extension* extension,
                                         disable_reason::DisableReason* reason,
                                         std::u16string* error) const {
  CHECK(extension);
  if (!CanUseExtensionApis(*extension))
    return false;
  if (Manifest::IsUnpackedLocation(extension->location()))
    return false;
  if (extension->location() == mojom::ManifestLocation::kComponent)
    return false;
  if (AllowedByEnterprisePolicy(extension->id()))
    return false;

  bool verified = true;
  if (base::Contains(InstallSigner::GetForcedNotFromWebstore(),
                     extension->id())) {
    verified = false;
  } else if (!IsFromStore(*extension, context_)) {
    verified = false;
  } else if (!signature_ && (!bootstrap_check_complete_ ||
                             GetStatus() < VerifyStatus::ENFORCE_STRICT)) {
    // If we don't have a signature yet, we'll temporarily consider every
    // extension from the webstore verified to avoid false positives on existing
    // profiles hitting this code for the first time. The InstallVerifier
    // will bootstrap itself once the ExtensionsSystem is ready.
    // |verified| is already set to true.
  } else if (!IsVerified(extension->id())) {
    // Transient network failures can create a stale signature missing recently
    // added extension ids. To avoid false positives, consider all extensions to
    // be from the webstore unless the signature explicitly lists the extension
    // as invalid.
    if (!signature_ ||
        base::Contains(signature_->invalid_ids, extension->id()) ||
        GetStatus() >= VerifyStatus::ENFORCE_STRICT) {
      verified = false;
    }
  }

  if (!verified && !ShouldEnforce())
    verified = true;

  if (!verified) {
    DLOG(WARNING) << "Disabling extension " << extension->id() << " ('"
                  << extension->name()
                  << "') due to install verification failure. In tests you "
                  << "might want to use a ScopedInstallVerifierBypassForTest "
                  << "instance to prevent this.";

    if (reason)
      *reason = disable_reason::DISABLE_NOT_VERIFIED;
    if (error)
      *error = l10n_util::GetStringFUTF16(
          IDS_EXTENSIONS_ADDED_WITHOUT_KNOWLEDGE,
          l10n_util::GetStringUTF16(IDS_EXTENSION_WEB_STORE_TITLE));
  }
  return !verified;
}

InstallVerifier::PendingOperation::PendingOperation(OperationType type)
    : type(type) {}

InstallVerifier::PendingOperation::~PendingOperation() = default;

ExtensionIdSet InstallVerifier::GetExtensionsToVerify() const {
  ExtensionIdSet result;
  const ExtensionSet extensions =
      ExtensionRegistry::Get(context_)->GenerateInstalledExtensionsSet();
  for (const auto& extension : extensions) {
    if (NeedsVerification(*extension, context_)) {
      result.insert(extension->id());
    }
  }
  return result;
}

void InstallVerifier::MaybeBootstrapSelf() {
  ExtensionIdSet extension_ids = GetExtensionsToVerify();
  if ((signature_.get() == nullptr && ShouldFetchSignature()) ||
      base::ranges::any_of(extension_ids, [this](const std::string& id) {
        return !IsKnownId(id);
      })) {
    AddMany(extension_ids, ADD_ALL_BOOTSTRAP);
  } else {
    bootstrap_check_complete_ = true;
  }
}

void InstallVerifier::OnVerificationComplete(bool success, OperationType type) {
  switch (type) {
    case ADD_ALL:
    case ADD_ALL_BOOTSTRAP:
      bootstrap_check_complete_ = true;
      if (success) {
        // Iterate through the extensions and, if any are newly-verified and
        // should have the DISABLE_NOT_VERIFIED reason lifted, do so.
        const ExtensionSet& disabled_extensions =
            ExtensionRegistry::Get(context_)->disabled_extensions();
        for (ExtensionSet::const_iterator iter = disabled_extensions.begin();
             iter != disabled_extensions.end(); ++iter) {
          int disable_reasons = prefs_->GetDisableReasons((*iter)->id());
          if (disable_reasons & disable_reason::DISABLE_NOT_VERIFIED &&
              !MustRemainDisabled(iter->get(), nullptr, nullptr)) {
            prefs_->RemoveDisableReason((*iter)->id(),
                                        disable_reason::DISABLE_NOT_VERIFIED);
          }
        }
      }
      if (success || GetStatus() == VerifyStatus::ENFORCE_STRICT) {
        ExtensionSystem::Get(context_)
            ->extension_service()
            ->CheckManagementPolicy();
      }
      break;
    // We don't need to check disable reasons for provisional adds or removals.
    case ADD_PROVISIONAL:
    case ADD_SINGLE:
    case REMOVE:
      break;
  }
}

void InstallVerifier::GarbageCollect() {
  if (!ShouldFetchSignature()) {
    return;
  }
  CHECK(signature_.get());
  ExtensionIdSet leftovers = signature_->ids;
  leftovers.insert(signature_->invalid_ids.begin(),
                   signature_->invalid_ids.end());
  for (const auto& extension_id : prefs_->GetExtensions()) {
    leftovers.erase(extension_id);
  }
  if (!leftovers.empty()) {
    RemoveMany(leftovers);
  }
}

bool InstallVerifier::IsVerified(const std::string& id) const {
  return ((signature_.get() && base::Contains(signature_->ids, id)) ||
          base::Contains(provisional_, id));
}

void InstallVerifier::BeginFetch() {
  DCHECK(ShouldFetchSignature());

  // TODO(asargent) - It would be possible to coalesce all operations in the
  // queue into one fetch - we'd probably just need to change the queue to
  // hold (set of ids, list of operation type) pairs.
  CHECK(!operation_queue_.empty());
  const PendingOperation& operation = *operation_queue_.front();

  ExtensionIdSet ids_to_sign;
  if (signature_.get()) {
    ids_to_sign.insert(signature_->ids.begin(), signature_->ids.end());
  }
  if (operation.type == InstallVerifier::REMOVE) {
    for (const std::string& id : operation.ids) {
      if (base::Contains(ids_to_sign, id))
        ids_to_sign.erase(id);
    }
  } else {  // All other operation types are some form of "ADD".
    ids_to_sign.insert(operation.ids.begin(), operation.ids.end());
  }

  auto url_loader_factory = context_->GetDefaultStoragePartition()
                                ->GetURLLoaderFactoryForBrowserProcess();
  signer_ = std::make_unique<InstallSigner>(url_loader_factory, ids_to_sign);
  signer_->GetSignature(base::BindOnce(&InstallVerifier::SignatureCallback,
                                       weak_factory_.GetWeakPtr()));
}

void InstallVerifier::SaveToPrefs() {
  if (signature_.get())
    DCHECK(InstallSigner::VerifySignature(*signature_));

  if (!signature_.get() || signature_->ids.empty()) {
    DVLOG(1) << "SaveToPrefs - saving NULL";
    prefs_->SetInstallSignature(nullptr);
  } else {
    base::Value::Dict pref = signature_->ToDict();
    if (VLOG_IS_ON(1)) {
      DVLOG(1) << "SaveToPrefs - saving";

      DCHECK(InstallSigner::VerifySignature(*signature_));
      std::unique_ptr<InstallSignature> rehydrated =
          InstallSignature::FromDict(pref);
      DCHECK(InstallSigner::VerifySignature(*rehydrated));
    }
    prefs_->SetInstallSignature(&pref);
  }
}

void InstallVerifier::SignatureCallback(
    std::unique_ptr<InstallSignature> signature) {
  std::unique_ptr<PendingOperation> operation =
      std::move(operation_queue_.front());
  operation_queue_.pop();

  bool success = signature.get() && InstallSigner::VerifySignature(*signature);
  if (success) {
    signature_ = std::move(signature);
    SaveToPrefs();

    if (!provisional_.empty()) {
      // Update |provisional_| to remove ids that were successfully signed.
      provisional_ =
          base::STLSetDifference<ExtensionIdSet>(provisional_, signature_->ids);
    }
  }

  // TODO(asargent) - if this was something like a network error, we need to
  // do retries with exponential back off.
  OnVerificationComplete(success, operation->type);
  if (!operation_queue_.empty())
    BeginFetch();
}

ScopedInstallVerifierBypassForTest::ScopedInstallVerifierBypassForTest(
    ForceType force_type)
    : value_(force_type), old_value_(g_bypass_for_test) {
  g_bypass_for_test = &value_;
}

ScopedInstallVerifierBypassForTest::~ScopedInstallVerifierBypassForTest() {
  g_bypass_for_test = old_value_;
}

}  // namespace extensions
