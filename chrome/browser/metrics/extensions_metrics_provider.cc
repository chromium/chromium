// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/extensions_metrics_provider.h"

#include <stddef.h>

#include <algorithm>
#include <memory>
#include <optional>
#include <set>
#include <vector>

#include "base/hash/legacy_hash.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/chrome_content_browser_client_extensions_part.h"
#include "chrome/browser/extensions/extension_management.h"
#include "chrome/browser/extensions/install_verifier.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/metrics/metrics_log.h"
#include "components/metrics/metrics_state_manager.h"
#include "extensions/browser/blocklist_extension_prefs.h"
#include "extensions/browser/disable_reason.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extension_util.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/features/feature_developer_mode_only.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handlers/background_info.h"
#include "third_party/metrics_proto/system_profile.pb.h"

using extensions::Extension;
using extensions::Manifest;
using extensions::mojom::ManifestLocation;
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
  NOTREACHED_IN_MIGRATION();
  return metrics::SystemProfileProto::NO_EXTENSIONS;
}

// Determines if the |extension| is an extension (can use extension APIs) and is
// not from the webstore. If local information claims the extension is from the
// webstore, we attempt to verify with |verifier| by checking if it has been
// explicitly deemed invalid. If |verifier| is inactive or if the extension is
// unknown to |verifier|, the local information is trusted.
ExtensionState IsOffStoreExtension(const extensions::Extension& extension,
                                   const extensions::InstallVerifier& verifier,
                                   content::BrowserContext* context) {
  if (!extension.is_extension() && !extension.is_legacy_packaged_app())
    return NO_EXTENSIONS;

  // Component extensions are considered safe.
  if (extensions::Manifest::IsComponentLocation(extension.location()))
    return NO_EXTENSIONS;

  if (verifier.AllowedByEnterprisePolicy(extension.id()))
    return NO_EXTENSIONS;

  if (!extensions::InstallVerifier::IsFromStore(extension, context))
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
                                const extensions::InstallVerifier& verifier,
                                content::BrowserContext* context) {
  ExtensionState state = NO_EXTENSIONS;
  for (extensions::ExtensionSet::const_iterator it = extensions.begin();
       it != extensions.end() && state < OFF_STORE; ++it) {
    // Combine the state of each extension, always favoring the higher state as
    // defined by the order of ExtensionState.
    state = std::max(state, IsOffStoreExtension(**it, verifier, context));
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
    case Manifest::TYPE_CHROMEOS_SYSTEM_EXTENSION:
      // TODO(mgawad): introduce new CHROMEOS_SYSTEM_EXTENSION type.
      return ExtensionInstallProto::EXTENSION;
    case Manifest::NUM_LOAD_TYPES:
      NOTREACHED_IN_MIGRATION();
      // Fall through.
  }
  return ExtensionInstallProto::UNKNOWN_TYPE;
}

ExtensionInstallProto::InstallLocation GetInstallLocation(
    ManifestLocation location) {
  switch (location) {
    case ManifestLocation::kInvalidLocation:
      return ExtensionInstallProto::UNKNOWN_LOCATION;
    case ManifestLocation::kInternal:
      return ExtensionInstallProto::INTERNAL;
    case ManifestLocation::kExternalPref:
      return ExtensionInstallProto::EXTERNAL_PREF;
    case ManifestLocation::kExternalRegistry:
      return ExtensionInstallProto::EXTERNAL_REGISTRY;
    case ManifestLocation::kUnpacked:
      return ExtensionInstallProto::UNPACKED;
    case ManifestLocation::kComponent:
      return ExtensionInstallProto::COMPONENT;
    case ManifestLocation::kExternalPrefDownload:
      return ExtensionInstallProto::EXTERNAL_PREF_DOWNLOAD;
    case ManifestLocation::kExternalPolicyDownload:
      return ExtensionInstallProto::EXTERNAL_POLICY_DOWNLOAD;
    case ManifestLocation::kCommandLine:
      return ExtensionInstallProto::COMMAND_LINE;
    case ManifestLocation::kExternalPolicy:
      return ExtensionInstallProto::EXTERNAL_POLICY;
    case ManifestLocation::kExternalComponent:
      return ExtensionInstallProto::EXTERNAL_COMPONENT;
  }
  return ExtensionInstallProto::UNKNOWN_LOCATION;
}

ExtensionInstallProto::ActionType GetActionType(const Manifest& manifest) {
  // Arbitrary order; each of these is mutually exclusive.
  if (manifest.FindKey(extensions::manifest_keys::kBrowserAction))
    return ExtensionInstallProto::BROWSER_ACTION;
  if (manifest.FindKey(extensions::manifest_keys::kPageAction))
    return ExtensionInstallProto::PAGE_ACTION;
  if (manifest.FindKey(extensions::manifest_keys::kSystemIndicator))
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
  if (extensions::BackgroundInfo::IsServiceWorkerBased(&extension))
    return ExtensionInstallProto::SERVICE_WORKER;

  // If an extension had neither a persistent background page, a lazy
  // background page nor a service worker based background script, it must not
  // have a background script.
  DCHECK(!extensions::BackgroundInfo::HasBackgroundPage(&extension));
  return ExtensionInstallProto::NO_BACKGROUND_SCRIPT;
}

static_assert(extensions::disable_reason::DISABLE_REASON_LAST == (1LL << 25),
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
      {extensions::disable_reason::DISABLE_REINSTALL,
       ExtensionInstallProto::REINSTALL},
      {extensions::disable_reason::DISABLE_NOT_ALLOWLISTED,
       ExtensionInstallProto::NOT_ALLOWLISTED},
      {extensions::disable_reason::DISABLE_NOT_ASH_KEEPLISTED,
       ExtensionInstallProto::NOT_ASH_KEEPLISTED},
      {extensions::disable_reason::
           DISABLE_PUBLISHED_IN_STORE_REQUIRED_BY_POLICY,
       ExtensionInstallProto::PUBLISHED_IN_STORE_REQUIRED_BY_POLICY},
      {extensions::disable_reason::DISABLE_UNSUPPORTED_MANIFEST_VERSION,
       ExtensionInstallProto::UNSUPPORTED_MANIFEST_VERSION},
      {extensions::disable_reason::DISABLE_UNSUPPORTED_DEVELOPER_EXTENSION,
       ExtensionInstallProto::UNSUPPORTED_DEVELOPER_EXTENSION},
  };

  int disable_reasons = prefs->GetDisableReasons(id);
  DCHECK_EQ(
      0, disable_reasons &
             extensions::disable_reason::DEPRECATED_DISABLE_UNKNOWN_FROM_SYNC)
      << "Encountered bad disable reason: " << disable_reasons;
  std::vector<ExtensionInstallProto::DisableReason> reasons;
  for (const auto& entry : disable_reason_map) {
    int mask = static_cast<int>(entry.disable_reason);
    if ((disable_reasons & mask) != 0) {
      reasons.push_back(entry.proto_disable_reason);
      disable_reasons &= ~mask;
    }
  }
  if (disable_reasons !=
      extensions::disable_reason::DisableReason::DISABLE_NONE) {
    // Record any unexpected disable reasons - these are likely deprecated
    // reason(s) that have not been migrated over in a few clients. Use this
    // histogram to determine how many clients are affected to decide what
    // action to take (if any).
    base::UmaHistogramSparse("Extensions.DeprecatedDisableReasonsObserved",
                             disable_reasons);
  }

  return reasons;
}

ExtensionInstallProto::BlacklistState GetBlacklistState(
    const extensions::ExtensionId& id,
    extensions::ExtensionPrefs* prefs) {
  extensions::BitMapBlocklistState state =
      extensions::blocklist_prefs::GetExtensionBlocklistState(id, prefs);
  switch (state) {
    case extensions::BitMapBlocklistState::NOT_BLOCKLISTED:
      return ExtensionInstallProto::NOT_BLACKLISTED;
    case extensions::BitMapBlocklistState::BLOCKLISTED_MALWARE:
      return ExtensionInstallProto::BLACKLISTED_MALWARE;
    case extensions::BitMapBlocklistState::BLOCKLISTED_SECURITY_VULNERABILITY:
      return ExtensionInstallProto::BLACKLISTED_SECURITY_VULNERABILITY;
    case extensions::BitMapBlocklistState::BLOCKLISTED_CWS_POLICY_VIOLATION:
      return ExtensionInstallProto::BLACKLISTED_CWS_POLICY_VIOLATION;
    case extensions::BitMapBlocklistState::BLOCKLISTED_POTENTIALLY_UNWANTED:
      return ExtensionInstallProto::BLACKLISTED_POTENTIALLY_UNWANTED;
  }
  NOTREACHED_IN_MIGRATION();
  return ExtensionInstallProto::BLACKLISTED_UNKNOWN;
}

// Creates the install proto for a given |extension|. |now| is the current
// time, and |time_since_last_sample| is the elapsed time since the previous
// sample was recorded. These are curried in for testing purposes.
metrics::ExtensionInstallProto ConstructInstallProto(
    const extensions::Extension& extension,
    extensions::ExtensionPrefs* prefs,
    base::Time last_sample_time,
    extensions::ExtensionManagement* extension_management,
    bool in_extensions_developer_mode) {
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
      extension_management->UpdatesFromWebstore(extension));
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
      prefs->GetLastUpdateTime(extension.id()) >= last_sample_time);
  install.set_in_extensions_developer_mode(in_extensions_developer_mode);

  return install;
}

// Returns all the extension installs for a given |profile|.
std::vector<metrics::ExtensionInstallProto> GetInstallsForProfile(
    Profile* profile,
    base::Time last_sample_time) {
  bool in_extensions_developer_mode = extensions::GetCurrentDeveloperMode(
      extensions::util::GetBrowserContextId(profile));
  extensions::ExtensionPrefs* prefs = extensions::ExtensionPrefs::Get(profile);
  const extensions::ExtensionSet extensions =
      extensions::ExtensionRegistry::Get(profile)
          ->GenerateInstalledExtensionsSet();
  std::vector<ExtensionInstallProto> installs;
  installs.reserve(extensions.size());
  extensions::ExtensionManagement* extension_management =
      extensions::ExtensionManagementFactory::GetForBrowserContext(profile);
  for (const auto& extension : extensions) {
    installs.push_back(ConstructInstallProto(
        *extension, prefs, last_sample_time, extension_management,
        in_extensions_developer_mode));
  }

  return installs;
}

}  // namespace

ExtensionsMetricsProvider::ExtensionsMetricsProvider(
    metrics::MetricsStateManager* metrics_state_manager)
    : metrics_state_manager_(metrics_state_manager) {
  DCHECK(metrics_state_manager_);
}

ExtensionsMetricsProvider::~ExtensionsMetricsProvider() = default;

// static
int ExtensionsMetricsProvider::HashExtension(const std::string& extension_id,
                                             uint32_t client_key) {
  DCHECK_LE(client_key, kExtensionListClientKeys);
  std::string message =
      base::StringPrintf("%u:%s", client_key, extension_id.c_str());
  uint64_t output =
      base::legacy::CityHash64(base::as_bytes(base::make_span(message)));
  return output % kExtensionListBuckets;
}

std::optional<extensions::ExtensionSet>
ExtensionsMetricsProvider::GetInstalledExtensions(Profile* profile) {
  // Some profiles cannot have extensions, such as the System Profile.
  if (!profile || extensions::ChromeContentBrowserClientExtensionsPart::
                      AreExtensionsDisabledForProfile(profile)) {
    return std::nullopt;
  }

  extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(profile);
  DCHECK(registry);
  return registry->GenerateInstalledExtensionsSet();
}

uint64_t ExtensionsMetricsProvider::GetClientID() const {
  // TODO(blundell): Create a MetricsLog::ClientIDAsInt() API and call it
  // here as well as in MetricsLog's population of the client_id field of
  // the uma_proto.
  return metrics::MetricsLog::Hash(metrics_state_manager_->client_id());
}

void ExtensionsMetricsProvider::ProvideCurrentSessionData(
    metrics::ChromeUserMetricsExtension* uma_proto) {
  Profile* profile = cached_profile_.GetMetricsProfile();
  if (!profile) {
    return;
  }

  bool in_extensions_developer_mode = extensions::GetCurrentDeveloperMode(
      extensions::util::GetBrowserContextId(profile));
  base::UmaHistogramBoolean("Extensions.DeveloperModeStatusEnabled",
                            in_extensions_developer_mode);
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
    base::Time last_sample_time,
    Profile* profile) {
  extensions::ExtensionManagement* extension_management =
      extensions::ExtensionManagementFactory::GetForBrowserContext(profile);
  bool in_extensions_developer_mode = extensions::GetCurrentDeveloperMode(
      extensions::util::GetBrowserContextId(profile));
  return ConstructInstallProto(extension, prefs, last_sample_time,
                               extension_management,
                               in_extensions_developer_mode);
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
    std::optional<extensions::ExtensionSet> extensions =
        GetInstalledExtensions(profiles[i]);
    if (!extensions)
      continue;

    extensions::InstallVerifier* verifier =
        extensions::InstallVerifier::Get(profiles[i]);
    DCHECK(verifier);

    // Combine the state from each profile, always favoring the higher state as
    // defined by the order of ExtensionState.
    state =
        std::max(state, CheckForOffStore(*extensions, *verifier, profiles[i]));
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

  std::optional<extensions::ExtensionSet> extensions =
      GetInstalledExtensions(profile);
  if (!extensions)
    return;

  const int client_key = GetClientID() % kExtensionListClientKeys;

  std::set<int> buckets;
  for (extensions::ExtensionSet::const_iterator it = extensions->begin();
       it != extensions->end(); ++it) {
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
    if (extensions::ChromeContentBrowserClientExtensionsPart::
            AreExtensionsDisabledForProfile(profile)) {
      continue;
    }

    std::vector<ExtensionInstallProto> installs =
        GetInstallsForProfile(profile, last_sample_time_);
    for (ExtensionInstallProto& install : installs)
      system_profile->add_extension_install()->Swap(&install);
  }
}
