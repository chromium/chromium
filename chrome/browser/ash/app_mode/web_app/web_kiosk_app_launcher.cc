// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_mode/web_app/web_kiosk_app_launcher.h"

#include <memory>
#include <mutex>

#include "ash/public/cpp/window_properties.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/syslog_logging.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/ash/app_mode/web_app/web_kiosk_app_data.h"
#include "chrome/browser/ash/app_mode/web_app/web_kiosk_app_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_install_utils.h"
#include "chrome/browser/web_applications/web_contents/web_app_data_retriever.h"
#include "chrome/browser/web_applications/web_contents/web_app_url_loader.h"
#include "chrome/common/chrome_features.h"
#include "chromeos/ui/base/window_pin_type.h"
#include "components/account_id/account_id.h"
#include "components/webapps/browser/install_result_code.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "content/public/browser/web_contents.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
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
    WebKioskAppLauncher::NetworkDelegate* network_delegate)
    : KioskAppLauncher(network_delegate),
      profile_(profile),
      account_id_(account_id),
      should_skip_install_(should_skip_install),
      url_loader_(std::make_unique<web_app::WebAppUrlLoader>()),
      data_retriever_factory_(base::BindRepeating(
          &std::make_unique<web_app::WebAppDataRetriever>)) {
  DCHECK(profile_);
}

WebKioskAppLauncher::~WebKioskAppLauncher() = default;

void WebKioskAppLauncher::AddObserver(KioskAppLauncher::Observer* observer) {
  observers_.AddObserver(observer);
}

void WebKioskAppLauncher::RemoveObserver(KioskAppLauncher::Observer* observer) {
  observers_.RemoveObserver(observer);
}

void WebKioskAppLauncher::Initialize() {
  const WebKioskAppData* app =
      WebKioskAppManager::Get()->GetAppByAccountId(account_id_);
  DCHECK(app);
  SYSLOG(INFO) << "Launching web kiosk for url: " << app->install_url();
  if (app->status() == WebKioskAppData::Status::kInstalled ||
      should_skip_install_) {
    observers_.NotifyAppPrepared();
    return;
  }
  // If the app is not yet installed -- require network connection.
  delegate_->InitializeNetwork();
}

void WebKioskAppLauncher::ContinueWithNetworkReady() {
  if (!profile_) {
    return;
  }

  observers_.NotifyAppInstalling();
  DCHECK(!is_installed_);

  web_contents_for_app_info_ = content::WebContents::Create(
      content::WebContents::CreateParams(profile_));
  web_app::CreateWebAppInstallTabHelpers(web_contents_for_app_info_.get());

  url_loader_->LoadUrl(
      WebKioskAppManager::Get()->GetAppByAccountId(account_id_)->install_url(),
      web_contents_for_app_info_.get(),
      web_app::WebAppUrlLoader::UrlComparison::kIgnoreQueryParamsAndRef,
      base::BindOnce(&WebKioskAppLauncher::OnUrlLoaded,
                     weak_ptr_factory_.GetWeakPtr()));
}

const WebKioskAppData* WebKioskAppLauncher::GetCurrentApp() const {
  const WebKioskAppData* app =
      WebKioskAppManager::Get()->GetAppByAccountId(account_id_);
  DCHECK(app);
  return app;
}

void WebKioskAppLauncher::OnUrlLoaded(web_app::WebAppUrlLoader::Result result) {
  if (web_contents_for_app_info_->IsBeingDestroyed() ||
      profile_->ShutdownStarted()) {
    OnAppDataObtained(webapps::InstallResultCode::kWebContentsDestroyed);
    return;
  }

  if (result == web_app::WebAppUrlLoader::Result::kRedirectedUrlLoaded) {
    OnAppDataObtained(webapps::InstallResultCode::kInstallURLRedirected);
    return;
  }

  if (result == web_app::WebAppUrlLoader::Result::kFailedPageTookTooLong) {
    OnAppDataObtained(webapps::InstallResultCode::kInstallURLLoadTimeOut);
    return;
  }

  if (result != web_app::WebAppUrlLoader::Result::kUrlLoaded) {
    OnAppDataObtained(webapps::InstallResultCode::kInstallURLLoadFailed);
    return;
  }

  data_retriever_ = data_retriever_factory_.Run();

  data_retriever_->GetWebAppInstallInfo(
      web_contents_for_app_info_.get(),
      base::BindOnce([](std::unique_ptr<WebAppInstallInfo> install_info) {
        absl::variant<WebAppInstallInfo, webapps::InstallResultCode> result;
        if (install_info) {
          result = std::move(*install_info);
        } else {
          result = webapps::InstallResultCode::kGetWebAppInstallInfoFailed;
        }
        return result;
      })
          .Then(base::BindOnce(&WebKioskAppLauncher::OnAppDataObtained,
                               weak_ptr_factory_.GetWeakPtr())));
}

void WebKioskAppLauncher::OnAppDataObtained(
    absl::variant<WebAppInstallInfo, webapps::InstallResultCode> info) {
  web_contents_for_app_info_.reset();
  data_retriever_.reset();
  if (absl::holds_alternative<webapps::InstallResultCode>(info)) {
    RecordKioskWebAppInstallError(absl::get<webapps::InstallResultCode>(info));
    // Notify about failed installation, let the controller decide what to do.
    observers_.NotifyLaunchFailed(KioskAppLaunchError::Error::kUnableToInstall);
    return;
  }

  DCHECK(absl::holds_alternative<WebAppInstallInfo>(info));
  const auto& app_info = absl::get<WebAppInstallInfo>(info);

  // When received |app_info.start_url| origin does not match the origin of
  // |install_url|, fail.
  if (url::Origin::Create(GetCurrentApp()->install_url()) !=
      url::Origin::Create(app_info.start_url)) {
    VLOG(1) << "Origin of the app does not match the origin of install url";
    observers_.NotifyLaunchFailed(KioskAppLaunchError::Error::kUnableToLaunch);
    return;
  }

  WebKioskAppManager::Get()->UpdateAppByAccountId(account_id_, app_info);
  observers_.NotifyAppPrepared();
}

void WebKioskAppLauncher::OnLacrosWindowCreated(
    crosapi::mojom::CreationResult result) {
  if (result != crosapi::mojom::CreationResult::kSuccess) {
    exo::WMHelper::GetInstance()->RemoveExoWindowObserver(this);
    LOG(ERROR) << "The lacros window failed to be created. Result: " << result;
    observers_.NotifyLaunchFailed(KioskAppLaunchError::Error::kUnableToLaunch);
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
  if (!profile_) {
    return;
  }

  DCHECK(!browser_);
  const WebKioskAppData* app = GetCurrentApp();

  if (crosapi::browser_util::IsLacrosEnabledInWebKioskSession()) {
    observers_.NotifyAppLaunched();
    CreateNewLacrosWindow();
    return;
  }

  Browser::CreateParams params = Browser::CreateParams::CreateForApp(
      app->name(), true, gfx::Rect(), profile_, false);
  params.initial_show_state = ui::SHOW_STATE_FULLSCREEN;
  if (test_browser_window_) {
    params.window = test_browser_window_.get();
  }

  browser_ = Browser::Create(params);
  NavigateParams nav_params(browser_, app->GetLaunchableUrl(),
                            ui::PageTransition::PAGE_TRANSITION_AUTO_TOPLEVEL);
  Navigate(&nav_params);
  CHECK(browser_);
  CHECK(browser_->window());
  browser_->window()->Show();

  observers_.NotifyAppLaunched();
  observers_.NotifyAppWindowCreated(browser_->app_name());
}

void WebKioskAppLauncher::OnExoWindowCreated(aura::Window* window) {
  if (!profile_) {
    return;
  }

  CHECK(crosapi::browser_util::IsLacrosWindow(window));
  exo::WMHelper::GetInstance()->RemoveExoWindowObserver(this);

  // NOTE: There is a known issue (crbug/1220680) that causes an obvious twinkle
  // when an exo window is launched in a fullscreen mode. This short delay is
  // just a temporary workaround, and should be removed after the issue is
  // solved.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&WebKioskAppLauncher::NotifyAppWindowCreated,
                     weak_ptr_factory_.GetWeakPtr()),
      kSplashWindowCloseDelayTime);
}

void WebKioskAppLauncher::NotifyAppWindowCreated() {
  observers_.NotifyAppWindowCreated();
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
