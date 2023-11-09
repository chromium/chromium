// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/optimization_guide/mock_optimization_guide_keyed_service.h"

#include "base/path_service.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/optimization_guide/core/prediction_model_store.h"
#include "components/prefs/testing_pref_service.h"

namespace {

// Prefix for the model store directory.
const base::FilePath::CharType kOptimizationGuideModelStoreDirPrefix[] =
    FILE_PATH_LITERAL("optimization_guide_model_store");

}  // namespace

// static
void MockOptimizationGuideKeyedService::Initialize(
    TestingPrefServiceSimple* local_state) {
  base::FilePath model_downloads_dir;
  base::PathService::Get(chrome::DIR_USER_DATA, &model_downloads_dir);
  model_downloads_dir =
      model_downloads_dir.Append(kOptimizationGuideModelStoreDirPrefix);
  // Create and initialize the install-wide model store.
  TestingBrowserProcess::GetGlobal()->SetLocalState(local_state);
  RegisterLocalState(local_state->registry());
  optimization_guide::PredictionModelStore::GetInstance()->Initialize(
      local_state, model_downloads_dir);
}

// static
void MockOptimizationGuideKeyedService::TearDown() {
  TestingBrowserProcess::GetGlobal()->SetLocalState(nullptr);
}

MockOptimizationGuideKeyedService::MockOptimizationGuideKeyedService(
    content::BrowserContext* browser_context)
    : OptimizationGuideKeyedService(browser_context) {}

MockOptimizationGuideKeyedService::~MockOptimizationGuideKeyedService() =
    default;
