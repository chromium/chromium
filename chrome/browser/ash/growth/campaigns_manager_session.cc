// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/growth/campaigns_manager_session.h"

#include <optional>
#include <string>
#include <string_view>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "base/check.h"
#include "base/check_is_test.h"
#include "base/logging.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/apps/app_service/app_service_proxy_ash.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/ownership/owner_settings_service_ash.h"
#include "chrome/browser/ash/ownership/owner_settings_service_ash_factory.h"
#include "chrome/browser/ash/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chromeos/ash/components/growth/action_performer.h"
#include "chromeos/ash/components/growth/campaigns_logger.h"
#include "chromeos/ash/components/growth/campaigns_manager.h"
#include "chromeos/ash/components/growth/campaigns_model.h"
#include "chromeos/ash/components/growth/campaigns_utils.h"
#include "chromeos/ash/components/growth/growth_metrics.h"
#include "components/account_id/account_id.h"
#include "components/app_constants/constants.h"
#include "components/services/app_service/public/cpp/app_registry_cache_wrapper.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/session_manager/core/session_manager.h"
#include "components/session_manager/session_manager_types.h"
#include "content/public/browser/web_contents.h"

namespace {

CampaignsManagerSession* g_instance = nullptr;

Profile* g_profile_for_testing = nullptr;

// The time to trigger delayed campaigns.
constexpr base::TimeDelta kTimeToTriggerDelayedCampaigns = base::Minutes(5);

Profile* GetProfile() {
  if (g_profile_for_testing) {
    return g_profile_for_testing;
  }

  return ProfileManager::GetActiveUserProfile();
}

bool IsEligible() {
  Profile* profile = GetProfile();

  if (!profile) {
    // Records metrics when profile is nullptr.
    // TODO: b/367998596 - Change this to CHECK(profile).
    // In the test ExtensionCrxInstallerTest.KioskOnlyTest, the call sequences
    // are this:
    // 1. CampaignsManagerSession::OnSessionStateChanged().
    // 2. The IsEligible() returns true, the code continues.
    // 3. Add a callback when the device owner is set: OnOwnershipDetermined().
    // 4. In OnOwnershipDetermined(), load the campaigns.
    // 5. When the campaigns are loaded, call MaybeTriggerRuntimeCampaigns().
    // 6. Which calls IsEligible() again, and hits the CHECK(profile).
    // The profile becames nullptr during steps 2-6.
    growth::RecordCampaignsManagerError(
        growth::CampaignsManagerError::kNullptrProfile);
    return false;
  }

  // TODO(b/320789239): Enable for unicorn users.
  if (profile->GetProfilePolicyConnector()->IsManaged()) {
    // Only enabled for consumer session for now.
    // Demo Mode session is handled separately at `DemoSession`.
    return false;
  }

  // TODO: b/341328441 - Enable Growth Framework on guest mode.
  if (profile->IsGuestSession()) {
    return false;
  }

  return true;
}

bool IsWebBrowserAppId(std::string_view app_id) {
  return app_id == app_constants::kChromeAppId ||
         app_id == app_constants::kAshDebugBrowserAppId ||
         app_id == app_constants::kLacrosAppId;
}

bool IsAppVisible(const apps::InstanceUpdate& update) {
  return update.State() & apps::InstanceState::kVisible;
}

bool IsAppActiveAndVisible(const apps::InstanceUpdate& update) {
  return IsAppVisible(update) &&
         (update.State() & apps::InstanceState::kActive);
}

std::optional<growth::ActionType> GetActionTypeBySlot(growth::Slot slot) {
  if (slot == growth::Slot::kNotification) {
    return growth::ActionType::kShowNotification;
  }

  if (slot == growth::Slot::kNudge) {
    return growth::ActionType::kShowNudge;
  }

  return std::nullopt;
}

std::string_view GetAppGroupId() {
  auto* campaigns_manager = growth::CampaignsManager::Get();
  CHECK(campaigns_manager);

  const auto app_id = campaigns_manager->GetOpenedAppId();
  return IsWebBrowserAppId(app_id)
             ? growth::GetAppGroupId(campaigns_manager->GetActiveUrl())
             : growth::GetAppGroupId(app_id);
}

base::TimeDelta GetTimeToTriggerDelayedCampaigns() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(
          ash::switches::kGrowthCampaignsDelayedTriggerTimeInSecs)) {
    const auto& value = command_line->GetSwitchValueASCII(
        ash::switches::kGrowthCampaignsDelayedTriggerTimeInSecs);

    double seconds;
    CHECK(base::StringToDouble(value, &seconds));
    return base::Seconds(seconds);
  }

  return kTimeToTriggerDelayedCampaigns;
}

void MaybeTriggerSlot(growth::Slot slot) {
  const auto action_type = GetActionTypeBySlot(slot);
  if (!action_type) {
    CAMPAIGNS_LOG(ERROR) << "Invalid: no supported action type for slot "
                         << static_cast<int>(action_type.value());
    return;
  }

  auto* campaigns_manager = growth::CampaignsManager::Get();
  CHECK(campaigns_manager);

  auto* campaign = campaigns_manager->GetCampaignBySlot(slot);
  if (!campaign) {
    // No campaign matched.
    return;
  }

  auto campaign_id = growth::GetCampaignId(campaign);
  if (!campaign_id) {
    CAMPAIGNS_LOG(ERROR) << "Invalid: Missing campaign id.";
    return;
  }

  const auto* payload = growth::GetPayloadBySlot(campaign, slot);
  if (!payload) {
    // No payload for the targeted slot. It is valid for counterfactual control.
    return;
  }

  campaigns_manager->PerformAction(campaign_id.value(),
                                   growth::GetCampaignGroupId(campaign),
                                   action_type.value(), payload);
}

void MaybeTriggerRuntimeCampaigns(growth::TriggerType type,
                                  std::string_view event = std::string_view()) {
  // We need this for trigger points that are not managed by
  // `CampaignsManagerSession`, e.g: `MaybeTriggerCampaignsOnEvent()`.
  if (!IsEligible()) {
    return;
  }

  auto* campaigns_manager = growth::CampaignsManager::Get();
  CHECK(campaigns_manager);

  growth::Trigger trigger(type);
  trigger.events = {std::string(event)};
  campaigns_manager->SetTrigger(std::move(trigger));

  MaybeTriggerSlot(growth::Slot::kNudge);
  MaybeTriggerSlot(growth::Slot::kNotification);
}

void MaybeTriggerCampaignsWhenCampaignsLoaded() {
  if (!ash::features::IsGrowthCampaignsTriggerAtLoadComplete()) {
    return;
  }

  MaybeTriggerRuntimeCampaigns(growth::TriggerType::kCampaignsLoaded);
}

void MaybeTriggerDelayedCampaigns() {
  MaybeTriggerRuntimeCampaigns(growth::TriggerType::kDelayedOneShotTimer);
}

// The app_id is optional and only required if the browser type is app.
content::WebContents* FindActiveWebContent(
    const Profile* profile,
    Browser::Type browser_type,
    const webapps::AppId& app_id = std::string()) {
  for (Browser* browser : BrowserList::GetInstance()->OrderedByActivation()) {
    if (browser->IsAttemptingToCloseBrowser() || browser->IsBrowserClosing()) {
      continue;
    }
    if (browser->type() != browser_type) {
      continue;
    }
    if (browser->profile() != profile) {
      continue;
    }
    // For web app type, it must match the app_id.
    if (browser_type == Browser::TYPE_APP &&
        !web_app::AppBrowserController::IsForWebApp(browser, app_id)) {
      continue;
    }

    const auto* tab_strip_model = browser->tab_strip_model();
    if (!tab_strip_model) {
      CAMPAIGNS_LOG(ERROR) << "No tab_strip_model.";
      continue;
    }

    auto* active_web_contents = tab_strip_model->GetActiveWebContents();
    if (!active_web_contents) {
      CAMPAIGNS_LOG(ERROR) << "No active web contents.";
      continue;
    }

    return active_web_contents;
  }
  return nullptr;
}

const GURL FindActiveWebAppUrl(Profile* profile, const webapps::AppId& app_id) {
  auto* active_web_contents =
      FindActiveWebContent(profile, Browser::TYPE_APP, app_id);
  if (!active_web_contents) {
    return GURL::EmptyGURL();
  }
  return active_web_contents->GetURL();
}

content::WebContents* FindActiveTabWebContent(Profile* profile) {
  return FindActiveWebContent(profile, Browser::TYPE_NORMAL);
}

std::optional<apps::AppType> GetAppType(const std::string& app_id) {
  auto account_id =
      ash::Shell::Get()->session_controller()->GetActiveAccountId();
  apps::AppRegistryCache* cache =
      apps::AppRegistryCacheWrapper::Get().GetAppRegistryCache(account_id);
  if (!cache) {
    return std::nullopt;
  }
  return cache->GetAppType(app_id);
}

// Returns current active browser. If there's no active browser, return nullptr.
Browser* GetActiveBrowser() {
  for (Browser* browser : BrowserList::GetInstance()->OrderedByActivation()) {
    if (browser->profile()->IsOffTheRecord() ||
        !browser->window()->IsVisible()) {
      continue;
    }

    if (browser->window()->IsActive()) {
      return browser;
    }
  }
  return nullptr;
}

bool IsSystemWebApp(Profile* profile, const webapps::AppId& app_id) {
  ash::SystemWebAppManager* swa_manager =
      ash::SystemWebAppManager::Get(profile);
  if (!swa_manager) {
    CHECK_IS_TEST();
    return false;
  }
  return swa_manager->IsSystemWebApp(app_id);
}

bool HasValidPwaBrowserForAppId(const std::string& app_id) {
  auto* browser = GetActiveBrowser();

  if (!browser) {
    CAMPAIGNS_LOG(ERROR) << "No browser window";
    return false;
  }

  if (browser->type() != Browser::TYPE_APP) {
    CAMPAIGNS_LOG(ERROR) << "Not pwa browser type";
    return false;
  }

  if (!web_app::AppBrowserController::IsForWebApp(browser, app_id)) {
    CAMPAIGNS_LOG(ERROR) << "Browser belongs to a different app";
    return false;
  }

  return true;
}

void SetCampaignManagerPrefService(Profile* profile) {
  auto* campaigns_manager = growth::CampaignsManager::Get();
  CHECK(campaigns_manager);

  if (!profile) {
    campaigns_manager->SetPrefs(nullptr);
    return;
  }
  campaigns_manager->SetPrefs(profile->GetPrefs());
}

}  // namespace

// static
CampaignsManagerSession* CampaignsManagerSession::Get() {
  return g_instance;
}

CampaignsManagerSession::CampaignsManagerSession() {
  CHECK_EQ(g_instance, nullptr);
  g_instance = this;

  // SessionManager may be unset in unit tests.
  auto* session_manager = session_manager::SessionManager::Get();
  if (session_manager) {
    session_manager_observation_.Observe(session_manager);
    OnSessionStateChanged();
  }
}

CampaignsManagerSession::~CampaignsManagerSession() {
  CHECK_EQ(g_instance, this);
  g_instance = nullptr;
  g_profile_for_testing = nullptr;
  SetCampaignManagerPrefService(nullptr);
}

void CampaignsManagerSession::OnSessionStateChanged() {
  // Stop the timer to avoid triggering campaigns if the session is not active.
  if (delayed_timer_.IsRunning()) {
    delayed_timer_.Stop();
  }

  if (session_manager::SessionManager::Get()->session_state() ==
      session_manager::SessionState::LOCKED) {
    if (scoped_observation_.IsObserving()) {
      scoped_observation_.Reset();
    }
    return;
  }

  if (session_manager::SessionManager::Get()->session_state() !=
      session_manager::SessionState::ACTIVE) {
    // Loads campaigns at session active only.
    return;
  }

  if (!IsEligible()) {
    return;
  }

  SetCampaignManagerPrefService(GetProfile());

  ash::OwnerSettingsServiceAsh* service =
      ash::OwnerSettingsServiceAshFactory::GetForBrowserContext(GetProfile());
  if (service) {
    service->IsOwnerAsync(
        base::BindOnce(&CampaignsManagerSession::OnOwnershipDetermined,
                       weak_ptr_factory_.GetWeakPtr()));
  } else {
    // TODO: b/338085893 - Add metric to track the case that settings service
    // is not available at this point.
    CAMPAIGNS_LOG(ERROR)
        << "Owner settings service unavailable for the profile.";
  }
}

void CampaignsManagerSession::OnInstanceUpdate(
    const apps::InstanceUpdate& update) {
  // No state changes. Ignore the update.
  if (!update.StateChanged()) {
    return;
  }

  if (update.IsDestruction()) {
    HandleAppInstanceDestruction(update);
    return;
  }

  auto app_id = update.AppId();
  auto app_type = GetAppType(app_id);
  if (!app_type) {
    CAMPAIGNS_LOG(ERROR) << "Invalid app type for " << app_id;
    return;
  }

  switch (app_type.value()) {
    case apps::AppType::kUnknown:
      // e.g Ash debug browser.
      break;
    case apps::AppType::kStandaloneBrowser:
    case apps::AppType::kChromeApp:
      HandleWebBrowserInstanceUpdate(update);
      break;
    case apps::AppType::kWeb:
      if (IsSystemWebApp(GetProfile(), app_id)) {
        // Active browser is not available for the SWA case, so we handle
        // it as a regular app open.
        HandleAppInstanceUpdate(update);
        break;
      }
      HandlePwaInstanceUpdate(update);
      break;
    case apps::AppType::kArc:
      HandleArcInstanceUpdate(update);
      break;
    default:
      HandleAppInstanceUpdate(update);
      break;
  }
}

void CampaignsManagerSession::OnInstanceRegistryWillBeDestroyed(
    apps::InstanceRegistry* cache) {
  if (scoped_observation_.GetSource() == cache) {
    scoped_observation_.Reset();
  }
}

void CampaignsManagerSession::MaybeTriggerCampaignsOnEvent(
    std::string_view event) {
  if (!ash::features::IsGrowthCampaignsTriggerByEventEnabled()) {
    return;
  }

  MaybeTriggerRuntimeCampaigns(growth::TriggerType::kEvent, event);
}

void CampaignsManagerSession::PrimaryPageChanged(
    const content::WebContents* web_contents) {
  auto* campaigns_manager = growth::CampaignsManager::Get();
  CHECK(campaigns_manager);

  auto app_id = campaigns_manager->GetOpenedAppId();
  if (!IsWebBrowserAppId(app_id)) {
    return;
  }

  // Skip triggering campaign if this Primary Page Changed event happens on an
  // inactive tab (i.e. `web_contents` is not the active tab `web_contents`).
  // For example:
  // 1. Load `url1` in "tab 1".
  // 2. While `url1` is loading, open a "tab 2" and load the same URL `url1`
  // 3. The nudge triggered twice - one by the inactive "tab 1" and one by the
  // active "tab 2".
  auto* active_tab_web_contents = FindActiveTabWebContent(GetProfile());
  if (active_tab_web_contents != web_contents) {
    return;
  }

  auto url = active_tab_web_contents->GetURL();
  campaigns_manager->SetActiveUrl(url);
  MaybeTriggerCampaignsWhenAppOpened();
}

void CampaignsManagerSession::SetProfileForTesting(Profile* profile) {
  g_profile_for_testing = profile;
}

void CampaignsManagerSession::SetupWindowObserver() {
  // Tests might not go through LOCKED state and `scoped_observation_` might
  // be observing.
  if (scoped_observation_.IsObserving()) {
    return;
  }

  auto* profile = GetProfile();
  // Some test profiles will not have AppServiceProxy.
  if (!apps::AppServiceProxyFactory::IsAppServiceAvailableForProfile(profile)) {
    return;
  }
  auto* proxy = apps::AppServiceProxyFactory::GetForProfile(profile);
  CHECK(proxy);
  scoped_observation_.Observe(&proxy->InstanceRegistry());
}

void CampaignsManagerSession::OnOwnershipDetermined(bool is_user_owner) {
  auto* campaigns_manager = growth::CampaignsManager::Get();
  CHECK(campaigns_manager);

  campaigns_manager->SetIsUserOwner(is_user_owner);

  campaigns_manager->LoadCampaigns(
      base::BindOnce(&CampaignsManagerSession::OnLoadCampaignsCompleted,
                     weak_ptr_factory_.GetWeakPtr()));
}

void CampaignsManagerSession::OnLoadCampaignsCompleted() {
  if (ash::features::IsGrowthCampaignsTriggerByAppOpenEnabled()) {
    SetupWindowObserver();
  }

  MaybeTriggerCampaignsWhenCampaignsLoaded();
  StartDelayedTimer();
}

void CampaignsManagerSession::StartDelayedTimer() {
  delayed_timer_.Start(FROM_HERE, GetTimeToTriggerDelayedCampaigns(),
                       base::BindOnce(&MaybeTriggerDelayedCampaigns));
}

void CampaignsManagerSession::CacheAppOpenContext(
    const apps::InstanceUpdate& update,
    const GURL& url) {
  auto* campaigns_manager = growth::CampaignsManager::Get();
  CHECK(campaigns_manager);

  auto app_id = update.AppId();
  campaigns_manager->SetOpenedApp(app_id);
  campaigns_manager->SetActiveUrl(url);
  opened_window_ = update.Window();
}

void CampaignsManagerSession::ClearAppOpenContext() {
  auto* campaigns_manager = growth::CampaignsManager::Get();
  CHECK(campaigns_manager);

  campaigns_manager->SetOpenedApp(std::string());
  campaigns_manager->SetActiveUrl(GURL::EmptyGURL());
  opened_window_ = nullptr;
}

void CampaignsManagerSession::HandleAppInstanceUpdate(
    const apps::InstanceUpdate& update) {
  if (!update.IsCreation()) {
    return;
  }
  CacheAppOpenContext(update, GURL::EmptyGURL());
  MaybeTriggerCampaignsWhenAppOpened();
}

void CampaignsManagerSession::HandleArcInstanceUpdate(
    const apps::InstanceUpdate& update) {
  // When an Arc app is opened, the instance state is `Started & Running &
  // Visible`. When an Arc app is closed, there are a sequence of
  // `Destroy`-`Started & Running`-`Destroy` state update as reported in
  // b/342489300. We skip the `Started & Running` state received after
  // closing the app.
  if (!update.IsCreation() || !IsAppVisible(update)) {
    return;
  }
  CacheAppOpenContext(update, GURL::EmptyGURL());
  MaybeTriggerCampaignsWhenAppOpened();
}

void CampaignsManagerSession::HandleWebBrowserInstanceUpdate(
    const apps::InstanceUpdate& update) {
  auto app_id = update.AppId();

  // Non web browser app such as text editor will be handled like default
  // app type.
  if (!IsWebBrowserAppId(app_id)) {
    CAMPAIGNS_LOG(ERROR) << "Not a web broswer: " << app_id;
    HandleAppInstanceUpdate(update);
    return;
  }

  if (!ash::features::IsGrowthCampaignsTriggerByBrowserEnabled()) {
    return;
  }

  // For browser app, the user can open a new tab or switch to an existing
  // tab before navigating to an url. So, it is not limited to a creation event
  // like other app types. In any case, the browser should be active and visible
  // when the user starts inserting the url.
  if (!IsAppActiveAndVisible(update)) {
    return;
  }

  // Caches the current app id and browser window and clears the url since it
  // isn't relevant to url navigation action. Caching url and triggering the
  // campaigns is deferred to PrimaryPageChanged when url navigation actually
  // happens.
  CacheAppOpenContext(update, GURL::EmptyGURL());
}

void CampaignsManagerSession::HandlePwaInstanceUpdate(
    const apps::InstanceUpdate& update) {
  auto* campaigns_manager = growth::CampaignsManager::Get();
  CHECK(campaigns_manager);

  if (!update.IsCreation()) {
    return;
  }

  // When a user navigates to the url from web browser, an instance update with
  // PWA id is also sent as reported in b/342489221. This check will skip
  // instance update with chrome browser window and PWA app id, which is a
  // mismatch.
  auto app_id = update.AppId();
  if (!HasValidPwaBrowserForAppId(app_id)) {
    CAMPAIGNS_LOG(ERROR) << "Invalid web app browser";
    return;
  }

  CacheAppOpenContext(update, FindActiveWebAppUrl(GetProfile(), app_id));
  MaybeTriggerCampaignsWhenAppOpened();
}

void CampaignsManagerSession::HandleAppInstanceDestruction(
    const apps::InstanceUpdate& update) {
  // TODO: b/330409492 - Maybe trigger a campaign when app is about to be
  // destroyed.
  auto* campaigns_manager = growth::CampaignsManager::Get();
  CHECK(campaigns_manager);

  if (update.AppId() != campaigns_manager->GetOpenedAppId()) {
    return;
  }
  ClearAppOpenContext();
}

void CampaignsManagerSession::MaybeTriggerCampaignsWhenAppOpened() {
  auto* campaigns_manager = growth::CampaignsManager::Get();
  CHECK(campaigns_manager);

  // If `app_group_id` is defined, record the `event` and trigger campaigns
  // based on the trigger `event`. An `app_group_id` is used to configurate how
  // often, i.e. the interval, to show the nudges.
  if (const std::string_view app_group_id = GetAppGroupId();
      !app_group_id.empty()) {
    campaigns_manager->RecordEvent(
        GetEventName(growth::CampaignEvent::kEvent, app_group_id));
    MaybeTriggerCampaignsOnEvent(app_group_id);
  }

  if (!ash::features::IsGrowthCampaignsTriggerByAppOpenEnabled()) {
    return;
  }

  MaybeTriggerRuntimeCampaigns(growth::TriggerType::kAppOpened);
}
