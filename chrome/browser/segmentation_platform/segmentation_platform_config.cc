// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/segmentation_platform/segmentation_platform_config.h"

#include <memory>

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "components/segmentation_platform/embedder/default_model/cross_device_user_segment.h"
#include "components/segmentation_platform/embedder/default_model/feed_user_segment.h"
#include "components/segmentation_platform/embedder/default_model/low_user_engagement_model.h"
#include "components/segmentation_platform/embedder/default_model/resume_heavy_user_model.h"
#include "components/segmentation_platform/embedder/default_model/shopping_user_model.h"
#include "components/segmentation_platform/internal/config_parser.h"
#include "components/segmentation_platform/public/config.h"
#include "components/segmentation_platform/public/constants.h"
#include "components/segmentation_platform/public/features.h"
#include "components/segmentation_platform/public/model_provider.h"
#include "components/segmentation_platform/public/proto/segmentation_platform.pb.h"
#include "content/public/browser/browser_context.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/commerce/shopping_service_factory.h"
#include "chrome/browser/feature_guide/notifications/feature_notification_guide_service.h"
#include "chrome/browser/flags/android/cached_feature_flags.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "chrome/browser/segmentation_platform/default_model/chrome_start_model_android.h"
#include "chrome/browser/segmentation_platform/default_model/chrome_start_model_android_v2.h"
#include "chrome/browser/ui/android/start_surface/start_surface_android.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/commerce/core/shopping_service.h"
#include "components/query_tiles/switches.h"
#include "components/segmentation_platform/embedder/default_model/intentional_user_model.h"
#include "components/segmentation_platform/embedder/default_model/power_user_segment.h"
#include "components/segmentation_platform/embedder/default_model/price_tracking_action_model.h"
#include "components/segmentation_platform/embedder/default_model/query_tiles_model.h"
#include "components/segmentation_platform/embedder/input_delegate/price_tracking_input_delegate.h"
#endif

namespace segmentation_platform {

using proto::SegmentId;

namespace {

constexpr char kDefaultModelEnabledParam[] = "enable_default_model";

// Default TTL for segment selection and unknown selection:

constexpr int kChromeLowUserEngagementSelectionTTLDays = 7;

constexpr int kFeedUserSegmentSelectionTTLDays = 14;
constexpr int kFeedUserSegmentUnknownSelectionTTLDays = 14;

constexpr int kShoppingUserDefaultSelectionTTLDays = 7;
constexpr int kShoppingUserDefaultUnknownSelectionTTLDays = 7;

constexpr int kCrossDeviceUserSegmentSelectionTTLDays = 7;
constexpr int kCrossDeviceUserSegmentUnknownSelectionTTLDays = 7;

constexpr int kResumeHeavyUserSegmentSelectionTTLDays = 14;
constexpr int kResumeHeavyUserSegmentUnknownSelectionTTLDays = 14;

constexpr char kVariationsParamNameSegmentSelectionTTLDays[] =
    "segment_selection_ttl_days";
constexpr char kVariationsParamNameUnknownSelectionTTLDays[] =
    "unknown_selection_ttl_days";

#if BUILDFLAG(IS_ANDROID)

constexpr int kAdaptiveToolbarDefaultSelectionTTLDays = 56;

constexpr int kChromeStartDefaultSelectionTTLDays = 30;
constexpr int kChromeStartDefaultUnknownTTLDays = 7;
// See
// https://source.chromium.org/chromium/chromium/src/+/main:chrome/android/java/src/org/chromium/chrome/browser/query_tiles/QueryTileUtils.java
const char kNumDaysKeepShowingQueryTiles[] =
    "num_days_keep_showing_query_tiles";
const char kNumDaysMVCkicksBelowThreshold[] =
    "num_days_mv_clicks_below_threshold";

// DEFAULT_NUM_DAYS_KEEP_SHOWING_QUERY_TILES
constexpr int kQueryTilesDefaultSelectionTTLDays = 28;
// DEFAULT_NUM_DAYS_MV_CLICKS_BELOW_THRESHOLD
constexpr int kQueryTilesDefaultUnknownTTLDays = 7;
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_ANDROID)
std::unique_ptr<Config> GetConfigForAdaptiveToolbar() {
  auto config = std::make_unique<Config>();
  config->segmentation_key = kAdaptiveToolbarSegmentationKey;
  config->segmentation_uma_name = kAdaptiveToolbarUmaName;

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

  return config;
}
#endif

#if BUILDFLAG(IS_ANDROID)
std::unique_ptr<ModelProvider> GetChromeStartAndroidModel() {
  if (!base::GetFieldTrialParamByFeatureAsBool(
          chrome::android::kStartSurfaceAndroid, kDefaultModelEnabledParam,
          false)) {
    return nullptr;
  }
  return std::make_unique<ChromeStartModel>();
}

std::unique_ptr<Config> GetConfigForChromeStartAndroid() {
  auto config = std::make_unique<Config>();
  config->segmentation_key = kChromeStartAndroidSegmentationKey;
  config->segmentation_uma_name = kChromeStartAndroidUmaName;
  config->AddSegmentId(
      SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_CHROME_START_ANDROID,
      GetChromeStartAndroidModel());

  int segment_selection_ttl_days = base::GetFieldTrialParamByFeatureAsInt(
      chrome::android::kStartSurfaceAndroid,
      kVariationsParamNameSegmentSelectionTTLDays,
      kChromeStartDefaultSelectionTTLDays);
  int unknown_selection_ttl_days = base::GetFieldTrialParamByFeatureAsInt(
      chrome::android::kStartSurfaceAndroid,
      "segment_unknown_selection_ttl_days", kChromeStartDefaultUnknownTTLDays);
  config->segment_selection_ttl = base::Days(segment_selection_ttl_days);
  config->unknown_selection_ttl = base::Days(unknown_selection_ttl_days);

  return config;
}

std::unique_ptr<ModelProvider> GetChromeStartAndroidModelV2() {
  if (!base::GetFieldTrialParamByFeatureAsBool(
          chrome::android::kStartSurfaceReturnTime, kDefaultModelEnabledParam,
          true)) {
    return nullptr;
  }
  return std::make_unique<ChromeStartModelV2>();
}

std::unique_ptr<Config> GetConfigForChromeStartAndroidV2() {
  auto config = std::make_unique<Config>();
  config->segmentation_key = kChromeStartAndroidV2SegmentationKey;
  config->segmentation_uma_name = kChromeStartAndroidV2UmaName;
  config->AddSegmentId(
      SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_CHROME_START_ANDROID_V2,
      GetChromeStartAndroidModelV2());

  int segment_selection_ttl_days = base::GetFieldTrialParamByFeatureAsInt(
      chrome::android::kStartSurfaceReturnTime,
      kVariationsParamNameSegmentSelectionTTLDays,
      kChromeStartDefaultSelectionTTLDays);
  config->segment_selection_ttl = base::Days(segment_selection_ttl_days);
  config->unknown_selection_ttl = config->segment_selection_ttl;

  return config;
}

std::unique_ptr<ModelProvider> GetQueryTilesDefaultModel() {
  if (!base::GetFieldTrialParamByFeatureAsBool(
          query_tiles::features::kQueryTilesSegmentation,
          kDefaultModelEnabledParam, false)) {
    return nullptr;
  }
  return std::make_unique<QueryTilesModel>();
}

std::unique_ptr<Config> GetConfigForQueryTiles() {
  auto config = std::make_unique<Config>();
  config->segmentation_key = kQueryTilesSegmentationKey;
  config->segmentation_uma_name = kQueryTilesUmaName;
  config->AddSegmentId(SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_QUERY_TILES,
                       GetQueryTilesDefaultModel());

  int segment_selection_ttl_days = base::GetFieldTrialParamByFeatureAsInt(
      query_tiles::features::kQueryTilesSegmentation,
      kNumDaysKeepShowingQueryTiles, kQueryTilesDefaultSelectionTTLDays);
  int unknown_selection_ttl_days = base::GetFieldTrialParamByFeatureAsInt(
      query_tiles::features::kQueryTilesSegmentation,
      kNumDaysMVCkicksBelowThreshold, kQueryTilesDefaultUnknownTTLDays);
  config->segment_selection_ttl = base::Days(segment_selection_ttl_days);
  config->unknown_selection_ttl = base::Days(unknown_selection_ttl_days);
  return config;
}

std::unique_ptr<Config> GetConfigForIntentionalUser() {
  auto config = std::make_unique<Config>();
  config->segmentation_key = kIntentionalUserKey;
  config->segmentation_uma_name = kIntentionalUserUmaName;
  config->AddSegmentId(SegmentId::INTENTIONAL_USER_SEGMENT,
                       std::make_unique<IntentionalUserModel>());
  config->segment_selection_ttl = base::Days(7);
  config->unknown_selection_ttl = base::Days(7);

  return config;
}

std::unique_ptr<Config> GetConfigForPowerUser() {
  auto config = std::make_unique<Config>();
  config->segmentation_key = kPowerUserKey;
  config->segmentation_uma_name = kPowerUserUmaName;
  config->AddSegmentId(SegmentId::POWER_USER_SEGMENT,
                       std::make_unique<PowerUserSegment>());
  config->segment_selection_ttl = base::Days(7);
  config->unknown_selection_ttl = base::Days(7);

  return config;
}

bool IsEnabledContextualPageActions() {
  if (!base::FeatureList::IsEnabled(features::kContextualPageActions))
    return false;

  return base::FeatureList::IsEnabled(
             features::kContextualPageActionPriceTracking) &&
         base::FeatureList::IsEnabled(commerce::kShoppingList);
}

std::unique_ptr<Config> GetConfigForContextualPageActions(
    content::BrowserContext* context) {
  auto config = std::make_unique<Config>();
  config->segmentation_key = kContextualPageActionsKey;
  config->segmentation_uma_name = kContextualPageActionsUmaName;
  if (base::FeatureList::IsEnabled(
          features::kContextualPageActionPriceTracking) &&
      base::FeatureList::IsEnabled(commerce::kShoppingList)) {
    config->AddSegmentId(
        SegmentId::OPTIMIZATION_TARGET_CONTEXTUAL_PAGE_ACTION_PRICE_TRACKING,
        std::make_unique<PriceTrackingActionModel>());

    auto shopping_service_getter = base::BindRepeating(
        commerce::ShoppingServiceFactory::GetForBrowserContextIfExists,
        context);
    auto bookmark_model_getter = base::BindRepeating(
        BookmarkModelFactory::GetForBrowserContext, context);
    auto price_tracking_input_delegate =
        std::make_unique<processing::PriceTrackingInputDelegate>(
            shopping_service_getter, bookmark_model_getter);
    config
        ->input_delegates[proto::CustomInput_FillPolicy_PRICE_TRACKING_HINTS] =
        std::move(price_tracking_input_delegate);
  }
  config->on_demand_execution = true;
  return config;
}

#endif  // BUILDFLAG(IS_ANDROID)

std::unique_ptr<ModelProvider> GetLowEngagementDefaultModel() {
  if (!base::GetFieldTrialParamByFeatureAsBool(
          features::kSegmentationPlatformLowEngagementFeature,
          kDefaultModelEnabledParam, true)) {
    return nullptr;
  }
  return std::make_unique<LowUserEngagementModel>();
}

bool IsLowEngagementFeatureEnabled() {
  // TODO(ssid): Remove this extra feature and change feature guide to use the
  // segmentation defined feature.
#if BUILDFLAG(IS_ANDROID)
  if (base::FeatureList::IsEnabled(
          feature_guide::features::kSegmentationModelLowEngagedUsers)) {
    return true;
  }
#endif
  return base::FeatureList::IsEnabled(
      features::kSegmentationPlatformLowEngagementFeature);
}

std::unique_ptr<Config> GetConfigForChromeLowUserEngagement() {
  auto config = std::make_unique<Config>();
  config->segmentation_key = kChromeLowUserEngagementSegmentationKey;
  config->segmentation_uma_name = kChromeLowUserEngagementUmaName;
  config->AddSegmentId(
      SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_CHROME_LOW_USER_ENGAGEMENT,
      GetLowEngagementDefaultModel());

#if BUILDFLAG(IS_ANDROID)
  int segment_selection_ttl_days = base::GetFieldTrialParamByFeatureAsInt(
      feature_guide::features::kSegmentationModelLowEngagedUsers,
      kVariationsParamNameSegmentSelectionTTLDays,
      kChromeLowUserEngagementSelectionTTLDays);
#else
  int segment_selection_ttl_days = base::GetFieldTrialParamByFeatureAsInt(
      features::kSegmentationPlatformLowEngagementFeature,
      kVariationsParamNameSegmentSelectionTTLDays,
      kChromeLowUserEngagementSelectionTTLDays);
#endif

  config->segment_selection_ttl = base::Days(segment_selection_ttl_days);
  config->unknown_selection_ttl = base::Days(segment_selection_ttl_days);
  return config;
}

std::unique_ptr<ModelProvider> GetFeedUserSegmentDefautlModel() {
  if (!base::GetFieldTrialParamByFeatureAsBool(
          features::kSegmentationPlatformFeedSegmentFeature,
          kDefaultModelEnabledParam, true)) {
    return nullptr;
  }
  return std::make_unique<FeedUserSegment>();
}

std::unique_ptr<Config> GetConfigForResumeHeavyUserSegment() {
  auto config = std::make_unique<Config>();
  config->segmentation_key = kResumeHeavyUserKey;
  config->segmentation_uma_name = kResumeHeavyUserUmaName;
  config->AddSegmentId(SegmentId::RESUME_HEAVY_USER_SEGMENT,
                       std::make_unique<ResumeHeavyUserModel>());
  config->segment_selection_ttl =
      base::Days(base::GetFieldTrialParamByFeatureAsInt(
          features::kResumeHeavyUserSegmentFeature,
          kVariationsParamNameSegmentSelectionTTLDays,
          kResumeHeavyUserSegmentSelectionTTLDays));
  config->unknown_selection_ttl =
      base::Days(base::GetFieldTrialParamByFeatureAsInt(
          features::kResumeHeavyUserSegmentFeature,
          kVariationsParamNameUnknownSelectionTTLDays,
          kResumeHeavyUserSegmentUnknownSelectionTTLDays));
  return config;
}

std::unique_ptr<Config> GetConfigForFeedSegments() {
  auto config = std::make_unique<Config>();
  config->segmentation_key = kFeedUserSegmentationKey;
  config->segmentation_uma_name = kFeedUserSegmentUmaName;
  config->AddSegmentId(SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_FEED_USER,
                       GetFeedUserSegmentDefautlModel());
  config->segment_selection_ttl =
      base::Days(base::GetFieldTrialParamByFeatureAsInt(
          features::kSegmentationPlatformFeedSegmentFeature,
          kVariationsParamNameSegmentSelectionTTLDays,
          kFeedUserSegmentSelectionTTLDays));
  config->unknown_selection_ttl =
      base::Days(base::GetFieldTrialParamByFeatureAsInt(
          features::kSegmentationPlatformFeedSegmentFeature,
          kVariationsParamNameUnknownSelectionTTLDays,
          kFeedUserSegmentUnknownSelectionTTLDays));
  return config;
}

std::unique_ptr<ModelProvider> GetShoppingUserDefaultModel() {
  if (!base::GetFieldTrialParamByFeatureAsBool(
          features::kShoppingUserSegmentFeature, kDefaultModelEnabledParam,
          true)) {
    return nullptr;
  }
  return std::make_unique<ShoppingUserModel>();
}

std::unique_ptr<Config> GetConfigForShoppingUser() {
  auto config = std::make_unique<Config>();
  config->segmentation_key = kShoppingUserSegmentationKey;
  config->segmentation_uma_name = kShoppingUserUmaName;
  config->AddSegmentId(
      SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_SHOPPING_USER,
      GetShoppingUserDefaultModel());
  config->segment_selection_ttl =
      base::Days(base::GetFieldTrialParamByFeatureAsInt(
          features::kShoppingUserSegmentFeature,
          kVariationsParamNameSegmentSelectionTTLDays,
          kShoppingUserDefaultSelectionTTLDays));
  config->unknown_selection_ttl =
      base::Days(base::GetFieldTrialParamByFeatureAsInt(
          features::kShoppingUserSegmentFeature,
          kVariationsParamNameUnknownSelectionTTLDays,
          kShoppingUserDefaultUnknownSelectionTTLDays));
  return config;
}

std::unique_ptr<ModelProvider> GetCrossDeviceUserSegmentDefautlModel() {
  return std::make_unique<CrossDeviceUserSegment>();
}

std::unique_ptr<Config> GetConfigForCrossDeviceSegments() {
  auto config = std::make_unique<Config>();
  config->segmentation_key = kCrossDeviceUserKey;
  config->segmentation_uma_name = kCrossDeviceUserUmaName;
  config->AddSegmentId(SegmentId::CROSS_DEVICE_USER_SEGMENT,
                       GetCrossDeviceUserSegmentDefautlModel());
  config->segment_selection_ttl =
      base::Days(kCrossDeviceUserSegmentSelectionTTLDays);
  config->unknown_selection_ttl =
      base::Days(kCrossDeviceUserSegmentUnknownSelectionTTLDays);
  return config;
}

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
  if (IsStartSurfaceBehaviouralTargetingEnabled()) {
    configs.emplace_back(GetConfigForChromeStartAndroid());
  }
  if (base::FeatureList::IsEnabled(
          query_tiles::features::kQueryTilesSegmentation)) {
    configs.emplace_back(GetConfigForQueryTiles());
  }

  configs.emplace_back(GetConfigForChromeStartAndroidV2());
  configs.emplace_back(GetConfigForIntentionalUser());

  if (base::FeatureList::IsEnabled(
          features::kSegmentationPlatformPowerUserFeature)) {
    configs.emplace_back(GetConfigForPowerUser());
  }
#endif
  if (IsLowEngagementFeatureEnabled()) {
    configs.emplace_back(GetConfigForChromeLowUserEngagement());
  }

  if (base::FeatureList::IsEnabled(
          features::kSegmentationPlatformFeedSegmentFeature)) {
    configs.emplace_back(GetConfigForFeedSegments());
  }

  if (base::FeatureList::IsEnabled(features::kShoppingUserSegmentFeature)) {
    configs.emplace_back(GetConfigForShoppingUser());
  }

  configs.emplace_back(GetConfigForCrossDeviceSegments());

  if (base::FeatureList::IsEnabled(features::kResumeHeavyUserSegmentFeature)) {
    configs.emplace_back(GetConfigForResumeHeavyUserSegment());
  }

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
      const auto& it = params.find("segmentation_platform_add_config_param");
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
  if (segment_id == SegmentId::CROSS_DEVICE_USER_SEGMENT) {
    group_name = CrossDeviceUserSegment::GetSubsegmentName(subsegment_rank);
  }

  if (!group_name) {
    return;
  }
  RegisterFieldTrial(trial_name, *group_name);
}

}  // namespace segmentation_platform
