// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/power/ml/smart_dim/smart_dim_worker.h"

#include "components/assist_ranker/proto/example_preprocessor.pb.h"

namespace ash {
namespace power {
namespace ml {

namespace {
using chromeos::machine_learning::mojom::FlatBufferModelSpec;
}  // namespace

SmartDimWorker::SmartDimWorker()
    : dim_threshold_(0.0), expected_feature_size_(0) {}

SmartDimWorker::~SmartDimWorker() = default;

double SmartDimWorker::dim_threshold() const {
  return dim_threshold_;
}

size_t SmartDimWorker::expected_feature_size() const {
  return expected_feature_size_;
}

void SmartDimWorker::Reset() {
  preprocessor_config_.reset();
  executor_.reset();
  model_.reset();
}

void SmartDimWorker::OnConnectionError() {
  DVLOG(1) << "Mojo connection for ML service is closed!";
  executor_.reset();
  model_.reset();
}

}  // namespace ml
}  // namespace power
}  // namespace ash
