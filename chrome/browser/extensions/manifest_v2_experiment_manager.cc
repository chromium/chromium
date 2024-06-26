// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/manifest_v2_experiment_manager.h"

#include "base/one_shot_event.h"
#include "chrome/browser/extensions/extension_management.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system_factory.h"
#include "chrome/browser/extensions/mv2_experiment_stage.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "extensions/browser/disable_reason.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_prefs_factory.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_factory.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/pref_types.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_features.h"

namespace extensions {

namespace {

// Stores the bit for whether the user has acknowledged the MV2 deprecation
// warning for a given extension.
constexpr PrefMap kMV2DeprecationExtensionWarningAcknowledgedPref = {
    "mv2_deprecation_warning_ack", PrefType::kBool,
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
          extensions_features::kExtensionManifestV2Disabled)) {
    return MV2ExperimentStage::kDisableWithReEnable;
  }

  if (base::FeatureList::IsEnabled(
          extensions_features::kExtensionManifestV2DeprecationWarning)) {
    return MV2ExperimentStage::kWarning;
  }

  return MV2ExperimentStage::kNone;
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
  // Only block extension installation during the disablement phase.
  if (experiment_stage_ != MV2ExperimentStage::kDisableWithReEnable) {
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

bool ManifestV2ExperimentManager::DidUserAcknowledgeWarning(
    const ExtensionId& extension_id) {
  bool acknowledged = false;
  return extension_prefs()->ReadPrefAsBoolean(
             extension_id, kMV2DeprecationExtensionWarningAcknowledgedPref,
             &acknowledged) &&
         acknowledged;
}

void ManifestV2ExperimentManager::MarkWarningAsAcknowledged(
    const ExtensionId& extension_id) {
  extension_prefs()->SetBooleanPref(
      extension_id, kMV2DeprecationExtensionWarningAcknowledgedPref, true);
}

bool ManifestV2ExperimentManager::DidUserAcknowledgeWarningGlobally() {
  return extension_prefs()->GetPrefAsBoolean(
      kMV2DeprecationWarningAcknowledgedGloballyPref);
}

void ManifestV2ExperimentManager::MarkWarningAsAcknowledgedGlobally() {
  extension_prefs()->SetBooleanPref(
      kMV2DeprecationWarningAcknowledgedGloballyPref, true);
}

ExtensionPrefs* ManifestV2ExperimentManager::extension_prefs() {
  if (!extension_prefs_) {
    extension_prefs_ = ExtensionPrefs::Get(browser_context_);
  }
  return extension_prefs_;
}

void ManifestV2ExperimentManager::OnExtensionSystemReady() {
  if (GetCurrentExperimentStage() != MV2ExperimentStage::kDisableWithReEnable) {
    return;
  }

  // TODO(https://crbug.com/339061151): Add metrics for disabled extension
  // counts.
  DisableAffectedExtensions();
}

void ManifestV2ExperimentManager::DisableAffectedExtensions() {
  ExtensionRegistry* extension_registry =
      ExtensionRegistry::Get(browser_context_);
  std::set<scoped_refptr<const Extension>> extensions_to_disable;
  for (const auto& extension : extension_registry->enabled_extensions()) {
    if (!impact_checker_.IsExtensionAffected(*extension)) {
      continue;
    }

    if (DidUserReEnableExtension(extension->id())) {
      // The user explicitly chose to re-enable the extension after it was
      // disabled. Allow it to remain enabled.
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

void ManifestV2ExperimentManager::MaybeReEnableExtension(
    const Extension& extension) {
  if (!extension_prefs()->HasDisableReason(
          extension.id(),
          disable_reason::DISABLE_UNSUPPORTED_MANIFEST_VERSION)) {
    return;
  }

  if (impact_checker_.IsExtensionAffected(extension)) {
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

bool ManifestV2ExperimentManager::DidUserReEnableExtensionForTesting(
    const ExtensionId& extension_id) {
  return DidUserReEnableExtension(extension_id);
}

void ManifestV2ExperimentManager::DisableAffectedExtensionsForTesting() {
  DisableAffectedExtensions();
}

}  // namespace extensions
