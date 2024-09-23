// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/browser_service_lacros.h"

#include "base/check.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/statistics_recorder.h"
#include "base/ranges/algorithm.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/arc/arc_web_contents_data.h"
#include "chrome/browser/chromeos/network/network_portal_signin_window.h"
#include "chrome/browser/feedback/feedback_dialog_utils.h"
#include "chrome/browser/lacros/app_mode/kiosk_session_service_lacros.h"
#include "chrome/browser/lacros/browser_launcher.h"
#include "chrome/browser/lacros/feedback_util.h"
#include "chrome/browser/lacros/profile_loader.h"
#include "chrome/browser/lacros/system_logs/lacros_system_log_fetcher.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/sessions/exit_type_service.h"
#include "chrome/browser/sessions/session_restore.h"
#include "chrome/browser/sessions/session_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/profiles/profile_picker.h"
#include "chrome/browser/ui/scoped_tabbed_browser_displayer.h"
#include "chrome/browser/ui/startup/first_run_service.h"
#include "chrome/browser/ui/startup/startup_browser_creator.h"
#include "chrome/browser/ui/startup/startup_tab.h"
#include "chrome/browser/ui/views/tabs/tab_scrubber_chromeos.h"
#include "chrome/browser/ui/webui/tab_strip/tab_strip_ui_util.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/api/tabs.h"
#include "chrome/common/webui_url_constants.h"
#include "chromeos/crosapi/mojom/crosapi.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#include "chromeos/startup/browser_params_proxy.h"
#include "components/feedback/feedback_common.h"
#include "components/feedback/feedback_report.h"
#include "components/feedback/feedback_util.h"
#include "components/feedback/system_logs/system_logs_fetcher.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/sessions/content/session_tab_helper.h"
#include "content/public/browser/browser_thread.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "ui/base/mojom/window_show_state.mojom.h"
#include "ui/display/scoped_display_for_new_windows.h"
#include "ui/platform_window/platform_window.h"
#include "ui/views/widget/desktop_aura/desktop_window_tree_host_lacros.h"
#include "url/gurl.h"

namespace {

constexpr char kHistogramsFilename[] = "lacros_histograms.txt";

std::string GetCompressedHistograms() {
  std::string histograms =
      base::StatisticsRecorder::ToJSON(base::JSON_VERBOSITY_LEVEL_FULL);
  std::optional<std::string> compressed_histograms =
      feedback_util::ZipString(base::FilePath(kHistogramsFilename), histograms);
  if (!compressed_histograms.has_value()) {
    LOG(ERROR) << "Failed to compress lacros histograms.";
    return std::string();
  }
  return compressed_histograms.value();
}

NavigateParams::PathBehavior ConvertPathBehavior(
    crosapi::mojom::OpenUrlParams_SwitchToTabPathBehavior path_behavior) {
  switch (path_behavior) {
    case crosapi::mojom::OpenUrlParams_SwitchToTabPathBehavior::kRespect:
      return NavigateParams::RESPECT;
    case crosapi::mojom::OpenUrlParams_SwitchToTabPathBehavior::kIgnore:
      return NavigateParams::IGNORE_AND_NAVIGATE;
  }
}

// Find the browser containing the tab with ID |tab_id_str| or nullptr if none
// is found within the given |profile|.
Browser* FindBrowserWithTabId(const std::string& tab_id_str) {
  if (tab_id_str.empty())
    return nullptr;

  int tab_id = -1;
  if (!base::StringToInt(tab_id_str, &tab_id))
    return nullptr;

  if (tab_id == extensions::api::tabs::TAB_ID_NONE)
    return nullptr;

  for (Browser* target_browser : *BrowserList::GetInstance()) {
    TabStripModel* target_tab_strip = target_browser->tab_strip_model();
    for (int i = 0; i < target_tab_strip->count(); ++i) {
      content::WebContents* target_contents =
          target_tab_strip->GetWebContentsAt(i);
      if (sessions::SessionTabHelper::IdForTab(target_contents).id() ==
          tab_id) {
        return target_browser;
      }
    }
  }

  return nullptr;
}

// The return value indicates whether the profile picker was shown.
bool ShowProfilePickerIfNeeded(bool incognito,
                               std::optional<int64_t> target_display_id) {
  if (StartupProfileModeFromReason(ProfilePicker::GetStartupModeReason()) ==
          StartupProfileMode::kProfilePicker &&
      chrome::GetTotalBrowserCount() == 0 && !incognito) {
    // Profile picker does not support passing through the incognito param. It
    // also does not support passing through the
    // `should_trigger_session_restore` param but that's very common (left
    // clicking the launcher icon) so we can't skip the picker in this case. The
    // default behavior for the first browser window supports session restore,
    // additional windows are opened blank and thus it works reasonably well for
    // BrowserServiceLacros.
    std::optional<display::ScopedDisplayForNewWindows> scoped_display;
    if (target_display_id.has_value()) {
      scoped_display.emplace(target_display_id.value());
    }
    ProfilePicker::Show(ProfilePicker::Params::FromEntryPoint(
        ProfilePicker::EntryPoint::kNewSessionOnExistingProcess));
    return true;
  }
  return false;
}

}  // namespace

// A struct to keep the pending OpenUrl task.
struct BrowserServiceLacros::PendingOpenUrl {
  raw_ptr<Profile> profile;
  GURL url;
  crosapi::mojom::OpenUrlParamsPtr params;
  OpenUrlCallback callback;
};

BrowserServiceLacros::BrowserServiceLacros() {
  session_restored_subscription_ =
      SessionRestore::RegisterOnSessionRestoredCallback(
          base::BindRepeating(&BrowserServiceLacros::OnSessionRestored,
                              weak_ptr_factory_.GetWeakPtr()));

  auto* lacros_service = chromeos::LacrosService::Get();
  const auto* init_params = chromeos::BrowserParamsProxy::Get();

  if (init_params->InitialKeepAlive() ==
      crosapi::mojom::BrowserInitParams::InitialKeepAlive::kUnknown) {
    // ash-chrome is too old, so for backward compatibility fallback to the old
    // way, which is "if launched with kDoNotOpenWindow, run the lacro process
    // on background, and reset the state when a Browser instance is created."
    // Thus, if a user creates a browser window then close it, Lacros is
    // terminated, but ash-chrome has responsibility to re-launch it soon.
    if (init_params->InitialBrowserAction() ==
        crosapi::mojom::InitialBrowserAction::kDoNotOpenWindow) {
      keep_alive_ = std::make_unique<ScopedKeepAlive>(
          KeepAliveOrigin::BROWSER_PROCESS_LACROS,
          KeepAliveRestartOption::ENABLED);
      BrowserList::AddObserver(this);
    }
  } else {
    if (init_params->InitialKeepAlive() ==
        crosapi::mojom::BrowserInitParams::InitialKeepAlive::kEnabled) {
      keep_alive_ = std::make_unique<ScopedKeepAlive>(
          KeepAliveOrigin::BROWSER_PROCESS_LACROS,
          KeepAliveRestartOption::ENABLED);
    }
  }

  if (lacros_service->IsAvailable<crosapi::mojom::BrowserServiceHost>()) {
    lacros_service->GetRemote<crosapi::mojom::BrowserServiceHost>()
        ->AddBrowserService(receiver_.BindNewPipeAndPassRemoteWithVersion());
  }
}

BrowserServiceLacros::~BrowserServiceLacros() {
  BrowserList::RemoveObserver(this);
}

void BrowserServiceLacros::REMOVED_0() {
  NOTIMPLEMENTED();
}

void BrowserServiceLacros::REMOVED_2() {
  NOTIMPLEMENTED();
}

void BrowserServiceLacros::REMOVED_7(bool should_trigger_session_restore,
                                     base::OnceCallback<void()> callback) {
  NOTIMPLEMENTED();
}

void BrowserServiceLacros::REMOVED_16(
    base::flat_map<policy::PolicyNamespace, std::vector<uint8_t>> policy) {
  NOTIMPLEMENTED();
}

void BrowserServiceLacros::NewWindow(bool incognito,
                                     bool should_trigger_session_restore,
                                     int64_t target_display_id,
                                     std::optional<uint64_t> profile_id,
                                     NewWindowCallback callback) {
  if (g_browser_process->IsShuttingDown()) {
    std::move(callback).Run(crosapi::mojom::CreationResult::kBrowserShutdown);
    return;
  }

  if (ShowProfilePickerIfNeeded(incognito, target_display_id)) {
    std::move(callback).Run(
        crosapi::mojom::CreationResult::kBrowserWindowUnavailable);
    return;
  }

  if (profile_id.has_value()) {
    LoadProfileWithId(
        base::BindOnce(&BrowserServiceLacros::NewWindowWithProfile,
                       weak_ptr_factory_.GetWeakPtr(), incognito,
                       should_trigger_session_restore, target_display_id,
                       std::move(callback)),
        /*can_trigger_fre=*/true, profile_id.value());
    return;
  }

  LoadMainProfile(base::BindOnce(&BrowserServiceLacros::NewWindowWithProfile,
                                 weak_ptr_factory_.GetWeakPtr(), incognito,
                                 should_trigger_session_restore,
                                 target_display_id, std::move(callback)),
                  /*can_trigger_fre=*/true);
}

void BrowserServiceLacros::NewFullscreenWindow(
    const GURL& url,
    int64_t target_display_id,
    NewFullscreenWindowCallback callback) {
  if (g_browser_process->IsShuttingDown()) {
    std::move(callback).Run(crosapi::mojom::CreationResult::kBrowserShutdown);
    return;
  }
  LoadMainProfile(
      base::BindOnce(&BrowserServiceLacros::NewFullscreenWindowWithProfile,
                     weak_ptr_factory_.GetWeakPtr(), url, target_display_id,
                     std::move(callback)),
      /*can_trigger_fre=*/false);
}

void BrowserServiceLacros::NewGuestWindow(int64_t target_display_id,
                                          NewGuestWindowCallback callback) {
  if (g_browser_process->IsShuttingDown()) {
    std::move(callback).Run(crosapi::mojom::CreationResult::kBrowserShutdown);
    return;
  }

  display::ScopedDisplayForNewWindows scoped(target_display_id);

  if (!profiles::IsGuestModeEnabled()) {
    std::move(callback).Run(
        crosapi::mojom::CreationResult::kBrowserWindowUnavailable);
    return;
  }

  profiles::SwitchToGuestProfile();
  std::move(callback).Run(crosapi::mojom::CreationResult::kSuccess);
}

void BrowserServiceLacros::NewWindowForDetachingTab(
    const std::u16string& tab_id,
    const std::u16string& group_id,
    NewWindowForDetachingTabCallback callback) {
  if (g_browser_process->IsShuttingDown()) {
    std::move(callback).Run(crosapi::mojom::CreationResult::kBrowserShutdown,
                            std::string());
    return;
  }

  auto* browser = FindBrowserWithTabId(base::UTF16ToUTF8(tab_id));
  if (!browser) {
    browser = tab_strip_ui::GetBrowserWithGroupId(/*profile=*/nullptr,
                                                  base::UTF16ToUTF8(group_id));
  }

  if (!browser) {
    std::move(callback).Run(
        crosapi::mojom::CreationResult::kBrowserWindowUnavailable,
        std::string());
    return;
  }

  NewWindowForDetachingTabWithProfile(tab_id, group_id, std::move(callback),
                                      browser->profile());
}

void BrowserServiceLacros::NewTab(std::optional<uint64_t> profile_id,
                                  NewTabCallback callback) {
  if (g_browser_process->IsShuttingDown()) {
    std::move(callback).Run(crosapi::mojom::CreationResult::kBrowserShutdown);
    return;
  }

  // TODO: crbug.com/333312496 - Update newtab to pass the target display id
  // through the crosapi.
  if (ShowProfilePickerIfNeeded(false, std::nullopt)) {
    std::move(callback).Run(
        crosapi::mojom::CreationResult::kBrowserWindowUnavailable);
    return;
  }

  if (profile_id.has_value()) {
    LoadProfileWithId(
        base::BindOnce(&BrowserServiceLacros::LaunchOrNewTabWithProfile,
                       weak_ptr_factory_.GetWeakPtr(),
                       /*should_trigger_session_restore=*/false, -1,
                       std::move(callback),
                       /*is_new_tab=*/true),
        /*can_trigger_fre=*/true, profile_id.value());
    return;
  }

  LoadMainProfile(
      base::BindOnce(&BrowserServiceLacros::LaunchOrNewTabWithProfile,
                     weak_ptr_factory_.GetWeakPtr(),
                     /*should_trigger_session_restore=*/false, -1,
                     std::move(callback),
                     /*is_new_tab=*/true),
      /*can_trigger_fre=*/true);
}

void BrowserServiceLacros::Launch(int64_t target_display_id,
                                  std::optional<uint64_t> profile_id,
                                  LaunchCallback callback) {
  if (g_browser_process->IsShuttingDown()) {
    std::move(callback).Run(crosapi::mojom::CreationResult::kBrowserShutdown);
    return;
  }

  if (ShowProfilePickerIfNeeded(false, target_display_id)) {
    std::move(callback).Run(
        crosapi::mojom::CreationResult::kBrowserWindowUnavailable);
    return;
  }

  if (profile_id.has_value()) {
    LoadProfileWithId(
        base::BindOnce(&BrowserServiceLacros::LaunchOrNewTabWithProfile,
                       weak_ptr_factory_.GetWeakPtr(),
                       /*should_trigger_session_restore=*/true,
                       target_display_id, std::move(callback),
                       /*is_new_tab=*/false),
        /*can_trigger_fre=*/true, profile_id.value());
    return;
  }

  LoadMainProfile(
      base::BindOnce(&BrowserServiceLacros::LaunchOrNewTabWithProfile,
                     weak_ptr_factory_.GetWeakPtr(),
                     /*should_trigger_session_restore=*/true, target_display_id,
                     std::move(callback), /*is_new_tab=*/false),
      /*can_trigger_fre=*/true);
}

void BrowserServiceLacros::OpenUrl(const GURL& url,
                                   crosapi::mojom::OpenUrlParamsPtr params,
                                   OpenUrlCallback callback) {
  if (g_browser_process->IsShuttingDown()) {
    std::move(callback).Run(crosapi::mojom::CreationResult::kBrowserShutdown);
    return;
  }
  LoadMainProfile(base::BindOnce(&BrowserServiceLacros::OpenUrlWithProfile,
                                 weak_ptr_factory_.GetWeakPtr(), url,
                                 std::move(params), std::move(callback)),
                  /*can_trigger_fre=*/true);
}

void BrowserServiceLacros::OpenCaptivePortalSignin(const GURL& url,
                                                   OpenUrlCallback callback) {
  if (g_browser_process->IsShuttingDown()) {
    std::move(callback).Run(crosapi::mojom::CreationResult::kBrowserShutdown);
    return;
  }
  // Ensure that the main profile is loaded so that the captive portal signin
  // profile (which is derived from the main profile) can be created if needed.
  LoadMainProfile(
      base::BindOnce(&BrowserServiceLacros::OpenCaptivePortalSigninWithProfile,
                     weak_ptr_factory_.GetWeakPtr(), url, std::move(callback)),
      /*can_trigger_fre=*/true);
}

void BrowserServiceLacros::RestoreTab(RestoreTabCallback callback) {
  if (g_browser_process->IsShuttingDown()) {
    std::move(callback).Run(crosapi::mojom::CreationResult::kBrowserShutdown);
    return;
  }
  LoadMainProfile(
      base::BindOnce(&BrowserServiceLacros::RestoreTabWithProfile,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)),
      /*can_trigger_fre=*/true);
}

void BrowserServiceLacros::HandleTabScrubbing(float x_offset,
                                              bool is_fling_scroll_event) {
  TabScrubberChromeOS::GetInstance()->SynthesizedScrollEvent(
      x_offset, is_fling_scroll_event);
}

void BrowserServiceLacros::GetFeedbackData(GetFeedbackDataCallback callback) {
  DCHECK(!callback.is_null());
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Self-deleting object.
  system_logs::SystemLogsFetcher* fetcher =
      system_logs::BuildLacrosSystemLogsFetcher(/*scrub_data=*/true);
  fetcher->Fetch(base::BindOnce(&BrowserServiceLacros::OnSystemInformationReady,
                                weak_ptr_factory_.GetWeakPtr(),
                                std::move(callback)));
}

void BrowserServiceLacros::GetHistograms(GetHistogramsCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // GetCompressedHistograms calls functions marking as blocking, so it
  // can not be running on UI thread.
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(GetCompressedHistograms),
      base::BindOnce(&BrowserServiceLacros::OnGetCompressedHistograms,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void BrowserServiceLacros::GetActiveTabUrl(GetActiveTabUrlCallback callback) {
  Browser* browser = chrome::FindBrowserWithActiveWindow();
  GURL page_url;
  if (browser) {
    page_url = chrome::GetTargetTabUrl(
        browser->session_id(), browser->tab_strip_model()->active_index());
  }
  std::move(callback).Run(page_url);
}

void BrowserServiceLacros::UpdateDeviceAccountPolicy(
    const std::vector<uint8_t>& policy) {
  chromeos::LacrosService::Get()->NotifyPolicyUpdated(policy);
}

void BrowserServiceLacros::NotifyPolicyFetchAttempt() {
  chromeos::LacrosService::Get()->NotifyPolicyFetchAttempt();
}

void BrowserServiceLacros::UpdateKeepAlive(bool enabled) {
  if (g_browser_process->IsShuttingDown()) {
    return;
  }

  if (enabled == static_cast<bool>(keep_alive_))
    return;

  if (enabled) {
    keep_alive_ = std::make_unique<ScopedKeepAlive>(
        KeepAliveOrigin::BROWSER_PROCESS_LACROS,
        KeepAliveRestartOption::ENABLED);
  } else {
    keep_alive_.reset();
  }
}

void BrowserServiceLacros::OpenForFullRestore(bool skip_crash_restore) {
  if (g_browser_process->IsShuttingDown()) {
    return;
  }

  LoadMainProfile(
      base::BindOnce(&BrowserServiceLacros::OpenForFullRestoreWithProfile,
                     weak_ptr_factory_.GetWeakPtr(), skip_crash_restore),
      /*can_trigger_fre=*/true);
}

void BrowserServiceLacros::OpenProfileManager() {
  ProfilePicker::Show(ProfilePicker::Params::FromEntryPoint(
      ProfilePicker::EntryPoint::kProfileMenuManageProfiles));
}

void BrowserServiceLacros::OnSystemInformationReady(
    GetFeedbackDataCallback callback,
    std::unique_ptr<system_logs::SystemLogsResponse> sys_info) {
  base::Value::Dict system_log_entries;
  if (sys_info) {
    std::string user_email = feedback_util::GetSignedInUserEmail();
    const bool google_email = gaia::IsGoogleInternalAccountEmail(user_email);

    for (auto& it : *sys_info) {
      // We only send the list of all the crash report IDs if the user has a
      // @google.com email. We strip this here so that the system information
      // view properly reflects what we will be uploading to the server. It is
      // also stripped later on in the feedback processing for other code paths
      // that don't go through this.
      if (FeedbackCommon::IncludeInSystemLogs(it.first, google_email)) {
        system_log_entries.Set(it.first, std::move(it.second));
      }
    }
  }

  DCHECK(!callback.is_null());
  std::move(callback).Run(std::move(system_log_entries));
}

void BrowserServiceLacros::OnGetCompressedHistograms(
    GetHistogramsCallback callback,
    const std::string& compressed_histograms) {
  DCHECK(!callback.is_null());
  std::move(callback).Run(compressed_histograms);
}

void BrowserServiceLacros::OnSessionRestored(Profile* profile,
                                             int num_tabs_restored) {
  if (pending_open_urls_.empty())
    return;
  // Extract pending OpenUrl tasks for the restored |profile|.
  std::vector<PendingOpenUrl> pendings;
  for (auto& pending : pending_open_urls_) {
    if (pending.profile == profile) {
      pendings.push_back(std::move(pending));
      pending.profile = nullptr;  // Mark as moved.
    }
  }
  // Remove marked entries.
  pending_open_urls_.erase(base::ranges::remove(pending_open_urls_, nullptr,
                                                [](PendingOpenUrl& pending) {
                                                  return pending.profile;
                                                }),
                           pending_open_urls_.end());

  // Then, run for each.
  for (auto& pending : pendings)
    OpenUrlImpl(pending.profile, pending.url, std::move(pending.params),
                std::move(pending.callback));
}

void BrowserServiceLacros::OpenUrlImpl(Profile* profile,
                                       const GURL& url,
                                       crosapi::mojom::OpenUrlParamsPtr params,
                                       OpenUrlCallback callback) {
  NavigateParams navigate_params(
      profile, url,
      // The page transition is chosen to satisfy one of the conditions in
      // lacros_url_handling::IsNavigationInterceptable.
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_TYPED |
                                ui::PAGE_TRANSITION_FROM_API |
                                ui::PAGE_TRANSITION_FROM_ADDRESS_BAR));

  using OpenUrlParams = crosapi::mojom::OpenUrlParams;

  // Set up the window disposition.
  auto mojo_disposition =
      params ? params->disposition
             : OpenUrlParams::WindowOpenDisposition::kLegacyAutoDetection;
  switch (mojo_disposition) {
    // kLegacyAutoDetection is no longer supported but the API still allows it.
    case OpenUrlParams::WindowOpenDisposition::kLegacyAutoDetection:
    case OpenUrlParams::WindowOpenDisposition::kNewForegroundTab:
      navigate_params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
      break;
    case OpenUrlParams::WindowOpenDisposition::kNewWindow:
      navigate_params.disposition = WindowOpenDisposition::NEW_WINDOW;
      break;
    case OpenUrlParams::WindowOpenDisposition::kOffTheRecord:
      navigate_params.disposition = WindowOpenDisposition::OFF_THE_RECORD;
      break;
    case OpenUrlParams::WindowOpenDisposition::kSwitchToTab:
      navigate_params.disposition = WindowOpenDisposition::SWITCH_TO_TAB;
      navigate_params.path_behavior =
          ConvertPathBehavior(params->path_behavior);
      break;
  }

  // Ensure the browser window is showing when the URL is opened. This avoids
  // the user being unaware a new tab with `url` has been opened (if the window
  // was minimized for example).
  navigate_params.window_action = NavigateParams::SHOW_WINDOW;

  // If we need to create a window, do it now in order to suppress session
  // restore.
  navigate_params.browser = chrome::FindTabbedBrowser(profile, false);
  if (!navigate_params.browser &&
      Browser::GetCreationStatusForProfile(profile) ==
          Browser::CreationStatus::kOk) {
    Browser::CreateParams create_params(profile, navigate_params.user_gesture);
    create_params.should_trigger_session_restore = false;
    navigate_params.browser = Browser::Create(create_params);
  }

  Navigate(&navigate_params);

  auto* tab = navigate_params.navigated_or_inserted_contents.get();
  if (tab && params->from == crosapi::mojom::OpenUrlFrom::kArc) {
    // Add a flag to remember this tab originated in the ARC context.
    tab->SetUserData(&arc::ArcWebContentsData::kArcTransitionFlag,
                     std::make_unique<arc::ArcWebContentsData>(tab));
  }

  std::move(callback).Run(crosapi::mojom::CreationResult::kSuccess);
}

void BrowserServiceLacros::NewWindowWithProfile(
    bool incognito,
    bool should_trigger_session_restore,
    int64_t target_display_id,
    NewWindowCallback callback,
    Profile* profile) {
  if (!profile) {
    LOG(WARNING) << "No profile, it might be an early exit from the FRE. "
                    "Aborting the requested action.";
    std::move(callback).Run(crosapi::mojom::CreationResult::kProfileNotExist);
    return;
  }

  switch (IncognitoModePrefs::GetAvailability(profile->GetPrefs())) {
    case policy::IncognitoModeAvailability::kEnabled:
      // Default behavior: both incognito and regular mode are allowed.
      break;
    case policy::IncognitoModeAvailability::kDisabled:
      incognito = false;
      break;
    case policy::IncognitoModeAvailability::kForced:
      incognito = true;
      break;
    case policy::IncognitoModeAvailability::kNumTypes:
      NOTREACHED_IN_MIGRATION();
      break;
  }

  display::ScopedDisplayForNewWindows scoped(target_display_id);

  if (HasPendingUncleanExit(profile) &&
      BrowserLauncher::GetForProfile(profile)->LaunchForLastOpenedProfiles(
          /*skip_crash_restore=*/false, /*restore_tabbed_browser=*/true)) {
    // Restore all previously open profiles when recovering from a crash with
    // the profile picker disabled.
    std::move(callback).Run(crosapi::mojom::CreationResult::kUnknown);
    return;
  }

  chrome::NewEmptyWindow(
      incognito ? profile->GetPrimaryOTRProfile(/*create_if_needed=*/true)
                : profile,
      should_trigger_session_restore);
  std::move(callback).Run(crosapi::mojom::CreationResult::kSuccess);
}

void BrowserServiceLacros::NewFullscreenWindowWithProfile(
    const GURL& url,
    int64_t target_display_id,
    NewFullscreenWindowCallback callback,
    Profile* profile) {
  if (!profile) {
    std::move(callback).Run(crosapi::mojom::CreationResult::kProfileNotExist);
    return;
  }

  display::ScopedDisplayForNewWindows scoped(target_display_id);

  // Launch a fullscreen window with the user profile, and navigate to the
  // target URL.
  Browser::CreateParams params = Browser::CreateParams::CreateForApp(
      "app_name", true, gfx::Rect(), profile, false);
  params.initial_show_state = ui::mojom::WindowShowState::kFullscreen;
  Browser* browser = Browser::Create(params);
  NavigateParams nav_params(browser, url,
                            ui::PageTransition::PAGE_TRANSITION_AUTO_TOPLEVEL);
  Navigate(&nav_params);

  // Verify the creation result of browser window.
  if (!browser || !browser->window()) {
    std::move(callback).Run(
        crosapi::mojom::CreationResult::kBrowserWindowUnavailable);
    return;
  }

  browser->window()->Show();

  // Report a success result to ash. Please note that showing Lacros window is
  // asynchronous. Ash-chrome should use the `exo::WMHelper` class rather than
  // this callback method call to track window creation status.
  std::move(callback).Run(crosapi::mojom::CreationResult::kSuccess);
}

void BrowserServiceLacros::NewWindowForDetachingTabWithProfile(
    const std::u16string& tab_id,
    const std::u16string& group_id,
    NewWindowForDetachingTabCallback callback,
    Profile* profile) {
  if (!profile) {
    LOG(ERROR) << "No profile is found.";
    std::move(callback).Run(crosapi::mojom::CreationResult::kProfileNotExist,
                            std::string());
    return;
  }

  Browser* browser = chrome::FindBrowserWithProfile(profile);
  if (!browser) {
    LOG(ERROR) << "No browser is found.";
    std::move(callback).Run(
        crosapi::mojom::CreationResult::kBrowserWindowUnavailable,
        std::string());
    return;
  }

  Browser::CreateParams params = browser->create_params();
  params.user_gesture = true;
  params.initial_show_state = ui::mojom::WindowShowState::kDefault;
  Browser* new_browser = Browser::Create(params);
  CHECK(new_browser);

  if (!tab_strip_ui::DropTabsInNewBrowser(new_browser, tab_id, group_id)) {
    new_browser->window()->Close();
    // TODO(tonikitoo): Return a more specific error status, in case anything
    // goes wrong.
    std::move(callback).Run(crosapi::mojom::CreationResult::kUnknown,
                            std::string());
    return;
  }

  new_browser->window()->Show();

  auto* native_window = new_browser->window()->GetNativeWindow();
  auto* dwth_platform =
      views::DesktopWindowTreeHostLacros::From(native_window->GetHost());
  auto* platform_window = dwth_platform->platform_window();
  std::move(callback).Run(crosapi::mojom::CreationResult::kSuccess,
                          platform_window->GetWindowUniqueId());
}

void BrowserServiceLacros::LaunchOrNewTabWithProfile(
    bool should_trigger_session_restore,
    int64_t target_display_id,
    NewTabCallback callback,
    bool is_new_tab,
    Profile* profile) {
  if (!profile) {
    LOG(WARNING) << "No profile, it might be an early exit from the FRE. "
                    "Aborting the requested action.";
    std::move(callback).Run(crosapi::mojom::CreationResult::kProfileNotExist);
    return;
  }

  display::ScopedDisplayForNewWindows scoped(target_display_id);

  if (HasPendingUncleanExit(profile) &&
      BrowserLauncher::GetForProfile(profile)->LaunchForLastOpenedProfiles(
          /*skip_crash_restore=*/false, /*restore_tabbed_browser=*/true)) {
    // Restore all previously open profiles when recovering from a crash with
    // the profile picker disabled.
    std::move(callback).Run(crosapi::mojom::CreationResult::kUnknown);
    return;
  }

  Browser* browser = chrome::FindTabbedBrowser(
      profile, /*match_original_profiles=*/false, display::kInvalidDisplayId,
      /*ignore_closing_browsers=*/true);
  if (browser != nullptr) {
    chrome::NewTab(browser);
    browser->SetFocusToLocationBar();
  } else {
    DCHECK(is_new_tab || should_trigger_session_restore);
    if (is_new_tab && should_trigger_session_restore) {
      // Session restore happens asynchronously. Let |OnSessionRestored| create
      // a new tab afterwards.
      using OpenUrlParams = crosapi::mojom::OpenUrlParams;
      auto params = OpenUrlParams::New();
      params->disposition = OpenUrlParams::WindowOpenDisposition::kSwitchToTab;
      pending_open_urls_.push_back(
          PendingOpenUrl{profile, GURL{chrome::kChromeUINewTabURL},
                         std::move(params), std::move(callback)});
    }
    chrome::NewEmptyWindow(profile, should_trigger_session_restore);
  }
  if (!callback.is_null())
    std::move(callback).Run(crosapi::mojom::CreationResult::kSuccess);
}

void BrowserServiceLacros::OpenUrlWithProfile(
    const GURL& url,
    crosapi::mojom::OpenUrlParamsPtr params,
    OpenUrlCallback callback,
    Profile* profile) {
  if (!profile) {
    LOG(WARNING) << "No profile, it might be an early exit from the FRE. "
                    "Aborting the requested action.";
    std::move(callback).Run(crosapi::mojom::CreationResult::kProfileNotExist);
    return;
  }

  // If there is on-going session restoring task, let OnSessionRestored open the
  // URL on completion.
  if (SessionRestore::IsRestoring(profile)) {
    pending_open_urls_.push_back(
        PendingOpenUrl{profile, url, std::move(params), std::move(callback)});
  } else {
    OpenUrlImpl(profile, url, std::move(params), std::move(callback));
  }
}

void BrowserServiceLacros::OpenCaptivePortalSigninWithProfile(
    const GURL& url,
    OpenUrlCallback callback,
    Profile* profile) {
  if (!profile) {
    LOG(WARNING) << "No profile, it might be an early exit from the FRE. "
                    "Aborting the requested action.";
    std::move(callback).Run(crosapi::mojom::CreationResult::kProfileNotExist);
    return;
  }

  chromeos::NetworkPortalSigninWindow::Get()->Show(url);
  std::move(callback).Run(crosapi::mojom::CreationResult::kSuccess);
}

void BrowserServiceLacros::RestoreTabWithProfile(RestoreTabCallback callback,
                                                 Profile* profile) {
  if (!profile) {
    LOG(WARNING) << "No profile, it might be an early exit from the FRE. "
                    "Aborting the requested action.";
    std::move(callback).Run(crosapi::mojom::CreationResult::kProfileNotExist);
    return;
  }

  Browser* browser = chrome::FindBrowserWithProfile(profile);
  if (browser) {
    chrome::RestoreTab(browser);
  } else {
    chrome::OpenWindowWithRestoredTabs(profile);
  }
  std::move(callback).Run(crosapi::mojom::CreationResult::kSuccess);
}

void BrowserServiceLacros::OpenForFullRestoreWithProfile(
    bool skip_crash_restore,
    Profile* profile) {
  if (!profile) {
    LOG(WARNING) << "No profile, it might be an early exit from the FRE. "
                    "Aborting the requested action.";
    return;
  }
  BrowserLauncher::GetForProfile(profile)->LaunchForLastOpenedProfiles(
      skip_crash_restore, /*restore_tabbed_browser=*/false);
}

void BrowserServiceLacros::UpdateComponentPolicy(
    policy::ComponentPolicyMap policy) {
  chromeos::LacrosService::Get()->NotifyComponentPolicyUpdated(
      std::move(policy));
}

void BrowserServiceLacros::OnBrowserAdded(Browser* browser) {
  // Note: this happens only when ash-chrome is too old.
  // Please see the comment in the ctor for the detail.
  BrowserList::RemoveObserver(this);
  keep_alive_.reset();
}
