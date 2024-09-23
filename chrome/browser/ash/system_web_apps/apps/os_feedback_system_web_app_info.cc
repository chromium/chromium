// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_web_apps/apps/os_feedback_system_web_app_info.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/webui/grit/ash_os_feedback_resources.h"
#include "ash/webui/os_feedback_ui/url_constants.h"
#include "base/memory/ref_counted_memory.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/ash/os_feedback/os_feedback_screenshot_manager.h"
#include "chrome/browser/ash/system_web_apps/apps/system_web_app_install_utils.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_util.h"
#include "chrome/browser/ui/ash/system_web_apps/system_web_app_ui_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "third_party/blink/public/mojom/manifest/display_mode.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/display/scoped_display_for_new_windows.h"
#include "ui/display/screen.h"

namespace {

// All Feedback Tool window will be a fixed 600px*640px portal per
// specification.
constexpr int kFeedbackAppDefaultWidth = 600;
constexpr int kFeedbackAppDefaultHeight = 640;

bool IsUserFeedbackAllowed(Profile* profile) {
  return profile->GetPrefs()->GetBoolean(prefs::kUserFeedbackAllowed);
}

}  // namespace

std::unique_ptr<web_app::WebAppInstallInfo>
CreateWebAppInfoForOSFeedbackSystemWebApp() {
  GURL start_url(ash::kChromeUIOSFeedbackUrl);
  auto info =
      web_app::CreateSystemWebAppInstallInfoWithStartUrlAsIdentity(start_url);
  info->scope = GURL(ash::kChromeUIOSFeedbackUrl);
  info->title = l10n_util::GetStringUTF16(IDS_FEEDBACK_REPORT_APP_TITLE);
  web_app::CreateIconInfoForSystemWebApp(
      info->start_url(),
      {// use the new icons per the request in http://b/186638497
       {"app_icon_48.png", 48, IDR_ASH_OS_FEEDBACK_APP_ICON_48_PNG},
       {"app_icon_192.png", 192, IDR_ASH_OS_FEEDBACK_APP_ICON_192_PNG},
       {"app_icon_256.png", 256, IDR_ASH_OS_FEEDBACK_APP_ICON_256_PNG}},
      *info);
  info->display_mode = blink::mojom::DisplayMode::kStandalone;
  info->user_display_mode = web_app::mojom::UserDisplayMode::kStandalone;

  return info;
}

gfx::Rect GetDefaultBoundsForOSFeedbackApp(Browser*) {
  gfx::Rect bounds =
      display::Screen::GetScreen()->GetDisplayForNewWindows().work_area();
  bounds.ClampToCenteredSize(
      {kFeedbackAppDefaultWidth, kFeedbackAppDefaultHeight});
  return bounds;
}

OSFeedbackAppDelegate::OSFeedbackAppDelegate(Profile* profile)
    : ash::SystemWebAppDelegate(ash::SystemWebAppType::OS_FEEDBACK,
                                "OSFeedback",
                                GURL(ash::kChromeUIOSFeedbackUrl),
                                profile) {}

OSFeedbackAppDelegate::~OSFeedbackAppDelegate() = default;

std::unique_ptr<web_app::WebAppInstallInfo>
OSFeedbackAppDelegate::GetWebAppInfo() const {
  return CreateWebAppInfoForOSFeedbackSystemWebApp();
}

bool OSFeedbackAppDelegate::ShouldAllowScriptsToCloseWindows() const {
  return true;
}
bool OSFeedbackAppDelegate::ShouldCaptureNavigations() const {
  return true;
}

bool OSFeedbackAppDelegate::ShouldAllowFullscreen() const {
  return false;
}

bool OSFeedbackAppDelegate::ShouldAllowMaximize() const {
  return false;
}
bool OSFeedbackAppDelegate::ShouldAllowResize() const {
  return false;
}

bool OSFeedbackAppDelegate::ShouldShowInLauncher() const {
  return false;
}

bool OSFeedbackAppDelegate::ShouldShowInSearchAndShelf() const {
  return IsUserFeedbackAllowed(profile());
}

gfx::Rect OSFeedbackAppDelegate::GetDefaultBounds(Browser* browser) const {
  return GetDefaultBoundsForOSFeedbackApp(browser);
}

Browser* OSFeedbackAppDelegate::LaunchAndNavigateSystemWebApp(
    Profile* profile,
    web_app::WebAppProvider* provider,
    const GURL& url,
    const apps::AppLaunchParams& params) const {
  // This check is needed to enforce the policy no matter how and from where the
  // feedback tool is to be launched.
  if (IsUserFeedbackAllowed(profile)) {
    // Check whether the feedback app is opened already. If yes, just show it.
    Browser* browser = ash::FindSystemWebAppBrowser(
        profile, ash::SystemWebAppType::OS_FEEDBACK);
    if (browser) {
      browser->window()->Show();
    } else {
      apps::AppLaunchParams app_params(
          params.app_id, params.container, params.disposition,
          params.launch_source, params.display_id, params.launch_files,
          params.intent ? params.intent->Clone() : nullptr);
      // Take a screenshot and launch the app afterward.
      ash::OsFeedbackScreenshotManager::GetInstance()->TakeScreenshot(
          base::BindOnce(&OSFeedbackAppDelegate::OnScreenshotTaken,
                         weak_ptr_factory_.GetWeakPtr(), profile, provider, url,
                         std::move(app_params)));

      // Record an UMA histogram when feedback app is open from Launcher.
      if (params.launch_source != apps::LaunchSource::kFromChromeInternal) {
        UMA_HISTOGRAM_ENUMERATION("Feedback.RequestSource",
                                  feedback::kFeedbackSourceLauncher,
                                  feedback::kFeedbackSourceCount);
      }
    }
  }
  // Return nullptr to tell the rest of the code SWA aborted the launch so that
  // the Feedback can use a customized launch process, i.e., take a screenshot
  // async, then launch afterward.
  return nullptr;
}

void OSFeedbackAppDelegate::OnScreenshotTaken(Profile* profile,
                                              web_app::WebAppProvider* provider,
                                              GURL url,
                                              apps::AppLaunchParams params,
                                              bool status) const {
  // Exit early if we can't create browser windows (e.g. when browser is
  // shutting down, or a wrong profile is given).
  if (Browser::GetCreationStatusForProfile(profile) !=
      Browser::CreationStatus::kOk) {
    return;
  }

  // Place new windows on the specified display.
  display::ScopedDisplayForNewWindows scoped_display(params.display_id);

  Browser* browser = SystemWebAppDelegate::LaunchAndNavigateSystemWebApp(
      profile, provider, url, params);
  if (!browser) {
    return;
  }

  // LaunchSystemWebAppImpl may be called with a profile associated with an
  // inactive (background) desktop (e.g. when multiple users are logged in).
  // Here we move the newly created browser window (or the existing one on the
  // inactive desktop) to the current active (visible) desktop, so the user
  // always sees the launched app.
  multi_user_util::MoveWindowToCurrentDesktop(
      browser->window()->GetNativeWindow());

  browser->window()->Show();
}
