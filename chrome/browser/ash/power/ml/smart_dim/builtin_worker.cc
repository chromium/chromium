// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/power/ml/smart_dim/builtin_worker.h"

#include "ash/constants/ash_features.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ref_counted_memory.h"
#include "chrome/browser/ash/power/ml/smart_dim/ml_agent_util.h"
#include "chrome/grit/browser_resources.h"
#include "chromeos/services/machine_learning/public/cpp/service_connection.h"
#include "components/assist_ranker/proto/example_preprocessor.pb.h"
#include "ui/base/resource/resource_bundle.h"

namespace ash {
namespace power {
namespace ml {

namespace {

using chromeos::machine_learning::mojom::BuiltinModelId;
using chromeos::machine_learning::mojom::BuiltinModelSpec;
using chromeos::machine_learning::mojom::BuiltinModelSpecPtr;

constexpr size_t k20190521ModelInputVectorSize = 592;

constexpr double k20190521ModelDefaultDimThreshold = -0.5;

}  // namespace

BuiltinWorker::BuiltinWorker() : SmartDimWorker() {}

BuiltinWorker::~BuiltinWorker() = default;

const assist_ranker::ExamplePreprocessorConfig*
BuiltinWorker::GetPreprocessorConfig() {
  LazyInitialize();
  return preprocessor_config_.get();
}

const mojo::Remote<chromeos::machine_learning::mojom::GraphExecutor>&
BuiltinWorker::GetExecutor() {
  LazyInitialize();
  return executor_;
}

void BuiltinWorker::LazyInitialize() {
  // Initialize builtin meta info.
  dim_threshold_ = k20190521ModelDefaultDimThreshold;
  expected_feature_size_ = k20190521ModelInputVectorSize;

  if (!preprocessor_config_) {
    preprocessor_config_ =
        std::make_unique<assist_ranker::ExamplePreprocessorConfig>();

    const int resource_id =
        IDR_SMART_DIM_20190521_EXAMPLE_PREPROCESSOR_CONFIG_PB;

    const scoped_refptr<base::RefCountedMemory> raw_config =
        ui::ResourceBundle::GetSharedInstance().LoadDataResourceBytes(
            resource_id);
    if (!raw_config || !raw_config->front()) {
      DLOG(FATAL)
          << "Failed to load Builtin SmartDimModel example preprocessor "
             "config.";
      return;
    }

    if (!preprocessor_config_->ParseFromArray(raw_config->front(),
                                              raw_config->size())) {
      DLOG(FATAL) << "Failed to parse Builtin SmartDimModel example "
                     "preprocessor config.";
      preprocessor_config_.reset();
      return;
    }
  }

  if (!model_) {
    // Load the model.
    BuiltinModelSpecPtr spec =
        BuiltinModelSpec::New(BuiltinModelId::SMART_DIM_20190521);
    // Builtin model is supposed to be always available and valid, using
    // base::DoNothing as callbacks.
    chromeos::machine_learning::ServiceConnection::GetInstance()
        ->GetMachineLearningService()
        .LoadBuiltinModel(std::move(spec), model_.BindNewPipeAndPassReceiver(),
                          base::DoNothing());
  }

  if (!executor_) {
    // Get the graph executor.
    model_->CreateGraphExecutor(
        chromeos::machine_learning::mojom::GraphExecutorOptions::New(),
        executor_.BindNewPipeAndPassReceiver(), base::DoNothing());
    executor_.set_disconnect_handler(base::BindOnce(
        &BuiltinWorker::OnConnectionError, base::Unretained(this)));
  }
}

}  // namespace ml
}  // namespace power
}  // namespace ash
