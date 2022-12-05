// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_mode/web_app/web_kiosk_app_launcher.h"

#include <memory>

#include "ash/public/cpp/window_properties.h"
#include "base/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/syslog_logging.h"
#include "chrome/browser/ash/app_mode/web_app/web_kiosk_app_data.h"
#include "chrome/browser/ash/app_mode/web_app/web_kiosk_app_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/web_applications/web_app_data_retriever.h"
#include "chrome/browser/web_applications/web_app_install_task.h"
#include "chrome/browser/web_applications/web_app_url_loader.h"
#include "chrome/common/chrome_features.h"
#include "chromeos/ui/base/window_pin_type.h"
#include "components/account_id/account_id.h"
#include "components/webapps/browser/install_result_code.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "ui/aura/window.h"
#include "ui/base/page_transition_types.h"
#include "url/origin.h"

namespace ash {

namespace {

void RecordKioskWebAppInstallError(webapps::InstallResultCode code) {
  base::UmaHistogramEnumeration("Kiosk.WebApp.InstallError", code);
}

}  // namespace

// The delay time of closing the splash window when a lacros-browser window is
// launched.
constexpr base::TimeDelta kSplashWindowCloseDelayTime = base::Seconds(1);

WebKioskAppLauncher::WebKioskAppLauncher(
    Profile* profile,
    const AccountId& account_id,
    bool should_skip_install,
    WebKioskAppLauncher::Delegate* delegate)
    : KioskAppLauncher(delegate),
      profile_(profile),
      account_id_(account_id),
      should_skip_install_(should_skip_install),
      url_loader_(std::make_unique<web_app::WebAppUrlLoader>()),
      data_retriever_factory_(base::BindRepeating(
          &std::make_unique<web_app::WebAppDataRetriever>)) {
  DCHECK(profile_);
}

WebKioskAppLauncher::~WebKioskAppLauncher() = default;

void WebKioskAppLauncher::Initialize() {
  const WebKioskAppData* app =
      WebKioskAppManager::Get()->GetAppByAccountId(account_id_);
  DCHECK(app);
  SYSLOG(INFO) << "Launching web kiosk for url: " << app->install_url();
  if (app->status() == WebKioskAppData::Status::kInstalled ||
      should_skip_install_) {
    delegate_->OnAppPrepared();
    return;
  }
  // If the app is not yet installed -- require network connection.
  delegate_->InitializeNetwork();
}

void WebKioskAppLauncher::ContinueWithNetworkReady() {
  if (!profile_)
    return;

  delegate_->OnAppInstalling();
  DCHECK(!is_installed_);
  install_task_ = std::make_unique<web_app::WebAppInstallTask>(
      profile_,
      /*install_finalizer=*/nullptr, data_retriever_factory_.Run(),
      /*registrar=*/nullptr, webapps::WebappInstallSource::MANAGEMENT_API);
  install_task_->LoadAndRetrieveWebAppInstallInfoWithIcons(
      WebKioskAppManager::Get()->GetAppByAccountId(account_id_)->install_url(),
      url_loader_.get(),
      base::BindOnce(&WebKioskAppLauncher::OnAppDataObtained,
                     weak_ptr_factory_.GetWeakPtr()));
}

const WebKioskAppData* WebKioskAppLauncher::GetCurrentApp() const {
  const WebKioskAppData* app =
      WebKioskAppManager::Get()->GetAppByAccountId(account_id_);
  DCHECK(app);
  return app;
}

void WebKioskAppLauncher::OnAppDataObtained(
    web_app::WebAppInstallTask::WebAppInstallInfoOrErrorCode info) {
  if (absl::holds_alternative<webapps::InstallResultCode>(info)) {
    RecordKioskWebAppInstallError(absl::get<webapps::InstallResultCode>(info));
    // Notify about failed installation, let the controller decide what to do.
    delegate_->OnLaunchFailed(KioskAppLaunchError::Error::kUnableToInstall);
    return;
  }

  DCHECK(absl::holds_alternative<WebAppInstallInfo>(info));
  const auto& app_info = absl::get<WebAppInstallInfo>(info);

  // When received |app_info.start_url| origin does not match the origin of
  // |install_url|, fail.
  if (url::Origin::Create(GetCurrentApp()->install_url()) !=
      url::Origin::Create(app_info.start_url)) {
    VLOG(1) << "Origin of the app does not match the origin of install url";
    delegate_->OnLaunchFailed(KioskAppLaunchError::Error::kUnableToLaunch);
    return;
  }

  WebKioskAppManager::Get()->UpdateAppByAccountId(account_id_, app_info);
  delegate_->OnAppPrepared();
}

void WebKioskAppLauncher::OnLacrosWindowCreated(
    crosapi::mojom::CreationResult result) {
  if (result != crosapi::mojom::CreationResult::kSuccess) {
    exo::WMHelper::GetInstance()->RemoveExoWindowObserver(this);
    LOG(ERROR) << "The lacros window failed to be created. Result: " << result;
    delegate_->OnLaunchFailed(KioskAppLaunchError::Error::kUnableToLaunch);
  }
}

void WebKioskAppLauncher::CreateNewLacrosWindow() {
  DCHECK(exo::WMHelper::HasInstance());
  exo::WMHelper::GetInstance()->AddExoWindowObserver(this);
  crosapi::BrowserManager::Get()->NewFullscreenWindow(
      GetCurrentApp()->GetLaunchableUrl(),
      base::BindOnce(&WebKioskAppLauncher::OnLacrosWindowCreated,
                     weak_ptr_factory_.GetWeakPtr()));
}

void WebKioskAppLauncher::LaunchApp() {
  if (!profile_)
    return;

  DCHECK(!browser_);
  const WebKioskAppData* app = GetCurrentApp();

  // Launch lacros-chrome if the corresponding feature flags are enabled.
  //
  // TODO(crbug.com/1101667): Currently, this source has log spamming by
  // LOG(WARNING) to make it easy to debug and develop. Get rid of the log
  // spamming when it gets stable enough.
  if (crosapi::browser_util::IsLacrosEnabledInWebKioskSession()) {
    LOG(WARNING) << "Using lacros-chrome for web kiosk session.";
    delegate_->OnAppLaunched();
    if (crosapi::BrowserManager::Get()->IsRunning()) {
      CreateNewLacrosWindow();
    } else {
      LOG(WARNING) << "Waiting for lacros-chrome to be ready.";
      observation_.Observe(crosapi::BrowserManager::Get());
    }
    return;
  }

  Browser::CreateParams params = Browser::CreateParams::CreateForApp(
      app->name(), true, gfx::Rect(), profile_, false);
  params.initial_show_state = ui::SHOW_STATE_FULLSCREEN;
  if (test_browser_window_) {
    params.window = test_browser_window_;
  }

  browser_ = Browser::Create(params);
  NavigateParams nav_params(browser_, app->GetLaunchableUrl(),
                            ui::PageTransition::PAGE_TRANSITION_AUTO_TOPLEVEL);
  Navigate(&nav_params);
  CHECK(browser_);
  CHECK(browser_->window());
  browser_->window()->Show();

  WebKioskAppManager::Get()->InitSession(browser_, browser_->profile());
  delegate_->OnAppLaunched();
  delegate_->OnAppWindowCreated();
}

void WebKioskAppLauncher::RestartLauncher() {
  weak_ptr_factory_.InvalidateWeakPtrs();
  install_task_.reset();

  Initialize();
}

void WebKioskAppLauncher::OnStateChanged() {
  if (crosapi::BrowserManager::Get()->IsRunning()) {
    observation_.Reset();
    CreateNewLacrosWindow();
  }
}

void WebKioskAppLauncher::OnExoWindowCreated(aura::Window* window) {
  if (!profile_)
    return;

  CHECK(crosapi::browser_util::IsLacrosWindow(window));
  exo::WMHelper::GetInstance()->RemoveExoWindowObserver(this);
  WebKioskAppManager::Get()->InitSession(nullptr, profile_);

  // NOTE: There is a known issue (crbug/1220680) that causes an obvious twinkle
  // when an exo window is launched in a fullscreen mode. This short delay is
  // just a temporary workaround, and should be removed after the issue is
  // solved.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&KioskAppLauncher::Delegate::OnAppWindowCreated,
                     base::Unretained(delegate_)),
      kSplashWindowCloseDelayTime);
}

void WebKioskAppLauncher::OnProfileWillBeDestroyed(Profile* profile) {
  DCHECK_EQ(profile_, profile);
  profile_observation_.Reset();
  profile_ = nullptr;
}

void WebKioskAppLauncher::SetDataRetrieverFactoryForTesting(
    base::RepeatingCallback<std::unique_ptr<web_app::WebAppDataRetriever>()>
        data_retriever_factory) {
  data_retriever_factory_ = data_retriever_factory;
}

void WebKioskAppLauncher::SetBrowserWindowForTesting(BrowserWindow* window) {
  test_browser_window_ = window;
}

void WebKioskAppLauncher::SetUrlLoaderForTesting(
    std::unique_ptr<web_app::WebAppUrlLoader> url_loader) {
  url_loader_ = std::move(url_loader);
}

}  // namespace ash
