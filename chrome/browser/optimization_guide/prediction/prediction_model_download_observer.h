// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_OPTIMIZATION_GUIDE_PREDICTION_PREDICTION_MODEL_DOWNLOAD_OBSERVER_H_
#define CHROME_BROWSER_OPTIMIZATION_GUIDE_PREDICTION_PREDICTION_MODEL_DOWNLOAD_OBSERVER_H_

#include "base/observer_list_types.h"
#include "components/optimization_guide/proto/models.pb.h"

namespace optimization_guide {

// Observes the PredictionModelDownloadManager for downloads that have
// completed.
class PredictionModelDownloadObserver : public base::CheckedObserver {
 public:
  // Invoked when a model has been downloaded and verified.
  virtual void OnModelReady(const proto::PredictionModel& model) = 0;
};

}  // namespace optimization_guide

#endif  // CHROME_BROWSER_OPTIMIZATION_GUIDE_PREDICTION_PREDICTION_MODEL_DOWNLOAD_OBSERVER_H_
