// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_MANIFEST_V2_EXPERIMENT_MANAGER_H_
#define CHROME_BROWSER_EXTENSIONS_MANIFEST_V2_EXPERIMENT_MANAGER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/extensions/mv2_deprecation_impact_checker.h"
#include "components/keyed_service/core/keyed_service.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/common/extension_id.h"

class BrowserContextKeyedServiceFactory;

namespace content {
class BrowserContext;
}  // namespace content

namespace extensions {
class Extension;
class ExtensionPrefs;
enum class MV2ExperimentStage;

// The central class responsible for managing experiments related to the MV2
// deprecation.
class ManifestV2ExperimentManager : public KeyedService,
                                    public ExtensionRegistryObserver {
 public:
  explicit ManifestV2ExperimentManager(
      content::BrowserContext* browser_context);
  ManifestV2ExperimentManager(const ManifestV2ExperimentManager&) = delete;
  ManifestV2ExperimentManager& operator=(const ManifestV2ExperimentManager&) =
      delete;
  ~ManifestV2ExperimentManager() override;

  // Retrieves the ManifestV2ExperimentManager associated with the given
  // `browser_context`. Note this instance is shared between on- and off-the-
  // record contexts.
  static ManifestV2ExperimentManager* Get(
      content::BrowserContext* browser_context);

  // Returns the singleton instance of the factory for this KeyedService.
  static BrowserContextKeyedServiceFactory* GetFactory();

  // Returns the current experiment stage for the MV2 experiment.  Note: You
  // should only use this for determining the experiment stage itself. For
  // determining if an extension is affected, use IsExtensionAffected() below.
  MV2ExperimentStage GetCurrentExperimentStage();

  // Returns true if the given `extension` is affected by the MV2 deprecation.
  // This may be false if, e.g., the extension is policy-installed.
  bool IsExtensionAffected(const Extension& extension);

  // Returns true if a new installation of the given `extension_id` should be
  // blocked.
  bool ShouldBlockExtensionInstallation(
      const ExtensionId& extension_id,
      int manifest_version,
      Manifest::Type manifest_type,
      mojom::ManifestLocation manifest_location,
      const HashedExtensionId& hashed_id);

  // Returns true if the given `extension_id` has been acknowledged by the user
  // during the warning stage of the MV2 deprecation.
  bool DidUserAcknowledgeWarning(const ExtensionId& extension_id);

  // Called to indicate the user chose to acknowledge the warning for the given
  // `extension_id`.
  void MarkWarningAsAcknowledged(const ExtensionId& extension_id);

  // Returns true if the user has acknowledge the global warning for the MV2
  // deprecation.
  bool DidUserAcknowledgeWarningGlobally();

  // Called to indicate the user chose to acknowledge the MV2 deprecation global
  // warning.
  void MarkWarningAsAcknowledgedGlobally();

  bool DidUserReEnableExtensionForTesting(const ExtensionId& extension_id);

 private:
  // Lazily initialize and access `extension_prefs_`. We do this lazily because:
  // - This service is created on Profile creation.
  // - A bunch of unit tests override ExtensionPrefs after Profile creation, but
  //   before the "real" test starts.
  // As such, if we instantiated ExtensionPrefs in the constructor, it would be
  // the improper ExtensionPrefs object and would trigger raw_ptr violations.
  ExtensionPrefs* extension_prefs();

  // Called when the extension system has finished its initialization steps.
  void OnExtensionSystemReady();

  // Disables any Manifest V2 extensions that are affected by the experiment,
  // if the user hasn't chosen to re-enable them.
  void DisableAffectedExtensions();

  // Returns true if a user re-enabled an extension after it was explicitly
  // disabled by the MV2 deprecation.
  bool DidUserReEnableExtension(const ExtensionId& extension_id);

  // ExtensionRegistry:
  void OnExtensionLoaded(content::BrowserContext* browser_context,
                         const Extension* extension) override;

  // The current stage of the MV2 deprecation experiments.
  const MV2ExperimentStage experiment_stage_;

  // A helper object to determine if a given extension is affected by the
  // MV2 deprecation experiments.
  MV2DeprecationImpactChecker impact_checker_;

  // The associated ExtensionPrefs. Guaranteed to be safe to use since this
  // class depends upon them via the KeyedService infrastructure.
  raw_ptr<ExtensionPrefs> extension_prefs_;

  // The associated BrowserContext. Guaranteed to be safe to use since this is
  // a KeyedService for the context.
  raw_ptr<content::BrowserContext> browser_context_;

  base::ScopedObservation<ExtensionRegistry, ExtensionRegistryObserver>
      registry_observation_{this};

  base::WeakPtrFactory<ManifestV2ExperimentManager> weak_factory_{this};
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_MANIFEST_V2_EXPERIMENT_MANAGER_H_
