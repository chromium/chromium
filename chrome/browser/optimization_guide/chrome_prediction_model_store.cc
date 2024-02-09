// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/optimization_guide/chrome_prediction_model_store.h"

#include "chrome/browser/browser_process.h"

namespace optimization_guide {

// static
ChromePredictionModelStore* ChromePredictionModelStore::GetInstance() {
  static base::NoDestructor<ChromePredictionModelStore> model_store;
  return model_store.get();
}

ChromePredictionModelStore::ChromePredictionModelStore() = default;
ChromePredictionModelStore::~ChromePredictionModelStore() = default;

PrefService* ChromePredictionModelStore::GetLocalState() const {
  return g_browser_process->local_state();
}

}  // namespace optimization_guide
