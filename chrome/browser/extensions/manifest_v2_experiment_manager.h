// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_MANIFEST_V2_EXPERIMENT_MANAGER_H_
#define CHROME_BROWSER_EXTENSIONS_MANIFEST_V2_EXPERIMENT_MANAGER_H_

#include "components/keyed_service/core/keyed_service.h"
#include "extensions/common/extension_id.h"

class BrowserContextKeyedServiceFactory;

namespace content {
class BrowserContext;
}  // namespace content

namespace extensions {
enum class MV2ExperimentStage;

// The central class responsible for managing experiments related to the MV2
// deprecation.
class ManifestV2ExperimentManager : public KeyedService {
 public:
  ManifestV2ExperimentManager();
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
  // should only use this for determining the experiment stage itself.
  MV2ExperimentStage GetCurrentExperimentStage();
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_MANIFEST_V2_EXPERIMENT_MANAGER_H_
