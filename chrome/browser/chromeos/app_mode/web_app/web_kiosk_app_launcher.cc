// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <chrome/browser/chromeos/app_mode/web_app/web_kiosk_app_launcher.h>
#include <memory>

#include "ash/public/cpp/window_pin_type.h"
#include "ash/public/cpp/window_properties.h"
#include "base/bind.h"
#include "chrome/browser/chromeos/app_mode/web_app/web_kiosk_app_data.h"
#include "chrome/browser/chromeos/app_mode/web_app/web_kiosk_app_manager.h"
#include "chrome/browser/extensions/api/tabs/tabs_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/web_applications/components/web_app_data_retriever.h"
#include "chrome/browser/web_applications/components/web_app_url_loader.h"
#include "chrome/browser/web_applications/web_app_install_task.h"
#include "components/account_id/account_id.h"
#include "ui/aura/window.h"
#include "ui/base/page_transition_types.h"

namespace chromeos {

WebKioskAppLauncher::WebKioskAppLauncher(
    Profile* profile,
    WebKioskAppLauncher::Delegate* delegate)
    : profile_(profile),
      delegate_(delegate),
      url_loader_(std::make_unique<web_app::WebAppUrlLoader>()) {}

WebKioskAppLauncher::~WebKioskAppLauncher() = default;

void WebKioskAppLauncher::Initialize(const AccountId& account_id) {
  account_id_ = account_id;
  const WebKioskAppData* app =
      WebKioskAppManager::Get()->GetAppByAccountId(account_id_);
  DCHECK(app);
  if (app->status() == WebKioskAppData::STATUS_INSTALLED) {
    delegate_->OnAppPrepared();
    return;
  }
  // If the app is not yet installed -- require network connection.
  delegate_->InitializeNetwork();
}

void WebKioskAppLauncher::ContinueWithNetworkReady() {
  delegate_->OnAppStartedInstalling();
  DCHECK(!is_installed_);
  install_task_.reset(new web_app::WebAppInstallTask(
      profile_, /*shortcut_manager=*/nullptr, /*install_finalizer=*/nullptr,
      std::make_unique<web_app::WebAppDataRetriever>()));
  install_task_->LoadAndRetrieveWebApplicationInfoWithIcons(
      WebKioskAppManager::Get()->GetAppByAccountId(account_id_)->install_url(),
      url_loader_.get(),
      base::BindOnce(&WebKioskAppLauncher::OnAppDataObtained,
                     weak_ptr_factory_.GetWeakPtr()));
}

void WebKioskAppLauncher::OnAppDataObtained(
    std::unique_ptr<WebApplicationInfo> app_info) {
  if (app_info) {
    WebKioskAppManager::Get()->UpdateAppByAccountId(account_id_,
                                                    std::move(app_info));
  }
  // If we could not update the app data, we should still launch the app(we may
  // be under captive portal, there can be redirect, etc).
  delegate_->OnAppPrepared();
}

void WebKioskAppLauncher::LaunchApp() {
  DCHECK(!browser_);
  const WebKioskAppData* app =
      WebKioskAppManager::Get()->GetAppByAccountId(account_id_);
  DCHECK(app);

  GURL url = app->status() == WebKioskAppData::STATUS_INSTALLED
                 ? app->launch_url()
                 : app->install_url();

  Browser::CreateParams params(Browser::TYPE_APP, profile_, false);

  browser_ = Browser::Create(params);
  NavigateParams nav_params(browser_, url,
                            ui::PageTransition::PAGE_TRANSITION_AUTO_TOPLEVEL);
  Navigate(&nav_params);
  CHECK(browser_);
  CHECK(browser_->window());
  CHECK(browser_->window()->GetNativeWindow());
  browser_->window()->GetNativeWindow()->SetProperty(
      ash::kWindowPinTypeKey, ash::WindowPinType::kTrustedPinned);
  browser_->window()->Show();
  delegate_->OnAppLaunched();
}

}  // namespace chromeos
