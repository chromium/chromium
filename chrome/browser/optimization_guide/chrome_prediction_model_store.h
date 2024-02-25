// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_OPTIMIZATION_GUIDE_CHROME_PREDICTION_MODEL_STORE_H_
#define CHROME_BROWSER_OPTIMIZATION_GUIDE_CHROME_PREDICTION_MODEL_STORE_H_

#include "base/no_destructor.h"
#include "components/optimization_guide/core/prediction_model_store.h"

class PrefService;

namespace optimization_guide {

class ChromePredictionModelStore : public PredictionModelStore {
 public:
  // Returns the singleton model store.
  static ChromePredictionModelStore* GetInstance();

  ChromePredictionModelStore();
  ~ChromePredictionModelStore() override;

  ChromePredictionModelStore(const ChromePredictionModelStore&) = delete;
  ChromePredictionModelStore& operator=(const ChromePredictionModelStore&) =
      delete;

  // optimization_guide::PredictionModelStore:
  PrefService* GetLocalState() const override;

 private:
  friend base::NoDestructor<ChromePredictionModelStore>;
};

}  // namespace optimization_guide

#endif  // CHROME_BROWSER_OPTIMIZATION_GUIDE_CHROME_PREDICTION_MODEL_STORE_H_
