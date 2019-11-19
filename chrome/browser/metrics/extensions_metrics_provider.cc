// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/extensions_metrics_provider.h"

#include <stddef.h>

#include <algorithm>
#include <memory>
#include <set>
#include <vector>

#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/install_verifier.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/metrics/metrics_log.h"
#include "components/metrics/metrics_state_manager.h"
#include "extensions/browser/disable_reason.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handlers/background_info.h"
#include "extensions/common/manifest_url_handlers.h"
#include "third_party/metrics_proto/system_profile.pb.h"
#include "third_party/smhasher/src/City.h"

using extensions::Extension;
using extensions::Manifest;
using metrics::ExtensionInstallProto;

namespace {

// The number of possible hash keys that a client may use.  The UMA client_id
// value is reduced modulo this value to produce the key used by that
// particular client.
const size_t kExtensionListClientKeys = 4096;

// The number of hash buckets into which extension IDs are mapped.  This sets
// the possible output range of the HashExtension function.
const size_t kExtensionListBuckets = 1024;

// Possible states for extensions. The order of these enum values is important,
// and is used when combining the state of multiple extensions and multiple
// profiles. Combining two states should always result in the higher state.
// Ex: One profile is in state FROM_STORE_VERIFIED, and another is in
// FROM_STORE_UNVERIFIED. The state of the two profiles together will be
// FROM_STORE_UNVERIFIED.
// This enum should be kept in sync with the corresponding enum in
// third_party/metrics_proto/system_profile.proto
enum ExtensionState {
  NO_EXTENSIONS,
  FROM_STORE_VERIFIED,
  FROM_STORE_UNVERIFIED,
  OFF_STORE
};

metrics::SystemProfileProto::ExtensionsState ExtensionStateAsProto(
    ExtensionState value) {
  switch (value) {
    case NO_EXTENSIONS:
      return metrics::SystemProfileProto::NO_EXTENSIONS;
    case FROM_STORE_VERIFIED:
      return metrics::SystemProfileProto::NO_OFFSTORE_VERIFIED;
    case FROM_STORE_UNVERIFIED:
      return metrics::SystemProfileProto::NO_OFFSTORE_UNVERIFIED;
    case OFF_STORE:
      return metrics::SystemProfileProto::HAS_OFFSTORE;
  }
  NOTREACHED();
  return metrics::SystemProfileProto::NO_EXTENSIONS;
}

// Determines if the |extension| is an extension (can use extension APIs) and is
// not from the webstore. If local information claims the extension is from the
// webstore, we attempt to verify with |verifier| by checking if it has been
// explicitly deemed invalid. If |verifier| is inactive or if the extension is
// unknown to |verifier|, the local information is trusted.
ExtensionState IsOffStoreExtension(
    const extensions::Extension& extension,
    const extensions::InstallVerifier& verifier) {
  if (!extension.is_extension() && !extension.is_legacy_packaged_app())
    return NO_EXTENSIONS;

  // Component extensions are considered safe.
  if (extensions::Manifest::IsComponentLocation(extension.location()))
    return NO_EXTENSIONS;

  if (verifier.AllowedByEnterprisePolicy(extension.id()))
    return NO_EXTENSIONS;

  if (!extensions::InstallVerifier::IsFromStore(extension))
    return OFF_STORE;

  // Local information about the extension implies it is from the store. We try
  // to use the install verifier to verify this.
  if (!verifier.IsKnownId(extension.id()))
    return FROM_STORE_UNVERIFIED;

  if (verifier.IsInvalid(extension.id()))
    return OFF_STORE;

  return FROM_STORE_VERIFIED;
}

// Finds the ExtensionState of |extensions|. The return value will be the
// highest (as defined by the order of ExtensionState) value of each extension
// in |extensions|.
ExtensionState CheckForOffStore(const extensions::ExtensionSet& extensions,
                                const extensions::InstallVerifier& verifier) {
  ExtensionState state = NO_EXTENSIONS;
  for (extensions::ExtensionSet::const_iterator it = extensions.begin();
       it != extensions.end() && state < OFF_STORE;
       ++it) {
    // Combine the state of each extension, always favoring the higher state as
    // defined by the order of ExtensionState.
    state = std::max(state, IsOffStoreExtension(**it, verifier));
  }
  return state;
}

ExtensionInstallProto::Type GetType(Manifest::Type type) {
  switch (type) {
    case Manifest::TYPE_UNKNOWN:
      return ExtensionInstallProto::UNKNOWN_TYPE;
    case Manifest::TYPE_EXTENSION:
      return ExtensionInstallProto::EXTENSION;
    case Manifest::TYPE_THEME:
      return ExtensionInstallProto::THEME;
    case Manifest::TYPE_USER_SCRIPT:
      return ExtensionInstallProto::USER_SCRIPT;
    case Manifest::TYPE_HOSTED_APP:
      return ExtensionInstallProto::HOSTED_APP;
    case Manifest::TYPE_LEGACY_PACKAGED_APP:
      return ExtensionInstallProto::LEGACY_PACKAGED_APP;
    case Manifest::TYPE_PLATFORM_APP:
      return ExtensionInstallProto::PLATFORM_APP;
    case Manifest::TYPE_SHARED_MODULE:
      return ExtensionInstallProto::SHARED_MODULE;
    case Manifest::TYPE_LOGIN_SCREEN_EXTENSION:
      return ExtensionInstallProto::LOGIN_SCREEN_EXTENSION;
    case Manifest::NUM_LOAD_TYPES:
      NOTREACHED();
      // Fall through.
  }
  return ExtensionInstallProto::UNKNOWN_TYPE;
}

ExtensionInstallProto::InstallLocation GetInstallLocation(
    Manifest::Location location) {
  switch (location) {
    case Manifest::INVALID_LOCATION:
      return ExtensionInstallProto::UNKNOWN_LOCATION;
    case Manifest::INTERNAL:
      return ExtensionInstallProto::INTERNAL;
    case Manifest::EXTERNAL_PREF:
      return ExtensionInstallProto::EXTERNAL_PREF;
    case Manifest::EXTERNAL_REGISTRY:
      return ExtensionInstallProto::EXTERNAL_REGISTRY;
    case Manifest::UNPACKED:
      return ExtensionInstallProto::UNPACKED;
    case Manifest::COMPONENT:
      return ExtensionInstallProto::COMPONENT;
    case Manifest::EXTERNAL_PREF_DOWNLOAD:
      return ExtensionInstallProto::EXTERNAL_PREF_DOWNLOAD;
    case Manifest::EXTERNAL_POLICY_DOWNLOAD:
      return ExtensionInstallProto::EXTERNAL_POLICY_DOWNLOAD;
    case Manifest::COMMAND_LINE:
      return ExtensionInstallProto::COMMAND_LINE;
    case Manifest::EXTERNAL_POLICY:
      return ExtensionInstallProto::EXTERNAL_POLICY;
    case Manifest::EXTERNAL_COMPONENT:
      return ExtensionInstallProto::EXTERNAL_COMPONENT;
    case Manifest::NUM_LOCATIONS:
      NOTREACHED();
      // Fall through.
  }
  return ExtensionInstallProto::UNKNOWN_LOCATION;
}

ExtensionInstallProto::ActionType GetActionType(const Manifest& manifest) {
  // Arbitrary order; each of these is mutually exclusive.
  if (manifest.HasKey(extensions::manifest_keys::kBrowserAction))
    return ExtensionInstallProto::BROWSER_ACTION;
  if (manifest.HasKey(extensions::manifest_keys::kPageAction))
    return ExtensionInstallProto::PAGE_ACTION;
  if (manifest.HasKey(extensions::manifest_keys::kSystemIndicator))
    return ExtensionInstallProto::SYSTEM_INDICATOR;
  return ExtensionInstallProto::NO_ACTION;
}

ExtensionInstallProto::BackgroundScriptType GetBackgroundScriptType(
    const Extension& extension) {
  // Arbitrary order; each of these is mutally exclusive.
  if (extensions::BackgroundInfo::HasPersistentBackgroundPage(&extension))
    return ExtensionInstallProto::PERSISTENT_BACKGROUND_PAGE;
  if (extensions::BackgroundInfo::HasLazyBackgroundPage(&extension))
    return ExtensionInstallProto::EVENT_PAGE;

  // If an extension had neither a persistent nor lazy background page, it must
  // not have a background page.
  DCHECK(!extensions::BackgroundInfo::HasBackgroundPage(&extension));
  return ExtensionInstallProto::NO_BACKGROUND_SCRIPT;
}

static_assert(extensions::disable_reason::DISABLE_REASON_LAST == (1LL << 17),
              "Adding a new disable reason? Be sure to include the new reason "
              "below, update the test to exercise it, and then adjust this "
              "value for DISABLE_REASON_LAST");
std::vector<ExtensionInstallProto::DisableReason> GetDisableReasons(
    const extensions::ExtensionId& id,
    extensions::ExtensionPrefs* prefs) {
  static struct {
    extensions::disable_reason::DisableReason disable_reason;
    ExtensionInstallProto::DisableReason proto_disable_reason;
  } disable_reason_map[] = {
      {extensions::disable_reason::DISABLE_USER_ACTION,
       ExtensionInstallProto::USER_ACTION},
      {extensions::disable_reason::DISABLE_PERMISSIONS_INCREASE,
       ExtensionInstallProto::PERMISSIONS_INCREASE},
      {extensions::disable_reason::DISABLE_RELOAD,
       ExtensionInstallProto::RELOAD},
      {extensions::disable_reason::DISABLE_UNSUPPORTED_REQUIREMENT,
       ExtensionInstallProto::UNSUPPORTED_REQUIREMENT},
      {extensions::disable_reason::DISABLE_SIDELOAD_WIPEOUT,
       ExtensionInstallProto::SIDELOAD_WIPEOUT},
      {extensions::disable_reason::DEPRECATED_DISABLE_UNKNOWN_FROM_SYNC,
       ExtensionInstallProto::UNKNOWN_FROM_SYNC},
      {extensions::disable_reason::DISABLE_NOT_VERIFIED,
       ExtensionInstallProto::NOT_VERIFIED},
      {extensions::disable_reason::DISABLE_GREYLIST,
       ExtensionInstallProto::GREYLIST},
      {extensions::disable_reason::DISABLE_CORRUPTED,
       ExtensionInstallProto::CORRUPTED},
      {extensions::disable_reason::DISABLE_REMOTE_INSTALL,
       ExtensionInstallProto::REMOTE_INSTALL},
      {extensions::disable_reason::DISABLE_EXTERNAL_EXTENSION,
       ExtensionInstallProto::EXTERNAL_EXTENSION},
      {extensions::disable_reason::DISABLE_UPDATE_REQUIRED_BY_POLICY,
       ExtensionInstallProto::UPDATE_REQUIRED_BY_POLICY},
      {extensions::disable_reason::DISABLE_CUSTODIAN_APPROVAL_REQUIRED,
       ExtensionInstallProto::CUSTODIAN_APPROVAL_REQUIRED},
      {extensions::disable_reason::DISABLE_BLOCKED_BY_POLICY,
       ExtensionInstallProto::BLOCKED_BY_POLICY},
  };

  int disable_reasons = prefs->GetDisableReasons(id);
  std::vector<ExtensionInstallProto::DisableReason> reasons;
  for (const auto& entry : disable_reason_map) {
    int mask = static_cast<int>(entry.disable_reason);
    if ((disable_reasons & mask) != 0) {
      reasons.push_back(entry.proto_disable_reason);
      disable_reasons &= ~mask;
    }
  }
  DCHECK_EQ(extensions::disable_reason::DisableReason::DISABLE_NONE,
            disable_reasons);

  return reasons;
}

ExtensionInstallProto::BlacklistState GetBlacklistState(
    const extensions::ExtensionId& id,
    extensions::ExtensionPrefs* prefs) {
  extensions::BlacklistState state = prefs->GetExtensionBlacklistState(id);
  switch (state) {
    case extensions::NOT_BLACKLISTED:
      return ExtensionInstallProto::NOT_BLACKLISTED;
    case extensions::BLACKLISTED_MALWARE:
      return ExtensionInstallProto::BLACKLISTED_MALWARE;
    case extensions::BLACKLISTED_SECURITY_VULNERABILITY:
      return ExtensionInstallProto::BLACKLISTED_SECURITY_VULNERABILITY;
    case extensions::BLACKLISTED_CWS_POLICY_VIOLATION:
      return ExtensionInstallProto::BLACKLISTED_CWS_POLICY_VIOLATION;
    case extensions::BLACKLISTED_POTENTIALLY_UNWANTED:
      return ExtensionInstallProto::BLACKLISTED_POTENTIALLY_UNWANTED;
    case extensions::BLACKLISTED_UNKNOWN:
      return ExtensionInstallProto::BLACKLISTED_UNKNOWN;
  }
  NOTREACHED();
  return ExtensionInstallProto::BLACKLISTED_UNKNOWN;
}

// Creates the install proto for a given |extension|. |now| is the current
// time, and |time_since_last_sample| is the elapsed time since the previous
// sample was recorded. These are curried in for testing purposes.
metrics::ExtensionInstallProto ConstructInstallProto(
    const extensions::Extension& extension,
    extensions::ExtensionPrefs* prefs,
    base::Time last_sample_time) {
  ExtensionInstallProto install;
  install.set_type(GetType(extension.manifest()->type()));
  install.set_install_location(GetInstallLocation(extension.location()));
  install.set_manifest_version(extension.manifest_version());
  install.set_action_type(GetActionType(*extension.manifest()));
  install.set_has_file_access(
      (extension.creation_flags() & Extension::ALLOW_FILE_ACCESS) != 0);
  install.set_has_incognito_access(prefs->IsIncognitoEnabled(extension.id()));
  install.set_is_from_store(extension.from_webstore());
  install.set_updates_from_store(
      extensions::ManifestURL::UpdatesFromGallery(&extension));
  install.set_is_from_bookmark(extension.from_bookmark());
  install.set_is_converted_from_user_script(
      extension.converted_from_user_script());
  install.set_is_default_installed(extension.was_installed_by_default());
  install.set_is_oem_installed(extension.was_installed_by_oem());
  install.set_background_script_type(GetBackgroundScriptType(extension));
  for (const ExtensionInstallProto::DisableReason reason :
       GetDisableReasons(extension.id(), prefs)) {
    install.add_disable_reasons(reason);
  }
  install.set_blacklist_state(GetBlacklistState(extension.id(), prefs));
  install.set_installed_in_this_sample_period(
      prefs->GetInstallTime(extension.id()) >= last_sample_time);

  return install;
}

// Returns all the extension installs for a given |profile|.
std::vector<metrics::ExtensionInstallProto> GetInstallsForProfile(
    Profile* profile,
    base::Time last_sample_time) {
  extensions::ExtensionPrefs* prefs = extensions::ExtensionPrefs::Get(profile);
  std::unique_ptr<extensions::ExtensionSet> extensions =
      extensions::ExtensionRegistry::Get(profile)
          ->GenerateInstalledExtensionsSet();
  std::vector<ExtensionInstallProto> installs;
  installs.reserve(extensions->size());
  for (const auto& extension : *extensions) {
    installs.push_back(
        ConstructInstallProto(*extension, prefs, last_sample_time));
  }

  return installs;
}

}  // namespace

ExtensionsMetricsProvider::ExtensionsMetricsProvider(
    metrics::MetricsStateManager* metrics_state_manager)
    : metrics_state_manager_(metrics_state_manager) {
  DCHECK(metrics_state_manager_);
}

ExtensionsMetricsProvider::~ExtensionsMetricsProvider() {
}

// static
int ExtensionsMetricsProvider::HashExtension(const std::string& extension_id,
                                             uint32_t client_key) {
  DCHECK_LE(client_key, kExtensionListClientKeys);
  std::string message =
      base::StringPrintf("%u:%s", client_key, extension_id.c_str());
  uint64_t output = CityHash64(message.data(), message.size());
  return output % kExtensionListBuckets;
}

std::unique_ptr<extensions::ExtensionSet>
ExtensionsMetricsProvider::GetInstalledExtensions(Profile* profile) {
  if (profile) {
    return extensions::ExtensionRegistry::Get(profile)
        ->GenerateInstalledExtensionsSet();
  }
  return std::unique_ptr<extensions::ExtensionSet>();
}

uint64_t ExtensionsMetricsProvider::GetClientID() {
  // TODO(blundell): Create a MetricsLog::ClientIDAsInt() API and call it
  // here as well as in MetricsLog's population of the client_id field of
  // the uma_proto.
  return metrics::MetricsLog::Hash(metrics_state_manager_->client_id());
}

void ExtensionsMetricsProvider::ProvideSystemProfileMetrics(
    metrics::SystemProfileProto* system_profile) {
  ProvideOffStoreMetric(system_profile);
  ProvideOccupiedBucketMetric(system_profile);
  ProvideExtensionInstallsMetrics(system_profile);
}

// static
metrics::ExtensionInstallProto
ExtensionsMetricsProvider::ConstructInstallProtoForTesting(
    const extensions::Extension& extension,
    extensions::ExtensionPrefs* prefs,
    base::Time last_sample_time) {
  return ConstructInstallProto(extension, prefs, last_sample_time);
}

// static
std::vector<metrics::ExtensionInstallProto>
ExtensionsMetricsProvider::GetInstallsForProfileForTesting(
    Profile* profile,
    base::Time last_sample_time) {
  return GetInstallsForProfile(profile, last_sample_time);
}

void ExtensionsMetricsProvider::ProvideOffStoreMetric(
    metrics::SystemProfileProto* system_profile) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  if (!profile_manager)
    return;

  ExtensionState state = NO_EXTENSIONS;

  // The off-store metric includes information from all loaded profiles at the
  // time when this metric is generated.
  std::vector<Profile*> profiles = profile_manager->GetLoadedProfiles();
  for (size_t i = 0u; i < profiles.size() && state < OFF_STORE; ++i) {
    extensions::InstallVerifier* verifier =
        extensions::InstallVerifier::Get(profiles[i]);

    std::unique_ptr<extensions::ExtensionSet> extensions(
        GetInstalledExtensions(profiles[i]));
    if (!extensions)
      continue;

    // Combine the state from each profile, always favoring the higher state as
    // defined by the order of ExtensionState.
    state = std::max(state, CheckForOffStore(*extensions.get(), *verifier));
  }

  system_profile->set_offstore_extensions_state(ExtensionStateAsProto(state));
}

void ExtensionsMetricsProvider::ProvideOccupiedBucketMetric(
    metrics::SystemProfileProto* system_profile) {
  // UMA reports do not support multiple profiles, but extensions are installed
  // per-profile.  We return the extensions installed in the primary profile.
  // In the future, we might consider reporting data about extensions in all
  // profiles.
  Profile* profile = cached_profile_.GetMetricsProfile();

  std::unique_ptr<extensions::ExtensionSet> extensions(
      GetInstalledExtensions(profile));
  if (!extensions)
    return;

  const int client_key = GetClientID() % kExtensionListClientKeys;

  std::set<int> buckets;
  for (extensions::ExtensionSet::const_iterator it = extensions->begin();
       it != extensions->end();
       ++it) {
    buckets.insert(HashExtension((*it)->id(), client_key));
  }

  for (auto it = buckets.begin(); it != buckets.end(); ++it) {
    system_profile->add_occupied_extension_bucket(*it);
  }
}

void ExtensionsMetricsProvider::ProvideExtensionInstallsMetrics(
    metrics::SystemProfileProto* system_profile) {
  std::vector<Profile*> profiles =
      g_browser_process->profile_manager()->GetLoadedProfiles();
  last_sample_time_ = base::Time::Now();
  for (Profile* profile : profiles) {
    std::vector<ExtensionInstallProto> installs =
        GetInstallsForProfile(profile, last_sample_time_);
    for (ExtensionInstallProto& install : installs)
      system_profile->add_extension_install()->Swap(&install);
  }
}
