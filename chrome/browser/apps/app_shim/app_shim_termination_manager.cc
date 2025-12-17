// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_shim/app_shim_termination_manager.h"

#include "base/callback_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/scoped_observation.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/apps/app_shim/app_shim_manager_mac.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/lifetime/application_lifetime_desktop.h"
#include "chrome/browser/ui/browser_window/public/browser_collection_observer.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "chrome/common/mac/app_mode_common.h"

namespace apps {

namespace {

void TerminateIfNoAppWindows() {
  auto* app_shim_manager = AppShimManager::Get();
  if (app_shim_manager && !app_shim_manager->HasNonBookmarkAppWindowsOpen())
    chrome::AttemptExit();
}

class AppShimTerminationManagerImpl : public AppShimTerminationManager,
                                      public BrowserCollectionObserver {
 public:
  AppShimTerminationManagerImpl() {
    browser_collection_observation_.Observe(
        GlobalBrowserCollection::GetInstance());

    closing_all_browsers_subscription_ =
        chrome::AddClosingAllBrowsersCallback(base::BindRepeating(
            &AppShimTerminationManagerImpl::OnClosingAllBrowsersChanged,
            base::Unretained(this)));
  }

  AppShimTerminationManagerImpl(const AppShimTerminationManagerImpl&) = delete;
  AppShimTerminationManagerImpl& operator=(
      const AppShimTerminationManagerImpl&) = delete;
  ~AppShimTerminationManagerImpl() override { NOTREACHED(); }

  // AppShimTerminationManager:
  void MaybeTerminate() override {
    if (!browser_session_running_) {
      // Post this to give AppWindows a chance to remove themselves from the
      // registry.
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(&TerminateIfNoAppWindows));
    }
  }

  bool ShouldRestoreSession() override { return !browser_session_running_; }

  // BrowserCollectionObserver:
  void OnBrowserCreated(BrowserWindowInterface* browser) override {
    browser_session_running_ = true;
  }

 private:
  void OnClosingAllBrowsersChanged(bool closing) {
    browser_session_running_ = !closing;
  }

  base::ScopedObservation<GlobalBrowserCollection, BrowserCollectionObserver>
      browser_collection_observation_{this};
  base::CallbackListSubscription closing_all_browsers_subscription_;
  bool browser_session_running_ = false;
};

}  // namespace

// static
AppShimTerminationManager* AppShimTerminationManager::Get() {
  static base::NoDestructor<AppShimTerminationManagerImpl> instance;
  return instance.get();
}

}  // namespace apps
