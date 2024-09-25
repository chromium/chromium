// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/manifest_v2_experiment_manager.h"

#include "base/functional/callback_forward.h"
#include "base/metrics/histogram_functions.h"
#include "base/one_shot_event.h"
#include "base/strings/stringprintf.h"
#include "base/types/pass_key.h"
#include "chrome/browser/extensions/extension_management.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system_factory.h"
#include "chrome/browser/extensions/mv2_experiment_stage.h"
#include "chrome/browser/extensions/profile_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "extensions/browser/disable_reason.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_prefs_factory.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_factory.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/pref_names.h"
#include "extensions/browser/pref_types.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_features.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"

namespace extensions {

namespace {

// Whether to override the MV2 deprecation for testing purposes.
bool g_allow_mv2_for_testing = false;

// Returns the suffix to use for histograms related to the manifest location
// grouping.
const char* GetHistogramManifestLocation(mojom::ManifestLocation location) {
  switch (location) {
    case mojom::ManifestLocation::kComponent:
    case mojom::ManifestLocation::kExternalComponent:
      return "Component";
    case mojom::ManifestLocation::kExternalPolicy:
    case mojom::ManifestLocation::kExternalPolicyDownload:
      return "Policy";
    case mojom::ManifestLocation::kCommandLine:
    case mojom::ManifestLocation::kUnpacked:
      return "Unpacked";
    case mojom::ManifestLocation::kExternalRegistry:
    case mojom::ManifestLocation::kExternalPref:
    case mojom::ManifestLocation::kExternalPrefDownload:
      return "External";
    case mojom::ManifestLocation::kInternal:
      return "Internal";
    case mojom::ManifestLocation::kInvalidLocation:
      NOTREACHED();
  }
}

// Stores the bit for whether the user has acknowledged the MV2 deprecation
// notice for a given extension in the warning stage.
constexpr PrefMap kMV2DeprecationExtensionWarningAcknowledgedPref = {
    "mv2_deprecation_warning_ack", PrefType::kBool,
    PrefScope::kExtensionSpecific};

// Stores the bit for whether the user has acknowledged the MV2 deprecation
// notice for a given extension in the disabled stage.
constexpr PrefMap kMV2DeprecationExtensionDisabledAcknowledgedPref = {
    "mv2_deprecation_disabled_ack", PrefType::kBool,
    PrefScope::kExtensionSpecific};

// Stores a bit for whether the extension has been disabled as part of the
// MV2 deprecation.
constexpr PrefMap kMV2DeprecationDidDisablePref = {
    "mv2_deprecation_did_disable", PrefType::kBool,
    PrefScope::kExtensionSpecific};

// Stores a bit for whether the extension was re-enabled after being previously
// disabled as part of the MV2 deprecation.
// We store this separately from kMV2DeprecationDidDisablePref (as opposed to
// relying on an enum or a single bool) since there are multiple phases and
// causes for extensions to be disabled and re-enabled, and we only want to
// record that a user re-enables it if it was explicitly disabled by this phase
// of the experiment.
constexpr PrefMap kMV2DeprecationUserReEnabledPref = {
    "mv2_deprecation_user_re_enabled", PrefType::kBool,
    PrefScope::kExtensionSpecific};

class ManifestV2ExperimentManagerFactory : public ProfileKeyedServiceFactory {
 public:
  ManifestV2ExperimentManagerFactory();
  ManifestV2ExperimentManagerFactory(
      const ManifestV2ExperimentManagerFactory&) = delete;
  ManifestV2ExperimentManagerFactory& operator=(
      const ManifestV2ExperimentManagerFactory&) = delete;
  ~ManifestV2ExperimentManagerFactory() override = default;

  ManifestV2ExperimentManager* GetForBrowserContext(
      content::BrowserContext* context);

 private:
  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
};

ManifestV2ExperimentManagerFactory::ManifestV2ExperimentManagerFactory()
    : ProfileKeyedServiceFactory(
          "ManifestV2ExperimentManager",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kRedirectedToOriginal)
              .WithGuest(ProfileSelection::kRedirectedToOriginal)
              .WithAshInternals(ProfileSelection::kNone)
              .Build()) {
  DependsOn(ExtensionManagementFactory::GetInstance());
  DependsOn(ExtensionPrefsFactory::GetInstance());
  DependsOn(ExtensionSystemFactory::GetInstance());
  DependsOn(ExtensionRegistryFactory::GetInstance());
}

ManifestV2ExperimentManager*
ManifestV2ExperimentManagerFactory::GetForBrowserContext(
    content::BrowserContext* browser_context) {
  return static_cast<ManifestV2ExperimentManager*>(
      GetServiceForBrowserContext(browser_context, /*create=*/true));
}

KeyedService* ManifestV2ExperimentManagerFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new ManifestV2ExperimentManager(context);
}

bool ManifestV2ExperimentManagerFactory::ServiceIsCreatedWithBrowserContext()
    const {
  return true;
}

// Determines the current stage of the MV2 deprecation experiments.
MV2ExperimentStage CalculateCurrentExperimentStage() {
  // Return the "highest" stage that is currently active for the user.
  if (base::FeatureList::IsEnabled(
          extensions_features::kExtensionManifestV2Unsupported)) {
    return MV2ExperimentStage::kUnsupported;
  }

  if (base::FeatureList::IsEnabled(
          extensions_features::kExtensionManifestV2Disabled)) {
    return MV2ExperimentStage::kDisableWithReEnable;
  }

  if (base::FeatureList::IsEnabled(
          extensions_features::kExtensionManifestV2DeprecationWarning)) {
    return MV2ExperimentStage::kWarning;
  }

  return MV2ExperimentStage::kNone;
}

// Returns the pref that stores whether the user has acknowledged the MV2
// deprecation notice for a given extension in `experiment_stage`.
PrefMap GetExtensionAcknowledgedPrefFor(MV2ExperimentStage experiment_stage) {
  switch (experiment_stage) {
    case MV2ExperimentStage::kNone:
      // There is no notice for this stage, thus it cannot be acknowledged.
      NOTREACHED();
    case MV2ExperimentStage::kWarning:
      return kMV2DeprecationExtensionWarningAcknowledgedPref;
    case MV2ExperimentStage::kDisableWithReEnable:
      return kMV2DeprecationExtensionDisabledAcknowledgedPref;
    case MV2ExperimentStage::kUnsupported:
      NOTREACHED();
  }
}

// Returns the pref that stores whether the user has acknowledged the MV2
// deprecation global notice in `experiment_stage`.
PrefMap GetGlobalNoticeAcknowledgedPrefFor(
    MV2ExperimentStage experiment_stage) {
  switch (experiment_stage) {
    case MV2ExperimentStage::kNone:
      // There is no notice for this stage, thus it cannot be acknowledged.
      NOTREACHED();
    case MV2ExperimentStage::kWarning:
      return kMV2DeprecationWarningAcknowledgedGloballyPref;
    case MV2ExperimentStage::kDisableWithReEnable:
      return kMV2DeprecationDisabledAcknowledgedGloballyPref;
    case MV2ExperimentStage::kUnsupported:
      return kMV2DeprecationUnsupportedAcknowledgedGloballyPref;
  }
}

// Returns true if legacy extensions should be disabled, looking at both
// experiment stage and global state.
bool ShouldDisableLegacyExtensions(MV2ExperimentStage stage) {
  if (g_allow_mv2_for_testing) {
    // We allow legacy MV2 extensions for testing purposes.
    return false;
  }

  if (base::FeatureList::IsEnabled(
          extensions_features::kAllowLegacyMV2Extensions)) {
    // The user explicitly set the flag to allow legacy MV2 extensions. It's
    // important we retain this functionality so that developers of MV2
    // extensions used by enterprises can continue developing (and testing)
    // them for as long as the ExtensionManifestV2Availability enterprise policy
    // is supported.
    return false;
  }

  // Check the experiment stage to determine if extensions should be disabled.
  switch (stage) {
    case MV2ExperimentStage::kNone:
    case MV2ExperimentStage::kWarning:
      return false;
    case MV2ExperimentStage::kDisableWithReEnable:
    case MV2ExperimentStage::kUnsupported:
      return true;
  }
}

// Returns true if the user is allowed to re-enable disabled extensions in the
// given experiment `stage`.
bool UserCanReEnableExtensionsForStage(MV2ExperimentStage stage) {
  switch (stage) {
    case MV2ExperimentStage::kNone:
    case MV2ExperimentStage::kWarning:
    case MV2ExperimentStage::kDisableWithReEnable:
      return true;
    case MV2ExperimentStage::kUnsupported:
      return false;
  }
}

}  // namespace

ManifestV2ExperimentManager::ManifestV2ExperimentManager(
    content::BrowserContext* browser_context)
    : experiment_stage_(CalculateCurrentExperimentStage()),
      // Note: passing `ExtensionManagement` is safe and guaranteed to outlive
      // the `impact_checker_` because this class is a KeyedService that depends
      // on `ExtensionManagement`.
      impact_checker_(
          experiment_stage_,
          ExtensionManagementFactory::GetForBrowserContext(browser_context)),
      browser_context_(browser_context) {
  registry_observation_.Observe(ExtensionRegistry::Get(browser_context));

  ExtensionSystem::Get(browser_context)
      ->ready()
      .Post(FROM_HERE,
            base::BindOnce(&ManifestV2ExperimentManager::OnExtensionSystemReady,
                           weak_factory_.GetWeakPtr()));

  // Listen to management policy changes. `Unretained` is safe since the
  // `pref_change_registrar` is owned by this class.
  pref_change_registrar_.Init(
      Profile::FromBrowserContext(browser_context_)->GetPrefs());
  pref_change_registrar_.Add(
      pref_names::kManifestV2Availability,
      base::BindRepeating(
          &ManifestV2ExperimentManager::OnManagementPolicyChanged,
          base::Unretained(this)));
}

ManifestV2ExperimentManager::~ManifestV2ExperimentManager() = default;

// static
ManifestV2ExperimentManager* ManifestV2ExperimentManager::Get(
    content::BrowserContext* browser_context) {
  return static_cast<ManifestV2ExperimentManagerFactory*>(GetFactory())
      ->GetForBrowserContext(browser_context);
}

// static
BrowserContextKeyedServiceFactory* ManifestV2ExperimentManager::GetFactory() {
  static base::NoDestructor<ManifestV2ExperimentManagerFactory> g_factory;
  return g_factory.get();
}

MV2ExperimentStage ManifestV2ExperimentManager::GetCurrentExperimentStage() {
  return experiment_stage_;
}

bool ManifestV2ExperimentManager::IsExtensionAffected(
    const Extension& extension) {
  return impact_checker_.IsExtensionAffected(extension);
}

bool ManifestV2ExperimentManager::ShouldBlockExtensionInstallation(
    const ExtensionId& extension_id,
    int manifest_version,
    Manifest::Type manifest_type,
    mojom::ManifestLocation manifest_location,
    const HashedExtensionId& hashed_id) {
  // Only block extension installation during phases in which legacy extensions
  // are automatically disabled.
  if (!ShouldDisableLegacyExtensions(experiment_stage_)) {
    return false;
  }

  if (Manifest::IsUnpackedLocation(manifest_location)) {
    // Unpacked extensions are special-cased.
    // If MV2 is blocked by policy, then installation is blocked.
    // Otherwise, we allow unpacked extensions, even if MV2 extensions are
    // disabled. This is because it's critical for developers to continue being
    // able to develop MV2 extensions as long as they're supported in some form
    // in current version of Chrome.
    ExtensionManagement* extension_management =
        ExtensionManagementFactory::GetForBrowserContext(browser_context_);
    if (extension_management->IsAllowedManifestVersion(
            manifest_version, extension_id, manifest_type)) {
      return false;
    }
  }

  // Otherwise, if the extension is affected by the deprecation, it should be
  // blocked.
  return impact_checker_.IsExtensionAffected(extension_id, manifest_version,
                                             manifest_type, manifest_location,
                                             hashed_id);
}

bool ManifestV2ExperimentManager::DidUserAcknowledgeNotice(
    const ExtensionId& extension_id) {
  // There is no notice for kNone stage, thus it cannot be acknowledged.
  // The notice cannot be acknowledged in kUnsupported stage.
  if (experiment_stage_ == MV2ExperimentStage::kNone ||
      experiment_stage_ == MV2ExperimentStage::kUnsupported) {
    return false;
  }

  bool acknowledged = false;
  PrefMap pref = GetExtensionAcknowledgedPrefFor(experiment_stage_);
  return extension_prefs()->ReadPrefAsBoolean(extension_id, pref,
                                              &acknowledged) &&
         acknowledged;
}

void ManifestV2ExperimentManager::MarkNoticeAsAcknowledged(
    const ExtensionId& extension_id) {
  // There is no notice for kNone stage, thus it cannot be acknowledged.
  if (experiment_stage_ == MV2ExperimentStage::kNone) {
    return;
  }

  PrefMap pref = GetExtensionAcknowledgedPrefFor(experiment_stage_);
  extension_prefs()->SetBooleanPref(extension_id, pref, true);
}

bool ManifestV2ExperimentManager::DidUserAcknowledgeNoticeGlobally() {
  // There is no notice for kNone stage, thus it cannot be acknowledged.
  if (experiment_stage_ == MV2ExperimentStage::kNone) {
    return false;
  }

  PrefMap pref = GetGlobalNoticeAcknowledgedPrefFor(experiment_stage_);
  return extension_prefs()->GetPrefAsBoolean(pref);
}

void ManifestV2ExperimentManager::MarkNoticeAsAcknowledgedGlobally() {
  // There is no notice for kNone stage, thus it cannot be acknowledged.
  if (experiment_stage_ == MV2ExperimentStage::kNone) {
    return;
  }

  PrefMap pref = GetGlobalNoticeAcknowledgedPrefFor(experiment_stage_);
  extension_prefs()->SetBooleanPref(pref, true);
}

ExtensionPrefs* ManifestV2ExperimentManager::extension_prefs() {
  if (!extension_prefs_) {
    extension_prefs_ = ExtensionPrefs::Get(browser_context_);
  }
  return extension_prefs_;
}

void ManifestV2ExperimentManager::OnExtensionSystemReady() {
  CheckDisabledExtensions();
  DisableAffectedExtensions();

  EmitMetricsForProfileReady();

  is_manager_ready_ = true;
  on_manager_ready_callback_list_.Notify();
}

base::CallbackListSubscription
ManifestV2ExperimentManager::RegisterOnManagerReadyCallback(
    base::RepeatingClosure callback) {
  CHECK(!is_manager_ready_);
  return on_manager_ready_callback_list_.Add(std::move(callback));
}

void ManifestV2ExperimentManager::SetHasTriggeredDisabledDialog(
    bool has_triggered) {
  has_triggered_disabled_dialog_ = has_triggered;
}

void ManifestV2ExperimentManager::DisableAffectedExtensions() {
  if (!ShouldDisableLegacyExtensions(experiment_stage_)) {
    return;
  }

  ExtensionRegistry* extension_registry =
      ExtensionRegistry::Get(browser_context_);
  std::set<scoped_refptr<const Extension>> extensions_to_disable;

  bool user_reenable_allowed =
      UserCanReEnableExtensionsForStage(experiment_stage_);
  for (const auto& extension : extension_registry->enabled_extensions()) {
    if (!impact_checker_.IsExtensionAffected(*extension)) {
      continue;
    }

    if (user_reenable_allowed && DidUserReEnableExtension(extension->id())) {
      // The user explicitly chose to re-enable the extension after it was
      // disabled, and that's allowed in this experiment stage. Allow it to
      // remain enabled.
      continue;
    }

    extensions_to_disable.insert(extension);
  }

  ExtensionService* extension_service =
      ExtensionSystem::Get(browser_context_)->extension_service();
  for (const auto& extension : extensions_to_disable) {
    extension_service->DisableExtension(
        extension->id(), disable_reason::DISABLE_UNSUPPORTED_MANIFEST_VERSION);
    extension_prefs()->SetBooleanPref(extension->id(),
                                      kMV2DeprecationDidDisablePref, true);
  }
}

void ManifestV2ExperimentManager::CheckDisabledExtensions() {
  ExtensionRegistry* extension_registry =
      ExtensionRegistry::Get(browser_context_);
  ExtensionSet disabled_extensions;
  // Loop through all disabled extensions. For each, check if they should be
  // re-enabled (e.g. because they've updated to MV3 or a change in policy
  // settings).
  // Use a copy of the set to avoid changing the set while iterating.
  disabled_extensions.InsertAll(extension_registry->disabled_extensions());
  for (const auto& extension : disabled_extensions) {
    MaybeReEnableExtension(*extension);
  }
}

void ManifestV2ExperimentManager::MaybeReEnableExtension(
    const Extension& extension) {
  if (!extension_prefs()->HasDisableReason(
          extension.id(),
          disable_reason::DISABLE_UNSUPPORTED_MANIFEST_VERSION)) {
    // We only care about extensions that were disabled for this reason.
    return;
  }

  // Check if the extension is still affected *and* whether the environment is
  // still one in which extensions should be disabled. It's possible the user
  // moved from a later experiment stage to an earlier one or set a feature
  // flag, in which case extensions should be re-enabled.
  if (impact_checker_.IsExtensionAffected(extension) &&
      ShouldDisableLegacyExtensions(experiment_stage_)) {
    return;
  }

  ExtensionService* extension_service =
      ExtensionSystem::Get(browser_context_)->extension_service();
  // Remove the bit that the extension was disabled by the MV2 deprecation,
  // since it no longer is. This also ensures we don't count it as user-
  // re-enabled, if it gets re-enabled below.
  extension_prefs()->SetBooleanPref(extension.id(),
                                    kMV2DeprecationDidDisablePref, false);
  // Remove the disable reason (possibly re-enabling the extension).
  extension_service->RemoveDisableReasonAndMaybeEnable(
      extension.id(), disable_reason::DISABLE_UNSUPPORTED_MANIFEST_VERSION);
}

bool ManifestV2ExperimentManager::DidUserReEnableExtension(
    const ExtensionId& extension_id) {
  bool acknowledged = false;
  return extension_prefs()->ReadPrefAsBoolean(
             extension_id, kMV2DeprecationUserReEnabledPref, &acknowledged) &&
         acknowledged;
}

void ManifestV2ExperimentManager::EmitMetricsForProfileReady() {
  if (!ShouldDisableLegacyExtensions(experiment_stage_)) {
    // Don't bother reporting MV2-specific metrics if the user isn't in an
    // environment in which extensions could be disabled.
    return;
  }

  if (!profile_util::ProfileCanUseNonComponentExtensions(
          Profile::FromBrowserContext(browser_context_))) {
    // Don't report metrics if the user can't install extensions in this
    // profile.
    return;
  }

  ExtensionRegistry* extension_registry =
      ExtensionRegistry::Get(browser_context_);

  auto emit_state_for_mv2_extension = [this](const Extension& extension,
                                             bool is_enabled) {
    if (extension.manifest_version() != 2) {
      return;
    }

    if (extension.GetType() != Manifest::TYPE_EXTENSION &&
        extension.GetType() != Manifest::TYPE_LOGIN_SCREEN_EXTENSION) {
      return;
    }

    if (Manifest::IsComponentLocation(extension.location())) {
      return;
    }

    bool user_reenabled = DidUserReEnableExtension(extension.id());
    MV2ExtensionState extension_state = MV2ExtensionState::kUnaffected;
    if (!impact_checker_.IsExtensionAffected(extension)) {
      extension_state = MV2ExtensionState::kUnaffected;
    } else if (is_enabled && user_reenabled) {
      extension_state = MV2ExtensionState::kUserReEnabled;
    } else if (extension_prefs()->HasDisableReason(
                   extension.id(),
                   disable_reason::DISABLE_UNSUPPORTED_MANIFEST_VERSION)) {
      CHECK(!is_enabled);
      CHECK(experiment_stage_ == MV2ExperimentStage::kUnsupported ||
            experiment_stage_ == MV2ExperimentStage::kDisableWithReEnable);
      extension_state = experiment_stage_ == MV2ExperimentStage::kUnsupported
                            ? MV2ExtensionState::kHardDisabled
                            : MV2ExtensionState::kSoftDisabled;
    } else {
      extension_state = MV2ExtensionState::kOther;
    }

    std::string histogram_name =
        base::StringPrintf("Extensions.MV2Deprecation.MV2ExtensionState.%s",
                           GetHistogramManifestLocation(extension.location()));

    base::UmaHistogramEnumeration(histogram_name, extension_state);
  };

  for (const auto& extension : extension_registry->enabled_extensions()) {
    emit_state_for_mv2_extension(*extension, /*is_enabled=*/true);
  }
  for (const auto& extension : extension_registry->disabled_extensions()) {
    emit_state_for_mv2_extension(*extension, /*is_enabled=*/false);
  }
}

void ManifestV2ExperimentManager::RecordUkmForExtension(
    const GURL& extension_url,
    ExtensionMV2DeprecationAction action) {
  if (experiment_stage_ != MV2ExperimentStage::kDisableWithReEnable) {
    // The UKM is only emitted for the "disable with re-enable" phase. We do
    // not need UKM for the "hard disable" phase (as the only action available
    // to the user is to remove or find alternatives).
    return;
  }

  ukm::builders::Extensions_MV2ExtensionHandledInSoftDisable(
      ukm::UkmRecorder::GetSourceIdForExtensionUrl(
          base::PassKey<ManifestV2ExperimentManager>(), extension_url))
      .SetAction(static_cast<int64_t>(action))
      .Record(ukm::UkmRecorder::Get());
}

void ManifestV2ExperimentManager::OnExtensionLoaded(
    content::BrowserContext* browser_context,
    const Extension* extension) {
  bool was_extension_disabled_by_mv2_deprecation = false;
  extension_prefs()->ReadPrefAsBoolean(
      extension->id(), kMV2DeprecationDidDisablePref,
      &was_extension_disabled_by_mv2_deprecation);
  if (!was_extension_disabled_by_mv2_deprecation) {
    return;
  }

  extension_prefs()->SetBooleanPref(extension->id(),
                                    kMV2DeprecationUserReEnabledPref, true);
  RecordUkmForExtension(extension->url(),
                        ExtensionMV2DeprecationAction::kReEnabled);
}

void ManifestV2ExperimentManager::OnExtensionInstalled(
    content::BrowserContext* browser_context,
    const Extension* extension,
    bool is_update) {
  if (!is_update) {
    // We would only ever re-enable a disabled extension if it was already
    // installed. No need to look at new installs.
    return;
  }

  MaybeReEnableExtension(*extension);
}

void ManifestV2ExperimentManager::OnExtensionUninstalled(
    content::BrowserContext* browser_context,
    const Extension* extension,
    UninstallReason uninstall_reason) {
  // We only care about user uninstallations...
  if (uninstall_reason != UNINSTALL_REASON_USER_INITIATED) {
    return;
  }
  // ... of extensions that were disabled by the MV2 deprecation.
  bool was_extension_disabled_by_mv2_deprecation = false;
  extension_prefs()->ReadPrefAsBoolean(
      extension->id(), kMV2DeprecationDidDisablePref,
      &was_extension_disabled_by_mv2_deprecation);
  if (!was_extension_disabled_by_mv2_deprecation) {
    return;
  }

  RecordUkmForExtension(extension->url(),
                        ExtensionMV2DeprecationAction::kRemoved);
}

void ManifestV2ExperimentManager::OnManagementPolicyChanged() {
  // The management policy has changed. Go through all disabled extensions to
  // check if any should be re-enabled, and go through all enabled extensions
  // to see if any should be disabled (if the experiment is active).
  CheckDisabledExtensions();
  DisableAffectedExtensions();
}

bool ManifestV2ExperimentManager::DidUserReEnableExtensionForTesting(
    const ExtensionId& extension_id) {
  return DidUserReEnableExtension(extension_id);
}

void ManifestV2ExperimentManager::DisableAffectedExtensionsForTesting() {
  DisableAffectedExtensions();
}

void ManifestV2ExperimentManager::EmitMetricsForProfileReadyForTesting() {
  EmitMetricsForProfileReady();
}

base::AutoReset<bool> ManifestV2ExperimentManager::AllowMV2ExtensionsForTesting(
    base::PassKey<ScopedTestMV2Enabler> pass_key) {
  return base::AutoReset<bool>(&g_allow_mv2_for_testing, true);
}

}  // namespace extensions
