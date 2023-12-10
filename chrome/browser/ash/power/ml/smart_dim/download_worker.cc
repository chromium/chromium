// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/power/ml/smart_dim/download_worker.h"

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/task/task_traits.h"
#include "chrome/browser/ash/power/ml/smart_dim/metrics.h"
#include "chrome/browser/ash/power/ml/smart_dim/ml_agent_util.h"
#include "chromeos/services/machine_learning/public/cpp/service_connection.h"
#include "components/assist_ranker/proto/example_preprocessor.pb.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "ui/base/resource/resource_bundle.h"

namespace ash {
namespace power {
namespace ml {

namespace {
using chromeos::machine_learning::mojom::FlatBufferModelSpec;
}  // namespace

DownloadWorker::DownloadWorker() : SmartDimWorker(), metrics_model_name_("") {}

DownloadWorker::~DownloadWorker() = default;

const assist_ranker::ExamplePreprocessorConfig*
DownloadWorker::GetPreprocessorConfig() {
  return preprocessor_config_.get();
}

const mojo::Remote<chromeos::machine_learning::mojom::GraphExecutor>&
DownloadWorker::GetExecutor() {
  return executor_;
}

void DownloadWorker::LoadModelCallback(
    chromeos::machine_learning::mojom::LoadModelResult result) {
  if (result != chromeos::machine_learning::mojom::LoadModelResult::OK) {
    LogLoadComponentEvent(LoadComponentEvent::kLoadModelError);
    DVLOG(1) << "Failed to load Smart Dim flatbuffer model.";
  }
}

void DownloadWorker::CreateGraphExecutorCallback(
    chromeos::machine_learning::mojom::CreateGraphExecutorResult result) {
  if (result !=
      chromeos::machine_learning::mojom::CreateGraphExecutorResult::OK) {
    LogLoadComponentEvent(LoadComponentEvent::kCreateGraphExecutorError);
    DVLOG(1) << "Failed to create a Smart Dim graph executor.";
  } else {
    LogLoadComponentEvent(LoadComponentEvent::kSuccess);
  }
}

bool DownloadWorker::IsReady() {
  return preprocessor_config_ && model_ && executor_ &&
         expected_feature_size_ > 0 && metrics_model_name_ != "";
}

void DownloadWorker::InitializeFromComponent(
    const ComponentFileContents& contents) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  auto [metadata_json, preprocessor_proto, model_flatbuffer] = contents;

  preprocessor_config_ =
      std::make_unique<assist_ranker::ExamplePreprocessorConfig>();
  if (!preprocessor_config_->ParseFromString(preprocessor_proto)) {
    LogLoadComponentEvent(LoadComponentEvent::kLoadPreprocessorError);
    DVLOG(1) << "Failed to load preprocessor_config.";
    preprocessor_config_.reset();
    return;
  }

  // Meta data contains necessary info to construct FlatBufferModelSpec, and
  // other optional info.
  data_decoder::DataDecoder::ParseJsonIsolated(
      std::move(metadata_json),
      base::BindOnce(&DownloadWorker::OnJsonParsed, base::Unretained(this),
                     std::move(model_flatbuffer)));
}

void DownloadWorker::SetOnReadyForTest(base::OnceClosure on_ready) {
  on_ready_for_test_ = std::move(on_ready);
}

void DownloadWorker::OnJsonParsed(
    const std::string& model_flatbuffer,
    const data_decoder::DataDecoder::ValueOrError result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!result.has_value() || !result->is_dict() ||
      !ParseMetaInfoFromJsonObject(*result, &metrics_model_name_,
                                   &dim_threshold_, &expected_feature_size_,
                                   &inputs_, &outputs_)) {
    LogLoadComponentEvent(LoadComponentEvent::kLoadMetadataError);
    DVLOG(1) << "Failed to parse meta info from metadata_json.";
    return;
  }
  content::GetUIThreadTaskRunner({base::TaskPriority::BEST_EFFORT})
      ->PostTask(
          FROM_HERE,
          base::BindOnce(&DownloadWorker::LoadModelAndCreateGraphExecutor,
                         base::Unretained(this), std::move(model_flatbuffer)));
}

void DownloadWorker::LoadModelAndCreateGraphExecutor(
    const std::string& model_flatbuffer) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(!model_.is_bound() && !executor_.is_bound());

  chromeos::machine_learning::ServiceConnection::GetInstance()
      ->GetMachineLearningService()
      .LoadFlatBufferModel(
          FlatBufferModelSpec::New(std::move(model_flatbuffer), inputs_,
                                   outputs_, metrics_model_name_),
          model_.BindNewPipeAndPassReceiver(),
          base::BindOnce(&DownloadWorker::LoadModelCallback,
                         base::Unretained(this)));
  model_->CreateGraphExecutor(
      chromeos::machine_learning::mojom::GraphExecutorOptions::New(),
      executor_.BindNewPipeAndPassReceiver(),
      base::BindOnce(&DownloadWorker::CreateGraphExecutorCallback,
                     base::Unretained(this)));
  executor_.set_disconnect_handler(base::BindOnce(
      &DownloadWorker::OnConnectionError, base::Unretained(this)));
  if (on_ready_for_test_) {
    std::move(on_ready_for_test_).Run();
  }
}

}  // namespace ml
}  // namespace power
}  // namespace ash
