// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/growth/campaigns_manager_client_impl.h"

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <variant>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/root_window_controller.h"
#include "ash/shelf/hotseat_widget.h"
#include "ash/shelf/shelf_app_button.h"
#include "ash/shelf/shelf_view.h"
#include "ash/shell.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/no_destructor.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/version.h"
#include "chrome/browser/ash/growth/campaigns_manager_session.h"
#include "chrome/browser/ash/growth/install_web_app_action_performer.h"
#include "chrome/browser/ash/growth/metrics.h"
#include "chrome/browser/ash/growth/open_url_action_performer.h"
#include "chrome/browser/ash/growth/show_notification_action_performer.h"
#include "chrome/browser/ash/growth/show_nudge_action_performer.h"
#include "chrome/browser/ash/growth/update_user_pref_action_performer.h"
#include "chrome/browser/ash/login/demo_mode/demo_components.h"
#include "chrome/browser/ash/login/demo_mode/demo_mode_dimensions.h"
#include "chrome/browser/ash/login/demo_mode/demo_session.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chromeos/ash/components/growth/campaigns_logger.h"
#include "chromeos/ash/components/growth/campaigns_manager.h"
#include "chromeos/ash/components/growth/campaigns_utils.h"
#include "chromeos/ash/components/growth/growth_metrics.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/component_updater/ash/component_manager_ash.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/public/tracker.h"
#include "components/language/core/browser/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/variations/service/variations_service.h"
#include "components/variations/synthetic_trials.h"

namespace {

inline constexpr char kCampaignComponentName[] = "growth-campaigns";

// A util function to add the `kGrowthCampaignsEventNamePrefix`.
std::string AddEventPrefix(std::string_view event) {
  return base::StrCat({growth::GetGrowthCampaignsEventNamePrefix(), event});
}

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
    const auto path =
        command_line->GetSwitchValuePath(ash::switches::kGrowthCampaignsPath);
    CAMPAIGNS_LOG(DEBUG) << "Switch `kGrowthCampaignsPath` is set. Load "
                            "campaigns component from file "
                         << path;
    std::move(callback).Run(base::FilePath(path));
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

void CampaignsManagerClientImpl::AddOnTrackerInitializedCallback(
    growth::OnTrackerInitializedCallback callback) {
  auto* tracker =
      feature_engagement::TrackerFactory::GetForBrowserContext(GetProfile());
  if (!tracker) {
    CAMPAIGNS_LOG(ERROR) << "Feature Engagement tracer is not available";
    std::move(callback).Run(false);
  }

  tracker->AddOnInitializedCallback(
      base::BindOnce(&CampaignsManagerClientImpl::OnTrackerInitialized,
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

bool CampaignsManagerClientImpl::IsAppIconOnShelf(
    const std::string& app_id) const {
  auto* shelf = ash::Shell::GetPrimaryRootWindowController()->shelf();
  const bool is_shelf_visible =
      shelf && (shelf->GetVisibilityState() ==
                    ash::ShelfVisibilityState::SHELF_VISIBLE ||
                (shelf->GetVisibilityState() ==
                     ash::ShelfVisibilityState::SHELF_AUTO_HIDE &&
                 shelf->GetAutoHideState() ==
                     ash::ShelfAutoHideState::SHELF_AUTO_HIDE_SHOWN));

  if (!is_shelf_visible) {
    growth::RecordCampaignsManagerError(
        growth::CampaignsManagerError::kShelfInvisibleAtMatching);
    CAMPAIGNS_LOG(ERROR) << "Matching hotseat state when shelf is not visible.";
    return false;
  }

  auto* hotseat_widget = shelf->hotseat_widget();
  const bool is_hotseat_visible =
      hotseat_widget && hotseat_widget->state() != ash::HotseatState::kNone &&
      hotseat_widget->state() != ash::HotseatState::kHidden;
  if (!is_hotseat_visible) {
    growth::RecordCampaignsManagerError(
        growth::CampaignsManagerError::kHotseatInvisibleAtMatching);
    CAMPAIGNS_LOG(ERROR)
        << "Matching hotseat state when hotseat is not visible.";
    return false;
  }

  auto* shelf_view = hotseat_widget->GetShelfView();
  if (!shelf_view) {
    growth::RecordCampaignsManagerError(
        growth::CampaignsManagerError::kShelfViewNotAvailableAtMatching);
    CAMPAIGNS_LOG(ERROR) << "Matching hotseat state when hotseat is available "
                            "but shelf_view is not available.";
    return false;
  }

  if (!shelf_view->GetShelfAppButton(ash::ShelfID(app_id))) {
    growth::RecordCampaignsManagerError(
        growth::CampaignsManagerError::kHotseatAppIconNotPresent);
    CAMPAIGNS_LOG(ERROR) << "App icon is not on shelf.";
    return false;
  }
  return true;
}

const std::string& CampaignsManagerClientImpl::GetApplicationLocale() const {
  // User selected locale, then resolved using
  // `l10n_util::CheckAndResolveLocale` to a platform locale.
  // For example: `en-IN` will be resolved to `en-GB`.
  return g_browser_process->GetApplicationLocale();
}

const std::string& CampaignsManagerClientImpl::GetUserLocale() const {
  // The locale as selected by the user, such as "en-IN". This is different
  // from `GetApplication` locale which is actually platform locale that
  // resolved using `l10n_util::CheckAndResolveLocale`.
  return GetProfile()->GetPrefs()->GetString(
      language::prefs::kApplicationLocale);
}

const std::string CampaignsManagerClientImpl::GetCountryCode() const {
  return g_browser_process->variations_service()->GetStoredPermanentCountry();
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
  action_map.emplace(
      make_pair(growth::ActionType::kUpdateUserPref,
                std::make_unique<UpdateUserPrefActionPerformer>()));
  return action_map;
}

void CampaignsManagerClientImpl::RegisterSyntheticFieldTrial(
    const std::string& trial_name,
    const std::string& group_name) const {
  CAMPAIGNS_LOG(DEBUG) << "Register synthetic field trial: trial_name: "
                       << trial_name << " group_name: " << group_name;
  ChromeMetricsServiceAccessor::RegisterSyntheticFieldTrial(
      trial_name, group_name,
      variations::SyntheticTrialAnnotationMode::kCurrentLog);
}

void CampaignsManagerClientImpl::RecordEvent(const std::string& event_name,
                                             bool trigger_campaigns) {
  auto* tracker =
      feature_engagement::TrackerFactory::GetForBrowserContext(GetProfile());
  if (!tracker || !tracker->IsInitialized()) {
    CAMPAIGNS_LOG(ERROR) << "Feature Engagement tracer is not available";
    growth::RecordCampaignsManagerError(
        growth::CampaignsManagerError::kTrackerNotAvailableInSession);
    return;
  }

  CAMPAIGNS_LOG(DEBUG) << "Record event: " << event_name
                       << " Trigger Campaigns: "
                       << growth::ToString(trigger_campaigns);
  tracker->NotifyEvent(AddEventPrefix(event_name));

  if (!trigger_campaigns) {
    return;
  }

  // If the App Mall app is not enabled, do not trigger by the event.
  if (event_name == growth::kGrowthCampaignsEventHotseatHover &&
      !chromeos::features::IsCrosMallSwaEnabled()) {
    return;
  }

  if (auto* session = CampaignsManagerSession::Get()) {
    session->MaybeTriggerCampaignsOnEvent(event_name);
  }
}

void CampaignsManagerClientImpl::ClearConfig(
    const std::map<std::string, std::string>& params) {
  auto* tracker =
      feature_engagement::TrackerFactory::GetForBrowserContext(GetProfile());
  if (!tracker || !tracker->IsInitialized()) {
    CAMPAIGNS_LOG(ERROR) << "Feature Engagement tracer is not available";
    growth::RecordCampaignsManagerError(
        growth::CampaignsManagerError::kTrackerNotAvailableInSession);
    return;
  }

  for (const auto& param : params) {
    CAMPAIGNS_LOG(DEBUG) << "Clear config: " << param.second;
  }
  UpdateConfig(params);
  tracker->ClearEventData(feature_engagement::kIPHGrowthFramework);
}

bool CampaignsManagerClientImpl::WouldTriggerHelpUI(
    const std::map<std::string, std::string>& params) {
  auto* tracker =
      feature_engagement::TrackerFactory::GetForBrowserContext(GetProfile());
  if (!tracker || !tracker->IsInitialized()) {
    CAMPAIGNS_LOG(ERROR) << "Feature Engagement tracer is not available";
    growth::RecordCampaignsManagerError(
        growth::CampaignsManagerError::kTrackerNotAvailableInSession);
    return false;
  }

  UpdateConfig(params);
  return tracker->WouldTriggerHelpUI(feature_engagement::kIPHGrowthFramework);
}

signin::IdentityManager* CampaignsManagerClientImpl::GetIdentityManager()
    const {
  return IdentityManagerFactory::GetForProfile(GetProfile());
}

void CampaignsManagerClientImpl::OnReadyToLogImpression(
    int campaign_id,
    std::optional<int> group_id,
    bool should_log_cros_events) {
  // Records impression UMA metrics.
  // TODO: b/348495965 - Verify group metrics when ready.
  RecordImpression(campaign_id, should_log_cros_events);
  RecordImpressionEvents(campaign_id, group_id);
}

void CampaignsManagerClientImpl::OnDismissed(int campaign_id,
                                             std::optional<int> group_id,
                                             bool should_mark_dismissed,
                                             bool should_log_cros_events) {
  // Records dismissal UMA metrics.
  // TODO: b/348495965 - Verify group metrics when ready.
  RecordDismissed(campaign_id, should_log_cros_events);

  if (!should_mark_dismissed) {
    return;
  }

  RecordDismissalEvents(campaign_id, group_id);
}

void CampaignsManagerClientImpl::OnButtonPressed(int campaign_id,
                                                 std::optional<int> group_id,
                                                 CampaignButtonId button_id,
                                                 bool should_mark_dismissed,
                                                 bool should_log_cros_events) {
  // TODO: b/348495965 - Verify group metrics when ready.
  RecordButtonPressed(campaign_id, button_id, should_log_cros_events);

  if (!should_mark_dismissed) {
    return;
  }

  // Notify `kDismissed` event to the Feature Engagement framework. This event
  // will be stored and could be used later.
  switch (button_id) {
    case CampaignButtonId::kPrimary:
    case CampaignButtonId::kSecondary:
    case CampaignButtonId::kClose:
      // Primary, Secondary and close button press will treated as user
      // dismissal.
      RecordDismissalEvents(campaign_id, group_id);
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
    // TODO - b/365582608: Add error metrics.
    CAMPAIGNS_LOG(ERROR) << "Failed to download campaigns component. Error: "
                         << static_cast<int>(error);
    std::move(loaded_callback).Run(std::nullopt);
    return;
  }

  std::move(loaded_callback).Run(path);
}

void CampaignsManagerClientImpl::OnTrackerInitialized(
    growth::OnTrackerInitializedCallback callback,
    bool init_success) {
  std::move(callback).Run(init_success);
}

void CampaignsManagerClientImpl::UpdateConfig(
    const std::map<std::string, std::string>& params) {
  auto* tracker =
      feature_engagement::TrackerFactory::GetForBrowserContext(GetProfile());
  if (!tracker || !tracker->IsInitialized()) {
    CAMPAIGNS_LOG(ERROR) << "Feature Engagement tracer is not available";
    growth::RecordCampaignsManagerError(
        growth::CampaignsManagerError::kTrackerNotAvailableInSession);
    return;
  }

  config_provider_.SetConfig(params);
  tracker->UpdateConfig(feature_engagement::kIPHGrowthFramework,
                        &config_provider_);
}

void CampaignsManagerClientImpl::RecordImpressionEvents(
    int campaign_id,
    std::optional<int> group_id) {
  campaigns_manager_->RecordEvent(GetEventName(
      growth::CampaignEvent::kImpression, base::NumberToString(campaign_id)));

  if (group_id) {
    campaigns_manager_->RecordEvent(
        GetEventName(growth::CampaignEvent::kGroupImpression,
                     base::NumberToString(group_id.value())));
  }
}

void CampaignsManagerClientImpl::RecordDismissalEvents(
    int campaign_id,
    std::optional<int> group_id) {
  campaigns_manager_->RecordEvent(GetEventName(
      growth::CampaignEvent::kDismissed, base::NumberToString(campaign_id)));

  if (group_id) {
    campaigns_manager_->RecordEvent(
        GetEventName(growth::CampaignEvent::kGroupDismissed,
                     base::NumberToString(group_id.value())));
  }
}
