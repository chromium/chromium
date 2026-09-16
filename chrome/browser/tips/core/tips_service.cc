// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tips/core/tips_service.h"

#include <algorithm>
#include <map>
#include <optional>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "components/prefs/pref_service.h"
#include "components/segmentation_platform/internal/metadata/metadata_writer.h"
#include "components/segmentation_platform/public/constants.h"
#include "components/segmentation_platform/public/database_client.h"
#include "components/segmentation_platform/public/model_provider.h"
#include "components/segmentation_platform/public/proto/model_metadata.pb.h"
#include "components/segmentation_platform/public/segmentation_platform_service.h"

namespace tips {

namespace {

// Converts a SignalDefinition into a feature signal recognized by the
// segmentation platform, such as a UserAction or UMA histogram.
segmentation_platform::features::Feature SignalToFeature(
    const SignalDefinition& signal) {
  switch (signal.type) {
    case SignalType::kUserAction:
      return segmentation_platform::features::UserAction(signal.name.c_str(),
                                                         signal.days);
    case SignalType::kHistogramSum:
      return segmentation_platform::features::UMASum(signal.name.c_str(),
                                                     signal.days);
    case SignalType::kHistogramEnum:
      return segmentation_platform::features::UMAEnum(
          signal.name.c_str(), signal.days, signal.enum_values);
  }
}

}  // namespace

TipsService::TipsService(
    PrefService* pref_service,
    segmentation_platform::SegmentationPlatformService* segmentation_service)
    : pref_service_(pref_service),
      segmentation_service_(segmentation_service) {}

TipsService::~TipsService() = default;

void TipsService::RegisterFeature(std::unique_ptr<TipsFeature> feature) {
  registered_features_.push_back(std::move(feature));
}

void TipsService::DetermineBestTip(OnBestTipChosen callback) {
  if (!segmentation_service_ || registered_features_.empty()) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  // TODO(crbug.com/559726350): Signal registration in the Segmentation
  // Platform currently relies on TipsNotificationsRanker being registered in
  // segmentation_platform_config.cc. If TipsNotificationsRanker is deprecated,
  // a new tips_config file will need to be created and registered so that UMA
  // signals continue to be monitored.
  auto* db_client = segmentation_service_->GetDatabaseClient();
  if (!db_client) {
    // Platform database client is not initialized yet.
    std::move(callback).Run(std::nullopt);
    return;
  }

  segmentation_platform::proto::SegmentationModelMetadata metadata;
  segmentation_platform::MetadataWriter writer(&metadata);
  // Min signal collection length 0, 28 days TTL.
  writer.SetDefaultSegmentationMetadataConfig(0, 28);

  // Add feature signals to the metadata writer. The features are added
  // sequentially and in OnFeaturesProcessed, will be returned as inputs in the
  // same sequential order that they were added.
  for (const std::unique_ptr<TipsFeature>& feature : registered_features_) {
    for (const SignalDefinition& signal : feature->GetRequiredSignals()) {
      writer.AddFeatures({SignalToFeature(signal)});
    }
  }

  db_client->ProcessFeatures(
      metadata, base::Time::Now(),
      base::BindOnce(&TipsService::OnFeaturesProcessed,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void TipsService::OnFeaturesProcessed(
    OnBestTipChosen callback,
    ResultStatus status,
    const segmentation_platform::ModelProvider::Request& inputs) {
  if (status != ResultStatus::kSuccess) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  // Map the flat inputs vector back to the corresponding feature signals. An
  // outer map is used to map each feature to an inner map, which records each
  // signal name in reference to its emitted value. Inputs are returned in the
  // same order as they were added sequentially above.
  std::map<TipsFeature*, std::map<std::string, float>> feature_signals_map;
  size_t input_index = 0;

  for (const std::unique_ptr<TipsFeature>& feature : registered_features_) {
    for (const SignalDefinition& signal : feature->GetRequiredSignals()) {
      if (input_index < inputs.size()) {
        feature_signals_map[feature.get()][signal.name] = inputs[input_index];
        input_index++;
      }
    }
  }

  std::optional<TipsNotificationsFeatureType> best_tip;
  std::optional<TipFeatureRank> best_rank;

  // Evaluate eligibility for each registered feature and apply the centralized
  // global ranking order.
  for (const std::unique_ptr<TipsFeature>& feature : registered_features_) {
    auto it = feature_signals_map.find(feature.get());
    if (it != feature_signals_map.end()) {
      if (feature->IsEligible(it->second, *pref_service_)) {
        TipFeatureRank rank = feature->GetRank();
        if (!best_rank ||
            static_cast<int>(rank) < static_cast<int>(*best_rank)) {
          best_rank = rank;
          best_tip = feature->GetFeatureType();
        }
      }
    }
  }

  std::move(callback).Run(best_tip);
}

}  // namespace tips
