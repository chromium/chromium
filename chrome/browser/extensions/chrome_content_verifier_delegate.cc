// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/extensions/chrome_content_verifier_delegate.h"

#include <algorithm>
#include <memory>
#include <set>
#include <vector>

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/escape.h"
#include "base/strings/string_util.h"
#include "base/syslog_logging.h"
#include "base/version.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "build/config/chromebox_for_meetings/buildflags.h"
#include "chrome/browser/extensions/corrupted_extension_reinstaller.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/install_verifier.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension_constants.h"
#include "extensions/browser/disable_reason.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/management_policy.h"
#include "extensions/browser/pref_types.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/extension_urls.h"
#include "extensions/common/extensions_client.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_url_handlers.h"
#include "extensions/common/switches.h"
#include "net/base/backoff_entry.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/extensions/extension_assets_manager_chromeos.h"
#endif

namespace extensions {

namespace {

std::optional<ChromeContentVerifierDelegate::VerifyInfo::Mode>&
GetModeForTesting() {
  static std::optional<ChromeContentVerifierDelegate::VerifyInfo::Mode>
      testing_mode;
  return testing_mode;
}

const char kContentVerificationExperimentName[] =
    "ExtensionContentVerification";

ChromeContentVerifierDelegate::GetVerifyInfoTestOverride::VerifyInfoCallback*
    g_verify_info_test_callback = nullptr;

}  // namespace

ChromeContentVerifierDelegate::GetVerifyInfoTestOverride::
    GetVerifyInfoTestOverride(VerifyInfoCallback callback)
    : callback_(std::move(callback)) {
  DCHECK_EQ(nullptr, g_verify_info_test_callback)
      << "Nested overrides are not supported.";
  g_verify_info_test_callback = &callback_;
}

ChromeContentVerifierDelegate::GetVerifyInfoTestOverride::
    ~GetVerifyInfoTestOverride() {
  DCHECK_EQ(&callback_, g_verify_info_test_callback)
      << "Nested overrides are not supported.";
  g_verify_info_test_callback = nullptr;
}

ChromeContentVerifierDelegate::VerifyInfo::VerifyInfo(Mode mode,
                                                      bool is_from_webstore,
                                                      bool should_repair)
    : mode(mode),
      is_from_webstore(is_from_webstore),
      should_repair(should_repair) {}

// static
ChromeContentVerifierDelegate::VerifyInfo::Mode
ChromeContentVerifierDelegate::GetDefaultMode() {
  if (GetModeForTesting())
    return *GetModeForTesting();
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();

#if BUILDFLAG(PLATFORM_CFM)
  if (command_line->HasSwitch(switches::kDisableAppContentVerification)) {
    return VerifyInfo::Mode::NONE;
  }
#endif  // BUILDFLAG(PLATFORM_CFM)

  VerifyInfo::Mode experiment_value;
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  experiment_value = VerifyInfo::Mode::ENFORCE_STRICT;
#else
  experiment_value = VerifyInfo::Mode::NONE;
#endif
  const std::string group =
      base::FieldTrialList::FindFullName(kContentVerificationExperimentName);
  if (group == "EnforceStrict")
    experiment_value = VerifyInfo::Mode::ENFORCE_STRICT;
  else if (group == "Enforce")
    experiment_value = VerifyInfo::Mode::ENFORCE;
  else if (group == "Bootstrap")
    experiment_value = VerifyInfo::Mode::BOOTSTRAP;
  else if (group == "None")
    experiment_value = VerifyInfo::Mode::NONE;

  // The field trial value that normally comes from the server can be
  // overridden on the command line, which we don't want to allow since
  // malware can set chrome command line flags. There isn't currently a way
  // to find out what the server-provided value is in this case, so we
  // conservatively default to the strictest mode if we detect our experiment
  // name being overridden.
  if (command_line->HasSwitch(::switches::kForceFieldTrials)) {
    std::string forced_trials =
        command_line->GetSwitchValueASCII(::switches::kForceFieldTrials);
    if (forced_trials.find(kContentVerificationExperimentName) !=
        std::string::npos)
      experiment_value = VerifyInfo::Mode::ENFORCE_STRICT;
  }

  VerifyInfo::Mode cmdline_value = VerifyInfo::Mode::NONE;
  if (command_line->HasSwitch(::switches::kExtensionContentVerification)) {
    std::string switch_value = command_line->GetSwitchValueASCII(
        ::switches::kExtensionContentVerification);
    if (switch_value == ::switches::kExtensionContentVerificationBootstrap)
      cmdline_value = VerifyInfo::Mode::BOOTSTRAP;
    else if (switch_value == ::switches::kExtensionContentVerificationEnforce)
      cmdline_value = VerifyInfo::Mode::ENFORCE;
    else if (switch_value ==
             ::switches::kExtensionContentVerificationEnforceStrict)
      cmdline_value = VerifyInfo::Mode::ENFORCE_STRICT;
    else
      // If no value was provided (or the wrong one), just default to enforce.
      cmdline_value = VerifyInfo::Mode::ENFORCE;
  }

  // We don't want to allow the command-line flags to eg disable enforcement
  // if the experiment group says it should be on, or malware may just modify
  // the command line flags. So return the more restrictive of the 2 values.
  return std::max(experiment_value, cmdline_value);
}

// static
void ChromeContentVerifierDelegate::SetDefaultModeForTesting(
    std::optional<VerifyInfo::Mode> mode) {
  DCHECK(!GetModeForTesting() || !mode)
      << "Verification mode already overridden, unset it first.";
  GetModeForTesting() = mode;
}

ChromeContentVerifierDelegate::ChromeContentVerifierDelegate(
    content::BrowserContext* context)
    : context_(context), default_mode_(GetDefaultMode()) {}

ChromeContentVerifierDelegate::~ChromeContentVerifierDelegate() {
}

ContentVerifierDelegate::VerifierSourceType
ChromeContentVerifierDelegate::GetVerifierSourceType(
    const Extension& extension) {
  const VerifyInfo info = GetVerifyInfo(extension);
  if (info.mode == VerifyInfo::Mode::NONE)
    return VerifierSourceType::NONE;
  if (info.is_from_webstore)
    return VerifierSourceType::SIGNED_HASHES;
  return VerifierSourceType::UNSIGNED_HASHES;
}

ContentVerifierKey ChromeContentVerifierDelegate::GetPublicKey() {
  return ContentVerifierKey(kWebstoreSignaturesPublicKey,
                            kWebstoreSignaturesPublicKeySize);
}

GURL ChromeContentVerifierDelegate::GetSignatureFetchUrl(
    const ExtensionId& extension_id,
    const base::Version& version) {
  // TODO(asargent) Factor out common code from the extension updater's
  // ManifestFetchData class that can be shared for use here.
  std::string id_part = "id=" + extension_id;
  std::string version_part = "v=" + version.GetString();
  std::string x_value = base::EscapeQueryParamValue(
      base::JoinString({"uc", "installsource=signature", id_part, version_part},
                       "&"),
      true);
  std::string query = "response=redirect&x=" + x_value;

  GURL base_url = extension_urls::GetWebstoreUpdateUrl();
  GURL::Replacements replacements;
  replacements.SetQueryStr(query);
  return base_url.ReplaceComponents(replacements);
}

std::set<base::FilePath> ChromeContentVerifierDelegate::GetBrowserImagePaths(
    const Extension* extension) {
  return ExtensionsClient::Get()->GetBrowserImagePaths(extension);
}

void ChromeContentVerifierDelegate::VerifyFailed(
    const ExtensionId& extension_id,
    ContentVerifyJob::FailureReason reason) {
  ExtensionRegistry* registry = ExtensionRegistry::Get(context_);
  const Extension* extension =
      registry->enabled_extensions().GetByID(extension_id);
  if (!extension)
    return;

  ExtensionSystem* system = ExtensionSystem::Get(context_);
  if (!system->extension_service()) {
    // Some tests will add an extension to the registry, but there are no
    // subsystems.
    return;
  }

  ExtensionService* service = system->extension_service();
  CorruptedExtensionReinstaller* corrupted_extension_reinstaller =
      service->corrupted_extension_reinstaller();

  const VerifyInfo info = GetVerifyInfo(*extension);

  if (reason == ContentVerifyJob::MISSING_ALL_HASHES) {
    // If the failure was due to hashes missing, only "enforce_strict" would
    // disable the extension, but not "enforce".
    if (info.mode != VerifyInfo::Mode::ENFORCE_STRICT)
      return;

    // If a non-webstore extension has no computed hashes for content
    // verification, leave it as is for now.
    // See https://crbug.com/958794#c22 for more details.
    // TODO(crbug.com/40669814): Schedule the extension for reinstall.
    if (!info.is_from_webstore) {
      if (!base::Contains(would_be_reinstalled_ids_, extension_id)) {
        corrupted_extension_reinstaller->RecordPolicyReinstallReason(
            CorruptedExtensionReinstaller::PolicyReinstallReason::
                NO_UNSIGNED_HASHES_FOR_NON_WEBSTORE_SKIP);
        would_be_reinstalled_ids_.insert(extension_id);
      }
      return;
    }
  }

  SYSLOG(WARNING) << "Corruption detected in extension " << extension_id
                  << " installed at: " << extension->path().value()
                  << ", from webstore: " << info.is_from_webstore
                  << ", corruption reason: " << reason
                  << ", should be repaired: " << info.should_repair
                  << ", extension location: " << extension->location();

  const bool should_disable = info.mode >= VerifyInfo::Mode::ENFORCE;
  // Configuration when we should repair extension, but not disable it, is
  // invalid.
  DCHECK(!info.should_repair || should_disable);

  if (!should_disable) {
    if (!base::Contains(would_be_disabled_ids_, extension_id)) {
      would_be_disabled_ids_.insert(extension_id);
    }
    return;
  }

  if (info.should_repair) {
    if (corrupted_extension_reinstaller->IsReinstallForCorruptionExpected(
            extension_id))
      return;
    corrupted_extension_reinstaller->ExpectReinstallForCorruption(
        extension_id,
        info.is_from_webstore
            ? CorruptedExtensionReinstaller::PolicyReinstallReason::
                  CORRUPTION_DETECTED_WEBSTORE
            : CorruptedExtensionReinstaller::PolicyReinstallReason::
                  CORRUPTION_DETECTED_NON_WEBSTORE,
        extension->location());
    service->DisableExtension(extension_id, disable_reason::DISABLE_CORRUPTED);
    // Attempt to reinstall.
    corrupted_extension_reinstaller->NotifyExtensionDisabledDueToCorruption();
    return;
  }

  DCHECK(should_disable);
  service->DisableExtension(extension_id, disable_reason::DISABLE_CORRUPTED);
  ExtensionPrefs::Get(context_)->IncrementPref(kCorruptedDisableCount);
  base::UmaHistogramEnumeration("Extensions.CorruptExtensionDisabledReason",
                                reason, ContentVerifyJob::FAILURE_REASON_MAX);
}

void ChromeContentVerifierDelegate::Shutdown() {}

bool ChromeContentVerifierDelegate::IsFromWebstore(
    const Extension& extension) const {
  // Use the InstallVerifier's |IsFromStore| method to avoid discrepancies
  // between which extensions are considered in-store.
  // See https://crbug.com/766806 for details.
  if (!InstallVerifier::IsFromStore(extension, context_)) {
    // It's possible that the webstore update url was overridden for testing
    // so also consider extensions with the default (production) update url
    // to be from the store as well. Therefore update URL is compared with
    // |GetDefaultWebstoreUpdateUrl|, not the |GetWebstoreUpdateUrl| used by
    // |IsWebstoreUpdateUrl|.
    ExtensionManagement* extension_management =
        ExtensionManagementFactory::GetForBrowserContext(context_);
    if (extension_management->GetEffectiveUpdateURL(extension) !=
        extension_urls::GetDefaultWebstoreUpdateUrl()) {
      return false;
    }
  }
  return true;
}

ChromeContentVerifierDelegate::VerifyInfo
ChromeContentVerifierDelegate::GetVerifyInfo(const Extension& extension) const {
  if (g_verify_info_test_callback) {
    return g_verify_info_test_callback->Run(extension);
  }

  ManagementPolicy* management_policy =
      ExtensionSystem::Get(context_)->management_policy();

  // Magement policy may be not configured in some tests.
  bool should_repair = management_policy &&
                       management_policy->ShouldRepairIfCorrupted(&extension);
  bool is_from_webstore = IsFromWebstore(extension);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (ExtensionAssetsManagerChromeOS::IsSharedInstall(&extension)) {
    return VerifyInfo(VerifyInfo::Mode::ENFORCE_STRICT, is_from_webstore,
                      should_repair);
  }
#endif

  if (should_repair)
    return VerifyInfo(default_mode_, is_from_webstore, should_repair);

  if (!extension.is_extension() && !extension.is_legacy_packaged_app())
    return VerifyInfo(VerifyInfo::Mode::NONE, is_from_webstore, should_repair);
  if (!Manifest::IsAutoUpdateableLocation(extension.location()))
    return VerifyInfo(VerifyInfo::Mode::NONE, is_from_webstore, should_repair);
  if (!is_from_webstore)
    return VerifyInfo(VerifyInfo::Mode::NONE, is_from_webstore, should_repair);

  return VerifyInfo(default_mode_, is_from_webstore, should_repair);
}

}  // namespace extensions
