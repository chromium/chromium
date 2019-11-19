// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/print_dialog_cloud.h"

#include "base/bind.h"
#include "base/macros.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/account_consistency_mode_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "components/cloud_devices/common/cloud_devices_urls.h"
#include "components/google/core/common/google_util.h"
#include "components/signin/public/base/signin_metrics.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"

namespace print_dialog_cloud {

namespace {

class SignInObserver : public content::WebContentsObserver {
 public:
  SignInObserver(content::WebContents* web_contents, base::OnceClosure callback)
      : WebContentsObserver(web_contents), callback_(std::move(callback)) {}

 private:
  // Overridden from content::WebContentsObserver:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override {
    if (!navigation_handle->IsInMainFrame() ||
        !navigation_handle->HasCommitted()) {
      return;
    }

    if (cloud_devices::IsCloudPrintURL(navigation_handle->GetURL())) {
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::BindOnce(&SignInObserver::OnSignIn,
                                    weak_ptr_factory_.GetWeakPtr()));
    }
  }

  void WebContentsDestroyed() override { delete this; }

  void OnSignIn() {
    std::move(callback_).Run();
    if (web_contents())
      web_contents()->Close();
  }

  base::OnceClosure callback_;
  base::WeakPtrFactory<SignInObserver> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(SignInObserver);
};

}  // namespace

void CreateCloudPrintSigninTab(Browser* browser,
                               bool add_account,
                               base::OnceClosure callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (AccountConsistencyModeManager::IsMirrorEnabledForProfile(
          browser->profile())) {
    browser->window()->ShowAvatarBubbleFromAvatarButton(
        add_account ? BrowserWindow::AVATAR_BUBBLE_MODE_ADD_ACCOUNT
                    : BrowserWindow::AVATAR_BUBBLE_MODE_SIGNIN,

        signin_metrics::AccessPoint::ACCESS_POINT_CLOUD_PRINT, false);
  } else {
    GURL url = add_account ? cloud_devices::GetCloudPrintAddAccountURL()
                           : cloud_devices::GetCloudPrintSigninURL();
    content::WebContents* web_contents =
        browser->OpenURL(content::OpenURLParams(
            google_util::AppendGoogleLocaleParam(
                url, g_browser_process->GetApplicationLocale()),
            content::Referrer(), WindowOpenDisposition::NEW_FOREGROUND_TAB,
            ui::PAGE_TRANSITION_AUTO_BOOKMARK, false));
    // This observer will delete itself after destroying the WebContents.
    new SignInObserver(web_contents, std::move(callback));
  }
}

}  // namespace print_dialog_cloud
