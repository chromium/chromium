// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/optimization_guide/prediction/prediction_model_download_manager.h"

#include "base/bind.h"
#include "base/guid.h"
#include "chrome/browser/download/download_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_key.h"
#include "components/download/public/background_service/download_service.h"
#include "components/optimization_guide/optimization_guide_features.h"
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

}  // namespace

PredictionModelDownloadManager::PredictionModelDownloadManager(Profile* profile)
    : download_service_(
          DownloadServiceFactory::GetForKey(profile->GetProfileKey())),
      is_available_for_downloads_(true),
      api_key_(features::GetOptimizationGuideServiceAPIKey()) {}

PredictionModelDownloadManager::~PredictionModelDownloadManager() = default;

void PredictionModelDownloadManager::StartDownload(const GURL& download_url) {
  download::DownloadParams download_params;
  download_params.client =
      download::DownloadClient::OPTIMIZATION_GUIDE_PREDICTION_MODELS;
  download_params.guid = base::GenerateGUID();
  download_params.callback =
      base::BindRepeating(&PredictionModelDownloadManager::OnDownloadStarted,
                          weak_ptr_factory_.GetWeakPtr());
  download_params.traffic_annotation = net::MutableNetworkTrafficAnnotationTag(
      kOptimizationGuidePredictionModelsTrafficAnnotation);
  download_params.request_params.url = download_url;
  download_params.request_params.method = "GET";
  download_params.request_params.request_headers.SetHeader(kGoogApiKey,
                                                           api_key_);
  // TODO(crbug/1146151): Add feature params to control the scheduling params.
  download_params.scheduling_params.priority =
      download::SchedulingParams::Priority::NORMAL;
  download_params.scheduling_params.battery_requirements =
      download::SchedulingParams::BatteryRequirements::BATTERY_INSENSITIVE;
  download_params.scheduling_params.network_requirements =
      download::SchedulingParams::NetworkRequirements::OPTIMISTIC;

  download_service_->StartDownload(download_params);
}

void PredictionModelDownloadManager::CancelAllPendingDownloads() {
  for (const std::string& pending_download_guid : pending_download_guids_)
    download_service_->CancelDownload(pending_download_guid);
}

bool PredictionModelDownloadManager::IsAvailableForDownloads() const {
  return is_available_for_downloads_;
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

  // TODO(crbug/1146151): Log histogram.
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

  // TODO(crbug/1146151): Verify download.
}

void PredictionModelDownloadManager::OnDownloadFailed(const std::string& guid) {
  pending_download_guids_.erase(guid);

  // TODO(crbug/1146151): Log histogram.
}

}  // namespace optimization_guide
