// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/optimization_guide/chrome_browser_main_extra_parts_optimization_guide.h"

#include "base/files/file_path.h"
#include "base/path_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_paths.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/prediction_manager.h"
#include "components/optimization_guide/core/prediction_model_store.h"

namespace {

// Prefix for the model store directory.
const base::FilePath::CharType kOptimizationGuideModelStoreDirPrefix[] =
    FILE_PATH_LITERAL("optimization_guide_model_store");

}  // namespace

void ChromeBrowserMainExtraPartsOptimizationGuide::PreCreateThreads() {
  if (!optimization_guide::features::IsInstallWideModelStoreEnabled())
    return;

  base::FilePath model_downloads_dir;
  base::PathService::Get(chrome::DIR_USER_DATA, &model_downloads_dir);
  model_downloads_dir =
      model_downloads_dir.Append(kOptimizationGuideModelStoreDirPrefix);
  // Create and initialize the install-wide model store.
  optimization_guide::PredictionModelStore::GetInstance()->Initialize(
      g_browser_process->local_state(), model_downloads_dir);
}
