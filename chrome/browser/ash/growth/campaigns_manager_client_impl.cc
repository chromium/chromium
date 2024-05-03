// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/growth/campaigns_manager_client_impl.h"

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <variant>

#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/no_destructor.h"
#include "base/strings/string_number_conversions.h"
#include "base/version.h"
#include "chrome/browser/ash/growth/install_web_app_action_performer.h"
#include "chrome/browser/ash/growth/metrics.h"
#include "chrome/browser/ash/growth/open_url_action_performer.h"
#include "chrome/browser/ash/growth/show_notification_action_performer.h"
#include "chrome/browser/ash/growth/show_nudge_action_performer.h"
#include "chrome/browser/ash/login/demo_mode/demo_components.h"
#include "chrome/browser/ash/login/demo_mode/demo_mode_dimensions.h"
#include "chrome/browser/ash/login/demo_mode/demo_session.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chromeos/ash/components/growth/campaigns_constants.h"
#include "chromeos/ash/components/growth/campaigns_manager.h"
#include "chromeos/ash/components/growth/growth_metrics.h"
#include "components/component_updater/ash/component_manager_ash.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/public/tracker.h"
#include "components/variations/synthetic_trials.h"

namespace {

inline constexpr char kCampaignComponentName[] = "growth-campaigns";

// The synthetic trial name prefix for growth experiment. Formatted as
// `CrOSGrowthStudy{studyId}`, where `studyId` is an integer. For non
// experimental campaigns, `studyId` will be empty.
inline constexpr char kGrowthStudyName[] = "CrOSGrowthStudy";
// The synthetical trial group name for growth experiment. The campaign id
// will be unique for different groups.
inline constexpr char kGrowthGroupName[] = "CampaignId";

Profile* GetProfile() {
  return ProfileManager::GetActiveUserProfile();
}

}  // namespace

CampaignsManagerClientImpl::CampaignsManagerClientImpl() {
  // `show_nudge_performer_observation_` is used in `campaigns_manager_` ctor,
  // so it needs to be initialized first.
  campaigns_manager_ = std::make_unique<growth::CampaignsManager>(
      /*client=*/this, g_browser_process->local_state());
}

CampaignsManagerClientImpl::~CampaignsManagerClientImpl() = default;

void CampaignsManagerClientImpl::LoadCampaignsComponent(
    growth::CampaignComponentLoadedCallback callback) {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(ash::switches::kGrowthCampaignsPath)) {
    std::move(callback).Run(base::FilePath(command_line->GetSwitchValueASCII(
        ash::switches::kGrowthCampaignsPath)));
    return;
  }

  // Loads campaigns component.
  auto component_manager_ash =
      g_browser_process->platform_part()->component_manager_ash();
  CHECK(component_manager_ash);

  component_manager_ash->Load(
      kCampaignComponentName,
      component_updater::ComponentManagerAsh::MountPolicy::kMount,
      component_updater::ComponentManagerAsh::UpdatePolicy::kDontForce,
      base::BindOnce(&CampaignsManagerClientImpl::OnComponentDownloaded,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

bool CampaignsManagerClientImpl::IsDeviceInDemoMode() const {
  return ash::DemoSession::IsDeviceInDemoMode();
}

bool CampaignsManagerClientImpl::IsCloudGamingDevice() const {
  return ash::demo_mode::IsCloudGamingDevice();
}

bool CampaignsManagerClientImpl::IsFeatureAwareDevice() const {
  return ash::demo_mode::IsFeatureAwareDevice();
}

const std::string& CampaignsManagerClientImpl::GetApplicationLocale() const {
  return g_browser_process->GetApplicationLocale();
}

const base::Version& CampaignsManagerClientImpl::GetDemoModeAppVersion() const {
  auto* demo_session = ash::DemoSession::Get();
  CHECK(demo_session);

  const auto& version = demo_session->components()->app_component_version();
  if (!version.has_value()) {
    growth::RecordCampaignsManagerError(
        growth::CampaignsManagerError::kDemoModeAppVersionUnavailable);
    static const base::NoDestructor<base::Version> empty_version;
    return *empty_version;
  }

  return version.value();
}

growth::ActionMap CampaignsManagerClientImpl::GetCampaignsActions() {
  growth::ActionMap action_map;
  action_map.emplace(
      make_pair(growth::ActionType::kInstallWebApp,
                std::make_unique<InstallWebAppActionPerformer>()));
  action_map.emplace(make_pair(growth::ActionType::kOpenUrl,
                               std::make_unique<OpenUrlActionPerformer>()));

  std::unique_ptr<ShowNudgeActionPerformer> show_nudge_performer =
      std::make_unique<ShowNudgeActionPerformer>();
  show_nudge_performer_observation_.Observe(show_nudge_performer.get());
  action_map.emplace(make_pair(growth::ActionType::kShowNudge,
                               std::move(show_nudge_performer)));
  std::unique_ptr<ShowNotificationActionPerformer> show_notification_performer =
      std::make_unique<ShowNotificationActionPerformer>();
  show_notification_performer_observation_.Observe(
      show_notification_performer.get());
  action_map.emplace(make_pair(growth::ActionType::kShowNotification,
                               std::move(show_notification_performer)));
  return action_map;
}

void CampaignsManagerClientImpl::RegisterSyntheticFieldTrial(
    const std::optional<int> study_id,
    const int campaign_id) const {
  // If `study_id` is not null, appends it to the end of `trial_name`.
  std::string trial_name(kGrowthStudyName);
  if (study_id) {
    base::StringAppendF(&trial_name, "%d", *study_id);
  }
  std::string group_name(kGrowthGroupName);
  base::StringAppendF(&group_name, "%d", campaign_id);
  ChromeMetricsServiceAccessor::RegisterSyntheticFieldTrial(trial_name,
                                                            group_name);
}

void CampaignsManagerClientImpl::NotifyEvent(const std::string& event_name) {
  auto* tracker =
      feature_engagement::TrackerFactory::GetForBrowserContext(GetProfile());
  if (!tracker || !tracker->IsInitialized()) {
    LOG(ERROR) << "Feature Engagement tracer is not available";
    return;
  }

  tracker->NotifyEvent(event_name);
}

void CampaignsManagerClientImpl::ClearConfig(
    const std::map<std::string, std::string>& params) {
  auto* tracker =
      feature_engagement::TrackerFactory::GetForBrowserContext(GetProfile());
  if (!tracker || !tracker->IsInitialized()) {
    LOG(ERROR) << "Feature Engagement tracer is not available";
    return;
  }

  UpdateConfig(params);
  tracker->ClearEventData(feature_engagement::kIPHGrowthFramework);
}

bool CampaignsManagerClientImpl::WouldTriggerHelpUI(
    const std::map<std::string, std::string>& params) {
  auto* tracker =
      feature_engagement::TrackerFactory::GetForBrowserContext(GetProfile());
  if (!tracker || !tracker->IsInitialized()) {
    LOG(ERROR) << "Feature Engagement tracer is not available";
    return false;
  }

  UpdateConfig(params);
  return tracker->WouldTriggerHelpUI(feature_engagement::kIPHGrowthFramework);
}

signin::IdentityManager* CampaignsManagerClientImpl::GetIdentityManager()
    const {
  return IdentityManagerFactory::GetForProfile(GetProfile());
}

void CampaignsManagerClientImpl::OnReadyToLogImpression(int campaign_id) {
  RecordImpression(campaign_id);
  campaigns_manager_->NotifyEventForTargeting(
      growth::CampaignEvent::kImpression, base::NumberToString(campaign_id));
}

void CampaignsManagerClientImpl::OnDismissed(int campaign_id) {
  RecordDismissed(campaign_id);
}

void CampaignsManagerClientImpl::OnButtonPressed(int campaign_id,
                                                 CampaignButtonId button_id,
                                                 bool should_mark_dismissed) {
  RecordButtonPressed(campaign_id, button_id);
  if (!should_mark_dismissed) {
    return;
  }

  // Notify `kDismissed` event to the Feature Engagement framework. This event
  // will be stored and could be used later.
  switch (button_id) {
    case CampaignButtonId::kPrimary:
    case CampaignButtonId::kSecondary:
      // Primary and Secondary button press will treated as user dismissal.
      campaigns_manager_->NotifyEventForTargeting(
          growth::CampaignEvent::kDismissed, base::NumberToString(campaign_id));
      break;
    case CampaignButtonId::kOthers:
      break;
  }
}

void CampaignsManagerClientImpl::OnComponentDownloaded(
    growth::CampaignComponentLoadedCallback loaded_callback,
    component_updater::ComponentManagerAsh::Error error,
    const base::FilePath& path) {
  if (error != component_updater::ComponentManagerAsh::Error::NONE) {
    std::move(loaded_callback).Run(std::nullopt);
    return;
  }

  std::move(loaded_callback).Run(path);
}

void CampaignsManagerClientImpl::UpdateConfig(
    const std::map<std::string, std::string>& params) {
  auto* tracker =
      feature_engagement::TrackerFactory::GetForBrowserContext(GetProfile());
  if (!tracker || !tracker->IsInitialized()) {
    LOG(ERROR) << "Feature Engagement tracer is not available";
    return;
  }

  config_provider_.SetConfig(params);
  tracker->UpdateConfig(feature_engagement::kIPHGrowthFramework,
                        &config_provider_);
}
