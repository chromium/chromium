// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/segmentation_platform/segmentation_platform_config.h"

#include <memory>

#include "base/containers/cxx20_erase_vector.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "build/build_config.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "components/segmentation_platform/embedder/default_model/cross_device_user_segment.h"
#include "components/segmentation_platform/embedder/default_model/device_switcher_model.h"
#include "components/segmentation_platform/embedder/default_model/feed_user_segment.h"
#include "components/segmentation_platform/embedder/default_model/frequent_feature_user_model.h"
#include "components/segmentation_platform/embedder/default_model/low_user_engagement_model.h"
#include "components/segmentation_platform/embedder/default_model/resume_heavy_user_model.h"
#include "components/segmentation_platform/embedder/default_model/search_user_model.h"
#include "components/segmentation_platform/embedder/default_model/shopping_user_model.h"
#include "components/segmentation_platform/internal/config_parser.h"
#include "components/segmentation_platform/public/config.h"
#include "components/segmentation_platform/public/constants.h"
#include "components/segmentation_platform/public/features.h"
#include "components/segmentation_platform/public/proto/segmentation_platform.pb.h"
#include "content/public/browser/browser_context.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/commerce/shopping_service_factory.h"
#include "chrome/browser/feature_guide/notifications/feature_notification_guide_service.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "chrome/browser/segmentation_platform/default_model/chrome_start_model_android.h"
#include "chrome/browser/segmentation_platform/default_model/chrome_start_model_android_v2.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/commerce/core/shopping_service.h"
#include "components/segmentation_platform/embedder/default_model/contextual_page_actions_model.h"
#include "components/segmentation_platform/embedder/default_model/intentional_user_model.h"
#include "components/segmentation_platform/embedder/default_model/power_user_segment.h"
#include "components/segmentation_platform/embedder/default_model/query_tiles_model.h"
#endif

namespace segmentation_platform {

using proto::SegmentId;

namespace {

#if BUILDFLAG(IS_ANDROID)

constexpr int kAdaptiveToolbarDefaultSelectionTTLDays = 56;

#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_ANDROID)
std::unique_ptr<Config> GetConfigForAdaptiveToolbar() {
  auto config = std::make_unique<Config>();
  config->segmentation_key = kAdaptiveToolbarSegmentationKey;
  config->segmentation_uma_name = kAdaptiveToolbarUmaName;

  if (base::FeatureList::IsEnabled(
          segmentation_platform::features::
              kSegmentationPlatformAdaptiveToolbarV2Feature)) {
    config->AddSegmentId(
        SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_ADAPTIVE_TOOLBAR);
  } else {
    int segment_selection_ttl_days = base::GetFieldTrialParamByFeatureAsInt(
        chrome::android::kAdaptiveButtonInTopToolbarCustomizationV2,
        kVariationsParamNameSegmentSelectionTTLDays,
        kAdaptiveToolbarDefaultSelectionTTLDays);
    config->segment_selection_ttl = base::Days(segment_selection_ttl_days);
    // Do not set unknown TTL so that the platform ignores unknown results.

    // A hardcoded list of segment IDs known to the segmentation platform.
    config->AddSegmentId(SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB);
    config->AddSegmentId(SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_SHARE);
    config->AddSegmentId(SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_VOICE);
  }

  return config;
}
#endif

#if BUILDFLAG(IS_ANDROID)
bool IsEnabledContextualPageActions() {
  if (!base::FeatureList::IsEnabled(features::kContextualPageActions))
    return false;

  bool is_price_tracking_enabled = base::FeatureList::IsEnabled(
      features::kContextualPageActionPriceTracking);

  bool is_reader_mode_enabled =
      base::FeatureList::IsEnabled(features::kContextualPageActionReaderMode);

  return is_price_tracking_enabled || is_reader_mode_enabled;
}

std::unique_ptr<Config> GetConfigForContextualPageActions(
    content::BrowserContext* context) {
  auto config = std::make_unique<Config>();
  config->segmentation_key = kContextualPageActionsKey;
  config->segmentation_uma_name = kContextualPageActionsUmaName;
  config->AddSegmentId(
      SegmentId::OPTIMIZATION_TARGET_CONTEXTUAL_PAGE_ACTION_PRICE_TRACKING,
      std::make_unique<ContextualPageActionsModel>());
  config->on_demand_execution = true;
  return config;
}

#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace

std::vector<std::unique_ptr<Config>> GetSegmentationPlatformConfig(
    content::BrowserContext* context) {
  std::vector<std::unique_ptr<Config>> configs;
#if BUILDFLAG(IS_ANDROID)
  if (base::FeatureList::IsEnabled(
          chrome::android::kAdaptiveButtonInTopToolbarCustomizationV2)) {
    configs.emplace_back(GetConfigForAdaptiveToolbar());
  }
  if (IsEnabledContextualPageActions()) {
    configs.emplace_back(GetConfigForContextualPageActions(context));
  }

  configs.emplace_back(ChromeStartModel::GetConfig());
  configs.emplace_back(QueryTilesModel::GetConfig());
  configs.emplace_back(ChromeStartModelV2::GetConfig());
  configs.emplace_back(IntentionalUserModel::GetConfig());
  configs.emplace_back(PowerUserSegment::GetConfig());
  configs.emplace_back(FrequentFeatureUserModel::GetConfig());
#endif

  configs.emplace_back(LowUserEngagementModel::GetConfig());
  configs.emplace_back(SearchUserModel::GetConfig());
  configs.emplace_back(FeedUserSegment::GetConfig());
  configs.emplace_back(ShoppingUserModel::GetConfig());
  configs.emplace_back(CrossDeviceUserSegment::GetConfig());
  configs.emplace_back(ResumeHeavyUserModel::GetConfig());
  configs.emplace_back(DeviceSwitcherModel::GetConfig());

  base::EraseIf(configs, [](const auto& config) { return !config.get(); });

  AppendConfigsFromExperiments(configs);
  return configs;
}

void AppendConfigsFromExperiments(
    std::vector<std::unique_ptr<Config>>& out_configs) {
  base::FieldTrial::ActiveGroups active_groups;
  base::FieldTrialList::GetActiveFieldTrialGroups(&active_groups);
  std::vector<std::string> param_values;

  for (const auto& active_group : active_groups) {
    base::FieldTrialParams params;
    if (base::GetFieldTrialParams(active_group.trial_name, &params)) {
      const auto& it = params.find(kSegmentationConfigParamName);
      if (it == params.end())
        continue;
      param_values.push_back(it->second);
    }
  }

  for (const std::string& param : param_values) {
    auto config = ParseConfigFromString(param);
    VLOG(1) << "Segmentation config param from experiment, "
            << (config ? "added successfully: " : "failed to parse: ") << param;
    if (config) {
      out_configs.push_back(std::move(config));
    }
  }
}

FieldTrialRegisterImpl::FieldTrialRegisterImpl() = default;
FieldTrialRegisterImpl::~FieldTrialRegisterImpl() = default;

void FieldTrialRegisterImpl::RegisterFieldTrial(base::StringPiece trial_name,
                                                base::StringPiece group_name) {
  // The register method is called early in startup once the platform is
  // initialized. So, in most cases the client will register the field trial
  // before uploading the first UMA log of the current session. We do not want
  // to annotate logs from the previous session. (These comes in two types:
  // histograms persisted from the previous session or stability information
  // about the previous session.) Groups are not stable across sessions; we
  // don't know if the current segmentation applies to the previous session.
  // Incidentally, the platform records metrics to track the movement between
  // groups.
  // TODO(ssid): Move to a MetricsProvider approach to fill the groups so we are
  // able to track how often we miss the first session log due to delays in
  // platform initialization.
  ChromeMetricsServiceAccessor::RegisterSyntheticFieldTrial(
      trial_name, group_name,
      variations::SyntheticTrialAnnotationMode::kCurrentLog);
}

void FieldTrialRegisterImpl::RegisterSubsegmentFieldTrialIfNeeded(
    base::StringPiece trial_name,
    SegmentId segment_id,
    int subsegment_rank) {
  absl::optional<std::string> group_name;
  // TODO(ssid): Make GetSubsegmentName as a ModelProvider API so that clients
  // can simply implement it instead of adding conditions here, once the
  // subsegment process is more stable.
  if (segment_id == SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_FEED_USER) {
    group_name = FeedUserSegment::GetSubsegmentName(subsegment_rank);
  }
#if BUILDFLAG(IS_ANDROID)
  if (segment_id == SegmentId::POWER_USER_SEGMENT) {
    group_name = PowerUserSegment::GetSubsegmentName(subsegment_rank);
  }
#endif
  if (segment_id == SegmentId::CROSS_DEVICE_USER_SEGMENT) {
    group_name = CrossDeviceUserSegment::GetSubsegmentName(subsegment_rank);
  }

  if (!group_name) {
    return;
  }
  RegisterFieldTrial(trial_name, *group_name);
}

}  // namespace segmentation_platform
