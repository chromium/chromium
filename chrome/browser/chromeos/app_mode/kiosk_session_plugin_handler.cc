// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/app_mode/kiosk_session_plugin_handler.h"

#include <algorithm>

#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/chromeos/app_mode/kiosk_session_plugin_handler_delegate.h"

namespace chromeos {

KioskSessionPluginHandler::Observer::Observer(content::WebContents* contents,
                                              KioskSessionPluginHandler* owner)
    : content::WebContentsObserver(contents), owner_(owner) {}

KioskSessionPluginHandler::Observer::~Observer() = default;

std::set<int> KioskSessionPluginHandler::Observer::GetHungPluginsForTesting()
    const {
  return hung_plugins_;
}

void KioskSessionPluginHandler::Observer::OnHungWaitTimer() {
  owner_->OnPluginHung(hung_plugins_);
}

void KioskSessionPluginHandler::Observer::WebContentsDestroyed() {
  owner_->OnWebContentsDestroyed(this);
}

std::vector<KioskSessionPluginHandler::Observer*>
KioskSessionPluginHandler::GetWatchersForTesting() const {
  std::vector<KioskSessionPluginHandler::Observer*> observers;
  for (const auto& watcher : watchers_) {
    observers.push_back(watcher.get());
  }
  return observers;
}

KioskSessionPluginHandler::KioskSessionPluginHandler(
    KioskSessionPluginHandlerDelegate* delegate)
    : delegate_(delegate) {}

KioskSessionPluginHandler::~KioskSessionPluginHandler() = default;

void KioskSessionPluginHandler::Observe(content::WebContents* contents) {
  watchers_.push_back(std::make_unique<Observer>(contents, this));
}

void KioskSessionPluginHandler::OnPluginCrashed(
    const base::FilePath& plugin_path) {
  delegate_->OnPluginCrashed(plugin_path);
}

void KioskSessionPluginHandler::OnPluginHung(
    const std::set<int>& hung_plugins) {
  delegate_->OnPluginHung(hung_plugins);
}

void KioskSessionPluginHandler::OnWebContentsDestroyed(Observer* observer) {
  for (auto it = watchers_.begin(); it != watchers_.end(); ++it) {
    if (it->get() == observer) {
      it->release();
      watchers_.erase(it);

      // Schedule the delete later after `observer`'s WebContentsDestroyed
      // finishes.
      base::SingleThreadTaskRunner::GetCurrentDefault()->DeleteSoon(FROM_HERE,
                                                                    observer);

      return;
    }
  }
}

}  // namespace chromeos
