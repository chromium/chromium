// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/optimization_guide/chrome_browser_main_extra_parts_optimization_guide.h"

#include "base/files/file_path.h"
#include "base/path_service.h"
#include "chrome/browser/optimization_guide/chrome_prediction_model_store.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/common/chrome_paths.h"
#include "components/optimization_guide/core/optimization_guide_constants.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/prediction_manager.h"

void ChromeBrowserMainExtraPartsOptimizationGuide::PreCreateThreads() {
  base::FilePath model_downloads_dir;
  base::PathService::Get(chrome::DIR_USER_DATA, &model_downloads_dir);
  model_downloads_dir = model_downloads_dir.Append(
      optimization_guide::kOptimizationGuideModelStoreDirPrefix);
  // Create and initialize the install-wide model store.
  optimization_guide::ChromePredictionModelStore::GetInstance()->Initialize(
      model_downloads_dir);
}
