// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/ssl_error_controller_client.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/files/file_util.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "base/process/launch.h"
#include "base/task/post_task.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/interstitials/chrome_metrics_helper.h"
#include "chrome/browser/interstitials/enterprise_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ssl/chrome_ssl_host_state_delegate.h"
#include "chrome/browser/ssl/chrome_ssl_host_state_delegate_factory.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "components/safe_browsing/common/safe_browsing_prefs.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"

#if defined(OS_ANDROID)
#include "chrome/browser/android/intent_helper.h"
#endif

#if defined(OS_CHROMEOS)
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chrome/common/webui_url_constants.h"
#endif

#if defined(OS_WIN)
#include "base/base_paths_win.h"
#include "base/path_service.h"
#include "base/strings/string16.h"
#include "base/win/windows_version.h"
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

using content::Referrer;

namespace {

void RecordRecurrentErrorAction(
    SSLErrorControllerClient::RecurrentErrorAction action,
    int cert_error) {
  UMA_HISTOGRAM_ENUMERATION("interstitial.ssl_recurrent_error.action", action);
  if (cert_error == net::ERR_CERTIFICATE_TRANSPARENCY_REQUIRED) {
    UMA_HISTOGRAM_ENUMERATION(
        "interstitial.ssl_recurrent_error.ct_error.action", action);
  }
}

bool HasSeenRecurrentErrorInternal(content::WebContents* web_contents,
                                   int cert_error) {
  ChromeSSLHostStateDelegate* state =
      ChromeSSLHostStateDelegateFactory::GetForProfile(
          Profile::FromBrowserContext(web_contents->GetBrowserContext()));
  return state->HasSeenRecurrentErrors(cert_error);
}

#if !defined(OS_CHROMEOS)
void LaunchDateAndTimeSettingsImpl() {
// The code for each OS is completely separate, in order to avoid bugs like
// https://crbug.com/430877 . ChromeOS is handled on the UI thread.
#if defined(OS_ANDROID)
  chrome::android::OpenDateAndTimeSettings();

#elif defined(OS_LINUX)
  struct ClockCommand {
    const char* const pathname;
    const char* const argument;
  };
  static const ClockCommand kClockCommands[] = {
      // Unity
      {"/usr/bin/unity-control-center", "datetime"},
      // GNOME
      //
      // NOTE: On old Ubuntu, naming control panels doesn't work, so it
      // opens the overview. This will have to be good enough.
      {"/usr/bin/gnome-control-center", "datetime"},
      {"/usr/local/bin/gnome-control-center", "datetime"},
      {"/opt/bin/gnome-control-center", "datetime"},
      // KDE
      {"/usr/bin/kcmshell4", "clock"},
      {"/usr/local/bin/kcmshell4", "clock"},
      {"/opt/bin/kcmshell4", "clock"},
  };

  base::CommandLine command(base::FilePath(""));
  for (const ClockCommand& cmd : kClockCommands) {
    base::FilePath pathname(cmd.pathname);
    if (base::PathExists(pathname)) {
      command.SetProgram(pathname);
      command.AppendArg(cmd.argument);
      break;
    }
  }
  if (command.GetProgram().empty()) {
    // Alas, there is nothing we can do.
    return;
  }

  base::LaunchOptions options;
  options.wait = false;
  options.allow_new_privs = true;
  base::LaunchProcess(command, options);

#elif defined(OS_MACOSX)
  base::CommandLine command(base::FilePath("/usr/bin/open"));
  command.AppendArg("/System/Library/PreferencePanes/DateAndTime.prefPane");

  base::LaunchOptions options;
  options.wait = false;
  base::LaunchProcess(command, options);

#elif defined(OS_WIN)
  base::FilePath path;
  base::PathService::Get(base::DIR_SYSTEM, &path);
  static const base::char16 kControlPanelExe[] = L"control.exe";
  path = path.Append(base::string16(kControlPanelExe));
  base::CommandLine command(path);
  command.AppendArg(std::string("/name"));
  command.AppendArg(std::string("Microsoft.DateAndTime"));

  base::LaunchOptions options;
  options.wait = false;
  base::LaunchProcess(command, options);

#else
#error Unsupported target architecture.
#endif
  // Don't add code here! (See the comment at the beginning of the function.)
}
#endif

}  // namespace

SSLErrorControllerClient::SSLErrorControllerClient(
    content::WebContents* web_contents,
    const net::SSLInfo& ssl_info,
    int cert_error,
    const GURL& request_url,
    std::unique_ptr<security_interstitials::MetricsHelper> metrics_helper)
    : SecurityInterstitialControllerClient(
          web_contents,
          std::move(metrics_helper),
          Profile::FromBrowserContext(web_contents->GetBrowserContext())
              ->GetPrefs(),
          g_browser_process->GetApplicationLocale(),
          GURL(chrome::kChromeUINewTabURL)),
      ssl_info_(ssl_info),
      request_url_(request_url),
      cert_error_(cert_error) {
  if (HasSeenRecurrentErrorInternal(web_contents_, cert_error_)) {
    RecordRecurrentErrorAction(RecurrentErrorAction::kShow, cert_error_);
  }
}

SSLErrorControllerClient::~SSLErrorControllerClient() {}

void SSLErrorControllerClient::GoBack() {
  SecurityInterstitialControllerClient::GoBackAfterNavigationCommitted();
}

void SSLErrorControllerClient::Proceed() {
  if (HasSeenRecurrentErrorInternal(web_contents_, cert_error_)) {
    RecordRecurrentErrorAction(RecurrentErrorAction::kProceed, cert_error_);
  }

  MaybeTriggerSecurityInterstitialProceededEvent(web_contents_, request_url_,
                                                 "SSL_ERROR", cert_error_);
#if BUILDFLAG(ENABLE_EXTENSIONS)
  // Hosted Apps should not be allowed to run if there is a problem with their
  // certificate. So, when users click proceed on an interstitial, move the tab
  // to a regular Chrome window and proceed as usual there.
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents_);
  if (web_app::AppBrowserController::IsForWebAppBrowser(browser))
    chrome::OpenInChrome(browser);
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

  Profile* profile =
      Profile::FromBrowserContext(web_contents_->GetBrowserContext());
  ChromeSSLHostStateDelegate* state = static_cast<ChromeSSLHostStateDelegate*>(
      profile->GetSSLHostStateDelegate());
  // ChromeSSLHostStateDelegate can be null during tests.
  if (state) {
    state->AllowCert(request_url_.host(), *ssl_info_.cert.get(), cert_error_);
    Reload();
  }
}

bool SSLErrorControllerClient::CanLaunchDateAndTimeSettings() {
#if defined(OS_ANDROID) || defined(OS_LINUX) || defined(OS_MACOSX) || \
    defined(OS_WIN)
  return true;
#else
  return false;
#endif
}

void SSLErrorControllerClient::LaunchDateAndTimeSettings() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

#if defined(OS_CHROMEOS)
  chrome::SettingsWindowManager::GetInstance()->ShowOSSettings(
      ProfileManager::GetActiveUserProfile(), chrome::kDateTimeSubPage);
#else
  base::PostTask(
      FROM_HERE,
      {base::ThreadPool(), base::TaskPriority::USER_VISIBLE, base::MayBlock()},
      base::BindOnce(&LaunchDateAndTimeSettingsImpl));
#endif
}

bool SSLErrorControllerClient::HasSeenRecurrentError() {
  return HasSeenRecurrentErrorInternal(web_contents_, cert_error_);
}
