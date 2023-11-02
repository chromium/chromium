// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POWER_ML_SMART_DIM_SMART_DIM_WORKER_H_
#define CHROME_BROWSER_ASH_POWER_ML_SMART_DIM_SMART_DIM_WORKER_H_

#include <memory>

#include "chromeos/services/machine_learning/public/mojom/graph_executor.mojom.h"
#include "chromeos/services/machine_learning/public/mojom/machine_learning_service.mojom.h"
#include "chromeos/services/machine_learning/public/mojom/model.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace assist_ranker {
class ExamplePreprocessorConfig;
}  // namespace assist_ranker

namespace ash {
namespace power {
namespace ml {

// This is the common interface & base class that holds the preprocessor, model
// graph executor and other essential fields like dim_threshold,
// expected_feature_size for SmartDimMlAgent to make decision.
// It has two child classes, BuiltinWorker and DownloadWorker, both are members
// of SmartDimMlAgent and are created when SmartDimWorker is created.
// DownloadWorker is not ready until InitializeFromComponent is successfully
// called by component update service, while BuiltinWorker can be lazily
// initialized when GetPreprocessorConfig() or GetExecutor() are called.
class SmartDimWorker {
 public:
  SmartDimWorker();

  SmartDimWorker(const SmartDimWorker&) = delete;
  SmartDimWorker& operator=(const SmartDimWorker&) = delete;

  virtual ~SmartDimWorker();

  // Gets model score threshold above which the screen dim is recommended.
  double dim_threshold() const;
  // Gets expected feature size.
  size_t expected_feature_size() const;

  // Returns a preprocessor_config for SmartDimMlAgent to convert feature proto
  // to a vector.
  virtual const assist_ranker::ExamplePreprocessorConfig*
  GetPreprocessorConfig() = 0;
  // Returns a mojo remote of ML service GraphExecutor to make inference.
  virtual const mojo::Remote<chromeos::machine_learning::mojom::GraphExecutor>&
  GetExecutor() = 0;

  // Release the members on connection error, or when download_worker_ is ready
  // we can reset the builtin_worker_ to save memory.
  // Note: subclasses might reconnect automatically, e.g. if ml_agent decides to
  // use builtin_worker_ after it is Reset, LazyInitialize will reconnect it.
  virtual void Reset();

 protected:
  void OnConnectionError();

  double dim_threshold_;
  size_t expected_feature_size_;
  std::unique_ptr<assist_ranker::ExamplePreprocessorConfig>
      preprocessor_config_;

  // Remotes used to execute functions in the ML service side.
  mojo::Remote<chromeos::machine_learning::mojom::Model> model_;
  mojo::Remote<chromeos::machine_learning::mojom::GraphExecutor> executor_;
};

}  // namespace ml
}  // namespace power
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_POWER_ML_SMART_DIM_SMART_DIM_WORKER_H_
