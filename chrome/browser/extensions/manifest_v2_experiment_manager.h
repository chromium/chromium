// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_MANIFEST_V2_EXPERIMENT_MANAGER_H_
#define CHROME_BROWSER_EXTENSIONS_MANIFEST_V2_EXPERIMENT_MANAGER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/extensions/mv2_deprecation_impact_checker.h"
#include "components/keyed_service/core/keyed_service.h"
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
class ManifestV2ExperimentManager : public KeyedService {
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

 private:
  // The current stage of the MV2 deprecation experiments.
  const MV2ExperimentStage experiment_stage_;

  // A helper object to determine if a given extension is affected by the
  // MV2 deprecation experiments.
  MV2DeprecationImpactChecker impact_checker_;

  // The associated ExtensionPrefs. Guaranteed to be safe to use since this
  // class depends upon them via the KeyedService infrastructure.
  raw_ptr<ExtensionPrefs> extension_prefs_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_MANIFEST_V2_EXPERIMENT_MANAGER_H_
