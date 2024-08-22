// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/segmentation_platform/segmentation_platform_config.h"

#include <memory>
#include <string_view>
#include <vector>

#include "base/check_is_test.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "build/build_config.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "components/compose/buildflags.h"
#include "components/search/ntp_features.h"
#include "components/segmentation_platform/embedder/default_model/cross_device_user_segment.h"
#include "components/segmentation_platform/embedder/default_model/database_api_clients.h"
#include "components/segmentation_platform/embedder/default_model/device_switcher_model.h"
#include "components/segmentation_platform/embedder/default_model/feed_user_segment.h"
#include "components/segmentation_platform/embedder/default_model/frequent_feature_user_model.h"
#include "components/segmentation_platform/embedder/default_model/low_user_engagement_model.h"
#include "components/segmentation_platform/embedder/default_model/metrics_clustering.h"
#include "components/segmentation_platform/embedder/default_model/optimization_target_segmentation_dummy.h"
#include "components/segmentation_platform/embedder/default_model/password_manager_user_segment.h"
#include "components/segmentation_platform/embedder/default_model/resume_heavy_user_model.h"
#include "components/segmentation_platform/embedder/default_model/search_user_model.h"
#include "components/segmentation_platform/embedder/default_model/shopping_user_model.h"
#include "components/segmentation_platform/embedder/default_model/tab_resumption_ranker.h"
#include "components/segmentation_platform/embedder/default_model/url_visit_resumption_ranker.h"
#include "components/segmentation_platform/embedder/home_modules/ephemeral_home_module_backend.h"
#include "components/segmentation_platform/internal/config_parser.h"
#include "components/segmentation_platform/public/config.h"
#include "components/segmentation_platform/public/constants.h"
#include "components/segmentation_platform/public/features.h"
#include "components/segmentation_platform/public/proto/segmentation_platform.pb.h"
#include "components/webapps/browser/features.h"
#include "content/public/browser/browser_context.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/commerce/shopping_service_factory.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/commerce/core/shopping_service.h"
#include "components/segmentation_platform/embedder/default_model/android_home_module_ranker.h"
#include "components/segmentation_platform/embedder/default_model/contextual_page_actions_model.h"
#include "components/segmentation_platform/embedder/default_model/device_tier_segment.h"
#include "components/segmentation_platform/embedder/default_model/intentional_user_model.h"
#include "components/segmentation_platform/embedder/default_model/most_visited_tiles_user.h"
#include "components/segmentation_platform/embedder/default_model/power_user_segment.h"
#include "components/segmentation_platform/embedder/default_model/tablet_productivity_user_model.h"
#endif

#if BUILDFLAG(ENABLE_COMPOSE)
#include "components/segmentation_platform/embedder/default_model/compose_promotion.h"
#endif

namespace segmentation_platform {

using proto::SegmentId;

namespace {

#if BUILDFLAG(IS_ANDROID)

constexpr int kAdaptiveToolbarDefaultSelectionTTLDays = 56;

#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_ANDROID)
std::unique_ptr<Config> GetConfigForAdaptiveToolbar() {
  if (!base::FeatureList::IsEnabled(
          chrome::android::kAdaptiveButtonInTopToolbarCustomizationV2)) {
    return nullptr;
  }
  auto config = std::make_unique<Config>();
  config->segmentation_key = kAdaptiveToolbarSegmentationKey;
  config->segmentation_uma_name = kAdaptiveToolbarUmaName;
  config->auto_execute_and_cache = true;

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

std::unique_ptr<Config> GetConfigForContextualPageActions(
    content::BrowserContext* context) {
  auto config = std::make_unique<Config>();
  config->segmentation_key = kContextualPageActionsKey;
  config->segmentation_uma_name = kContextualPageActionsUmaName;
  config->AddSegmentId(
      SegmentId::OPTIMIZATION_TARGET_CONTEXTUAL_PAGE_ACTION_PRICE_TRACKING,
      std::make_unique<ContextualPageActionsModel>());
  config->auto_execute_and_cache = false;
  return config;
}

#endif  // BUILDFLAG(IS_ANDROID)

std::unique_ptr<Config> GetConfigForWebAppInstallationPromo() {
  auto config = std::make_unique<Config>();
  config->segmentation_key = kWebAppInstallationPromoKey;
  config->segmentation_uma_name = kWebAppInstallationPromoUmaName;
  config->AddSegmentId(
      SegmentId::OPTIMIZATION_TARGET_WEB_APP_INSTALLATION_PROMO);
  config->auto_execute_and_cache = false;
  return config;
}

std::unique_ptr<Config> GetConfigForDesktopNtpModule() {
  auto config = std::make_unique<Config>();
  config->segmentation_key = kDesktopNtpModuleKey;
  config->segmentation_uma_name = kDesktopNtpModuleUmaName;
  config->AddSegmentId(
      SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_DESKTOP_NTP_MODULE);
  config->auto_execute_and_cache = false;
  return config;
}

}  // namespace

// Note: Do not remove feature flag for models that are served on the server.
std::vector<std::unique_ptr<Config>> GetSegmentationPlatformConfig(
    content::BrowserContext* context,
    home_modules::HomeModulesCardRegistry* home_modules_card_registry) {
  std::vector<std::unique_ptr<Config>> configs;
#if BUILDFLAG(IS_ANDROID)
  if (base::FeatureList::IsEnabled(
          chrome::android::kAdaptiveButtonInTopToolbarCustomizationV2)) {
    configs.emplace_back(GetConfigForAdaptiveToolbar());
  }
  if (base::FeatureList::IsEnabled(features::kContextualPageActions)) {
    configs.emplace_back(GetConfigForContextualPageActions(context));
  }

  configs.emplace_back(IntentionalUserModel::GetConfig());
  configs.emplace_back(PowerUserSegment::GetConfig());
  configs.emplace_back(FrequentFeatureUserModel::GetConfig());
  configs.emplace_back(DeviceTierSegment::GetConfig());
  configs.emplace_back(TabletProductivityUserModel::GetConfig());
  configs.emplace_back(MostVisitedTilesUser::GetConfig());
  configs.emplace_back(AndroidHomeModuleRanker::GetConfig());
#endif
  configs.emplace_back(LowUserEngagementModel::GetConfig());
  configs.emplace_back(SearchUserModel::GetConfig());
  configs.emplace_back(FeedUserSegment::GetConfig());
  configs.emplace_back(ShoppingUserModel::GetConfig());
  configs.emplace_back(CrossDeviceUserSegment::GetConfig());
  configs.emplace_back(ResumeHeavyUserModel::GetConfig());
  configs.emplace_back(DeviceSwitcherModel::GetConfig());
  configs.emplace_back(TabResumptionRanker::GetConfig());
  configs.emplace_back(URLVisitResumptionRanker::GetConfig());
  configs.emplace_back(PasswordManagerUserModel::GetConfig());
  configs.emplace_back(DatabaseApiClients::GetConfig());
  configs.emplace_back(MetricsClustering::GetConfig());
  if (home_modules_card_registry) {
    configs.emplace_back(home_modules::EphemeralHomeModuleBackend::GetConfig(
        home_modules_card_registry));
  } else {
    CHECK_IS_TEST();
  }

#if BUILDFLAG(ENABLE_COMPOSE)
  configs.emplace_back(ComposePromotion::GetConfig());
#endif  // BUILDFLAG(ENABLE_COMPOSE)

  // Model used for testing.
  configs.emplace_back(OptimizationTargetSegmentationDummy::GetConfig());

  if (base::FeatureList::IsEnabled(
          webapps::features::kWebAppsEnableMLModelForPromotion)) {
    configs.emplace_back(GetConfigForWebAppInstallationPromo());
  }

  if (base::FeatureList::IsEnabled(ntp_features::kNtpDriveModuleSegmentation)) {
    configs.emplace_back(GetConfigForDesktopNtpModule());
  }

  std::erase_if(configs, [](const auto& config) { return !config.get(); });

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

void FieldTrialRegisterImpl::RegisterFieldTrial(std::string_view trial_name,
                                                std::string_view group_name) {
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
    std::string_view trial_name,
    SegmentId segment_id,
    int subsegment_rank) {
  std::optional<std::string> group_name;
  // TODO(ssid): Make GetSubsegmentName as a ModelProvider API so that clients
  // can simply implement it instead of adding conditions here, once the
  // subsegment process is more stable.
  if (!group_name) {
    return;
  }
  RegisterFieldTrial(trial_name, *group_name);
}

}  // namespace segmentation_platform
