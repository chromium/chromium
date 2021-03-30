// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_mode/web_app/web_kiosk_app_launcher.h"

#include <memory>

#include "ash/public/cpp/window_properties.h"
#include "base/bind.h"
#include "base/logging.h"
#include "chrome/browser/ash/app_mode/web_app/web_kiosk_app_data.h"
#include "chrome/browser/ash/app_mode/web_app/web_kiosk_app_manager.h"
#include "chrome/browser/extensions/api/tabs/tabs_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/web_applications/components/web_app_data_retriever.h"
#include "chrome/browser/web_applications/components/web_app_url_loader.h"
#include "chrome/browser/web_applications/web_app_install_task.h"
#include "chromeos/ui/base/window_pin_type.h"
#include "components/account_id/account_id.h"
#include "ui/aura/window.h"
#include "ui/base/page_transition_types.h"
#include "url/origin.h"

namespace ash {

WebKioskAppLauncher::WebKioskAppLauncher(
    Profile* profile,
    WebKioskAppLauncher::Delegate* delegate,
    const AccountId& account_id)
    : KioskAppLauncher(delegate),
      profile_(profile),
      account_id_(account_id),
      url_loader_(std::make_unique<web_app::WebAppUrlLoader>()),
      data_retriever_factory_(base::BindRepeating(
          &std::make_unique<web_app::WebAppDataRetriever>)) {}

WebKioskAppLauncher::~WebKioskAppLauncher() = default;

void WebKioskAppLauncher::Initialize() {
  const WebKioskAppData* app =
      WebKioskAppManager::Get()->GetAppByAccountId(account_id_);
  DCHECK(app);
  if (app->status() == WebKioskAppData::Status::kInstalled ||
      delegate_->ShouldSkipAppInstallation()) {
    delegate_->OnAppPrepared();
    return;
  }
  // If the app is not yet installed -- require network connection.
  delegate_->InitializeNetwork();
}

void WebKioskAppLauncher::ContinueWithNetworkReady() {
  delegate_->OnAppInstalling();
  DCHECK(!is_installed_);
  install_task_.reset(new web_app::WebAppInstallTask(
      profile_, /*os_integration_manager=*/nullptr,
      /*install_finalizer=*/nullptr, data_retriever_factory_.Run(),
      /*registrar=*/nullptr));
  install_task_->LoadAndRetrieveWebApplicationInfoWithIcons(
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
    std::unique_ptr<WebApplicationInfo> app_info) {
  if (!app_info) {
    // Notify about failed installation, let the controller decide what to do.
    delegate_->OnLaunchFailed(KioskAppLaunchError::Error::kUnableToInstall);
    return;
  }

  // When received |app_info->start_url| origin does not match the origin of
  // |install_url|, fail.
  if (url::Origin::Create(GetCurrentApp()->install_url()) !=
      url::Origin::Create(app_info->start_url)) {
    VLOG(1) << "Origin of the app does not match the origin of install url";
    delegate_->OnLaunchFailed(KioskAppLaunchError::Error::kUnableToLaunch);
    return;
  }

  WebKioskAppManager::Get()->UpdateAppByAccountId(account_id_,
                                                  std::move(app_info));
  delegate_->OnAppPrepared();
}

void WebKioskAppLauncher::LaunchApp() {
  DCHECK(!browser_);
  const WebKioskAppData* app = GetCurrentApp();

  GURL url = app->status() == WebKioskAppData::Status::kInstalled
                 ? app->launch_url()
                 : app->install_url();

  Browser::CreateParams params = Browser::CreateParams::CreateForApp(
      app->name(), true, gfx::Rect(), profile_, false);
  params.initial_show_state = ui::SHOW_STATE_FULLSCREEN;
  if (test_browser_window_) {
    params.window = test_browser_window_;
  }

  browser_ = Browser::Create(params);
  NavigateParams nav_params(browser_, url,
                            ui::PageTransition::PAGE_TRANSITION_AUTO_TOPLEVEL);
  Navigate(&nav_params);
  CHECK(browser_);
  CHECK(browser_->window());
  browser_->window()->Show();

  WebKioskAppManager::Get()->InitSession(browser_);
  delegate_->OnAppLaunched();
  delegate_->OnAppWindowCreated();
}

void WebKioskAppLauncher::RestartLauncher() {
  weak_ptr_factory_.InvalidateWeakPtrs();
  install_task_.reset();

  Initialize();
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
