// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/optimization_guide/mock_optimization_guide_keyed_service.h"

#include "base/path_service.h"
#include "chrome/browser/optimization_guide/chrome_prediction_model_store.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/optimization_guide/core/optimization_guide_constants.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/prefs/testing_pref_service.h"

// static
void MockOptimizationGuideKeyedService::Initialize(
    TestingPrefServiceSimple* local_state) {
  TestingBrowserProcess::GetGlobal()->SetLocalState(local_state);
  RegisterLocalState(local_state->registry());
  InitializeWithExistingTestLocalState();
}

// static
void MockOptimizationGuideKeyedService::InitializeWithExistingTestLocalState() {
  // Create and initialize the install-wide model store.
  base::FilePath model_downloads_dir;
  base::PathService::Get(chrome::DIR_USER_DATA, &model_downloads_dir);
  model_downloads_dir = model_downloads_dir.Append(
      optimization_guide::kOptimizationGuideModelStoreDirPrefix);
  optimization_guide::ChromePredictionModelStore::GetInstance()->Initialize(
      model_downloads_dir);
}

// static
void MockOptimizationGuideKeyedService::TearDown() {
  ResetForTesting();
  TestingBrowserProcess::GetGlobal()->SetLocalState(nullptr);
}

// static
void MockOptimizationGuideKeyedService::ResetForTesting() {
  // Reinitialize the store, so that tests do not use state from the
  // previous test.
  optimization_guide::ChromePredictionModelStore::GetInstance()
      ->ResetForTesting();
}

MockOptimizationGuideKeyedService::MockOptimizationGuideKeyedService()
    : OptimizationGuideKeyedService(nullptr) {}

MockOptimizationGuideKeyedService::~MockOptimizationGuideKeyedService() =
    default;

void MockOptimizationGuideKeyedService::Shutdown() {}
