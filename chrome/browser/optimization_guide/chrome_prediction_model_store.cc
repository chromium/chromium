// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/optimization_guide/chrome_prediction_model_store.h"

#include "base/files/file_path.h"
#include "base/path_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/chrome_paths.h"
#include "components/optimization_guide/core/optimization_guide_constants.h"

namespace optimization_guide {

ChromePredictionModelStore::ChromePredictionModelStore() {
  base::FilePath model_downloads_dir;
  base::PathService::Get(chrome::DIR_USER_DATA, &model_downloads_dir);
  model_downloads_dir = model_downloads_dir.Append(
      optimization_guide::kOptimizationGuideModelStoreDirPrefix);
  // Create and initialize the install-wide model store.
  Initialize(model_downloads_dir);
}

ChromePredictionModelStore::~ChromePredictionModelStore() = default;

PrefService* ChromePredictionModelStore::GetLocalState() const {
  return g_browser_process->local_state();
}

}  // namespace optimization_guide
