// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_OPTIMIZATION_GUIDE_CHROME_PREDICTION_MODEL_STORE_H_
#define CHROME_BROWSER_OPTIMIZATION_GUIDE_CHROME_PREDICTION_MODEL_STORE_H_

#include "components/optimization_guide/core/delivery/prediction_model_store.h"

class PrefService;

namespace optimization_guide {

class ChromePredictionModelStore : public PredictionModelStore {
 public:
  ChromePredictionModelStore();
  ~ChromePredictionModelStore() override;

  ChromePredictionModelStore(const ChromePredictionModelStore&) = delete;
  ChromePredictionModelStore& operator=(const ChromePredictionModelStore&) =
      delete;

  // optimization_guide::PredictionModelStore:
  PrefService* GetLocalState() const override;
};

}  // namespace optimization_guide

#endif  // CHROME_BROWSER_OPTIMIZATION_GUIDE_CHROME_PREDICTION_MODEL_STORE_H_
