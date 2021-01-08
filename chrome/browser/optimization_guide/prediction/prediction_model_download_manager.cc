// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/optimization_guide/prediction/prediction_model_download_manager.h"

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/guid.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"
#include "chrome/browser/optimization_guide/prediction/prediction_model_download_observer.h"
#include "components/crx_file/crx_verifier.h"
#include "components/download/public/background_service/download_service.h"
#include "components/optimization_guide/core/optimization_guide_enums.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_switches.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/services/unzip/content/unzip_service.h"
#include "components/services/unzip/public/cpp/unzip.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

namespace optimization_guide {

namespace {

// Header for API key.
constexpr char kGoogApiKey[] = "X-Goog-Api-Key";

const net::NetworkTrafficAnnotationTag
    kOptimizationGuidePredictionModelsTrafficAnnotation =
        net::DefineNetworkTrafficAnnotation("optimization_guide_model_download",
                                            R"(
        semantics {
          sender: "Optimization Guide"
          description:
            "Chromium interacts with Optimization Guide Service to download "
            "non-personalized models used to improve browser behavior around "
            "page load performance and features such as Translate."
          trigger:
            "When there are new models to download based on response from "
            "Optimization Guide Service that is triggered daily."
          data: "The URL provided by the Optimization Guide Service to fetch "
            "an updated model. No user information is sent."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting:
            "This request cannot be disabled in settings. However it will "
            "never be made if the "
            "'OptimizationGuideModelDownloading' feature is disabled."
          policy_exception_justification: "Not yet implemented."
        })");

const base::FilePath::CharType kModelInfoFileName[] =
    FILE_PATH_LITERAL("model-info.pb");
const base::FilePath::CharType kModelFileName[] =
    FILE_PATH_LITERAL("model.tflite");

bool IsRelevantFile(const base::FilePath& file_path) {
  base::FilePath::StringType base_name_value = file_path.BaseName().value();
  return base_name_value == kModelFileName ||
         base_name_value == kModelInfoFileName;
}

base::FilePath GetFilePathForModelInfo(const base::FilePath& dir,
                                       const proto::ModelInfo& model_info) {
  return dir.AppendASCII(base::StringPrintf(
      "%s_%s.tflite",
      proto::OptimizationTarget_Name(model_info.optimization_target()).c_str(),
      base::NumberToString(model_info.version()).c_str()));
}

void RecordPredictionModelDownloadStatus(PredictionModelDownloadStatus status) {
  base::UmaHistogramEnumeration(
      "OptimizationGuide.PredictionModelDownloadManager."
      "DownloadStatus",
      status);
}

}  // namespace

PredictionModelDownloadManager::PredictionModelDownloadManager(
    download::DownloadService* download_service,
    const base::FilePath& models_dir,
    scoped_refptr<base::SequencedTaskRunner> background_task_runner)
    : download_service_(download_service),
      models_dir_(models_dir),
      is_available_for_downloads_(true),
      api_key_(features::GetOptimizationGuideServiceAPIKey()),
      background_task_runner_(background_task_runner) {}

PredictionModelDownloadManager::~PredictionModelDownloadManager() = default;

void PredictionModelDownloadManager::StartDownload(const GURL& download_url) {
  download::DownloadParams download_params;
  download_params.client =
      download::DownloadClient::OPTIMIZATION_GUIDE_PREDICTION_MODELS;
  download_params.guid = base::GenerateGUID();
  download_params.callback =
      base::BindRepeating(&PredictionModelDownloadManager::OnDownloadStarted,
                          ui_weak_ptr_factory_.GetWeakPtr());
  download_params.traffic_annotation = net::MutableNetworkTrafficAnnotationTag(
      kOptimizationGuidePredictionModelsTrafficAnnotation);
  download_params.request_params.url = download_url;
  download_params.request_params.method = "GET";
  download_params.request_params.request_headers.SetHeader(kGoogApiKey,
                                                           api_key_);
  if (features::IsUnrestrictedModelDownloadingEnabled()) {
    // This feature param should really only be used for testing, so it is ok
    // to have this be a high priority download with no network restrictions.
    download_params.scheduling_params.priority =
        download::SchedulingParams::Priority::HIGH;
    download_params.scheduling_params.battery_requirements =
        download::SchedulingParams::BatteryRequirements::BATTERY_INSENSITIVE;
    download_params.scheduling_params.network_requirements =
        download::SchedulingParams::NetworkRequirements::NONE;
  } else {
    download_params.scheduling_params.priority =
        download::SchedulingParams::Priority::NORMAL;
    download_params.scheduling_params.battery_requirements =
        download::SchedulingParams::BatteryRequirements::BATTERY_INSENSITIVE;
    download_params.scheduling_params.network_requirements =
        download::SchedulingParams::NetworkRequirements::OPTIMISTIC;
  }

  download_service_->StartDownload(download_params);
}

void PredictionModelDownloadManager::CancelAllPendingDownloads() {
  for (const std::string& pending_download_guid : pending_download_guids_)
    download_service_->CancelDownload(pending_download_guid);
}

bool PredictionModelDownloadManager::IsAvailableForDownloads() const {
  return is_available_for_downloads_;
}

void PredictionModelDownloadManager::AddObserver(
    PredictionModelDownloadObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  observers_.AddObserver(observer);
}

void PredictionModelDownloadManager::RemoveObserver(
    PredictionModelDownloadObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  observers_.RemoveObserver(observer);
}

void PredictionModelDownloadManager::OnDownloadServiceReady(
    const std::set<std::string>& pending_download_guids,
    const std::map<std::string, base::FilePath>& successful_downloads) {
  for (const std::string& pending_download_guid : pending_download_guids)
    pending_download_guids_.insert(pending_download_guid);

  for (const auto& successful_download : successful_downloads)
    OnDownloadSucceeded(successful_download.first, successful_download.second);
}

void PredictionModelDownloadManager::OnDownloadServiceUnavailable() {
  is_available_for_downloads_ = false;
}

void PredictionModelDownloadManager::OnDownloadStarted(
    const std::string& guid,
    download::DownloadParams::StartResult start_result) {
  if (start_result == download::DownloadParams::StartResult::ACCEPTED)
    pending_download_guids_.insert(guid);
}

void PredictionModelDownloadManager::OnDownloadSucceeded(
    const std::string& guid,
    const base::FilePath& file_path) {
  pending_download_guids_.erase(guid);

  base::UmaHistogramBoolean(
      "OptimizationGuide.PredictionModelDownloadManager.DownloadSucceeded",
      true);

  background_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&PredictionModelDownloadManager::ProcessDownload,
                     base::Unretained(this), file_path),
      base::BindOnce(&PredictionModelDownloadManager::StartUnzipping,
                     ui_weak_ptr_factory_.GetWeakPtr()));
}

void PredictionModelDownloadManager::OnDownloadFailed(const std::string& guid) {
  pending_download_guids_.erase(guid);

  base::UmaHistogramBoolean(
      "OptimizationGuide.PredictionModelDownloadManager.DownloadSucceeded",
      false);
}

base::Optional<std::pair<base::FilePath, base::FilePath>>
PredictionModelDownloadManager::ProcessDownload(
    const base::FilePath& file_path) {
  DCHECK(background_task_runner_->RunsTasksInCurrentSequence());

  if (!switches::ShouldSkipModelDownloadVerificationForTesting()) {
    // Verify that the |file_path| contains a file signed with a key we trust.
    crx_file::VerifierResult verifier_result = crx_file::Verify(
        file_path, crx_file::VerifierFormat::CRX3_WITH_PUBLISHER_PROOF,
        /*required_key_hashes=*/{},
        /*required_file_hash=*/{}, /*public_key=*/nullptr,
        /*crx_id=*/nullptr);
    if (verifier_result != crx_file::VerifierResult::OK_FULL) {
      RecordPredictionModelDownloadStatus(
          PredictionModelDownloadStatus::kFailedCrxVerification);
      base::ThreadPool::PostTask(
          FROM_HERE, {base::TaskPriority::BEST_EFFORT, base::MayBlock()},
          base::BindOnce(base::GetDeleteFileCallback(), file_path));
      return base::nullopt;
    }
  }

  // Unzip download.
  base::FilePath temp_dir_path;
  if (!base::CreateNewTempDirectory(base::FilePath::StringType(),
                                    &temp_dir_path)) {
    RecordPredictionModelDownloadStatus(
        PredictionModelDownloadStatus::kFailedUnzipDirectoryCreation);
    base::ThreadPool::PostTask(
        FROM_HERE, {base::TaskPriority::BEST_EFFORT, base::MayBlock()},
        base::BindOnce(base::GetDeleteFileCallback(), file_path));
    return base::nullopt;
  }

  return std::make_pair(file_path, temp_dir_path);
}

void PredictionModelDownloadManager::StartUnzipping(
    const base::Optional<std::pair<base::FilePath, base::FilePath>>&
        unzip_paths) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!unzip_paths)
    return;

  unzip::UnzipWithFilter(
      unzip::LaunchUnzipper(), unzip_paths->first, unzip_paths->second,
      base::BindRepeating(&IsRelevantFile),
      base::BindOnce(&PredictionModelDownloadManager::OnDownloadUnzipped,
                     ui_weak_ptr_factory_.GetWeakPtr(), unzip_paths->first,
                     unzip_paths->second));
}

void PredictionModelDownloadManager::OnDownloadUnzipped(
    const base::FilePath& original_file_path,
    const base::FilePath& unzipped_dir_path,
    bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Clean up original download file when this function finishes.
  background_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(base::GetDeleteFileCallback(), original_file_path));

  if (!success) {
    RecordPredictionModelDownloadStatus(
        PredictionModelDownloadStatus::kFailedCrxUnzip);
    return;
  }

  background_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&PredictionModelDownloadManager::ProcessUnzippedContents,
                     base::Unretained(this), unzipped_dir_path),
      base::BindOnce(&PredictionModelDownloadManager::NotifyModelReady,
                     ui_weak_ptr_factory_.GetWeakPtr()));
}

base::Optional<proto::PredictionModel>
PredictionModelDownloadManager::ProcessUnzippedContents(
    const base::FilePath& unzipped_dir_path) {
  DCHECK(background_task_runner_->RunsTasksInCurrentSequence());

  // Clean up temp dir when this function finishes.
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(base::GetDeletePathRecursivelyCallback(),
                                unzipped_dir_path));

  // Unpack and verify model info file.
  base::FilePath model_info_path = unzipped_dir_path.Append(kModelInfoFileName);
  std::string binary_model_info_pb;
  if (!base::ReadFileToString(model_info_path, &binary_model_info_pb)) {
    RecordPredictionModelDownloadStatus(
        PredictionModelDownloadStatus::kFailedModelInfoFileRead);
    return base::nullopt;
  }
  proto::ModelInfo model_info;
  if (!model_info.ParseFromString(binary_model_info_pb)) {
    RecordPredictionModelDownloadStatus(
        PredictionModelDownloadStatus::kFailedModelInfoParsing);
    return base::nullopt;
  }
  if (!model_info.has_version() || !model_info.has_optimization_target()) {
    RecordPredictionModelDownloadStatus(
        PredictionModelDownloadStatus::kFailedModelInfoInvalid);
    return base::nullopt;
  }

  // Move model file away from temp directory.
  base::FilePath temp_model_path = unzipped_dir_path.Append(kModelFileName);
  base::FilePath model_path = GetFilePathForModelInfo(models_dir_, model_info);
  base::File::Error file_error;
  if (!base::ReplaceFile(temp_model_path, model_path, &file_error)) {
    if (file_error == base::File::FILE_ERROR_NOT_FOUND) {
      RecordPredictionModelDownloadStatus(
          PredictionModelDownloadStatus::kFailedModelFileNotFound);
    } else {
      RecordPredictionModelDownloadStatus(
          PredictionModelDownloadStatus::kFailedModelFileOtherError);
    }
    return base::nullopt;
  }

  RecordPredictionModelDownloadStatus(PredictionModelDownloadStatus::kSuccess);

  proto::PredictionModel model;
  *model.mutable_model_info() = model_info;
  SetFilePathInPredictionModel(model_path, &model);
  return model;
}

void PredictionModelDownloadManager::NotifyModelReady(
    const base::Optional<proto::PredictionModel>& model) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!model)
    return;

  for (PredictionModelDownloadObserver& observer : observers_)
    observer.OnModelReady(*model);
}

}  // namespace optimization_guide
