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

namespace {

// Seconds to wait after a plugin hung is detected.
const int kHungWaitSeconds = 20;

}  // namespace

KioskSessionPluginHandler::Observer::Observer(content::WebContents* contents,
                                              KioskSessionPluginHandler* owner)
    : content::WebContentsObserver(contents), owner_(owner) {}

KioskSessionPluginHandler::Observer::~Observer() {}

std::set<int> KioskSessionPluginHandler::Observer::GetHungPluginsForTesting()
    const {
  return hung_plugins_;
}

void KioskSessionPluginHandler::Observer::OnHungWaitTimer() {
  owner_->OnPluginHung(hung_plugins_);
}

void KioskSessionPluginHandler::Observer::PluginCrashed(
    const base::FilePath& plugin_path,
    base::ProcessId plugin_pid) {
  if (!owner_->delegate_->ShouldHandlePlugin(plugin_path)) {
    return;
  }

  owner_->OnPluginCrashed(plugin_path);
}

void KioskSessionPluginHandler::Observer::PluginHungStatusChanged(
    int plugin_child_id,
    const base::FilePath& plugin_path,
    bool is_hung) {
  if (!owner_->delegate_->ShouldHandlePlugin(plugin_path)) {
    return;
  }

  if (is_hung) {
    hung_plugins_.insert(plugin_child_id);
  } else {
    hung_plugins_.erase(plugin_child_id);
  }

  if (!hung_plugins_.empty()) {
    if (!hung_wait_timer_.IsRunning()) {
      hung_wait_timer_.Start(
          FROM_HERE, base::Seconds(kHungWaitSeconds), this,
          &KioskSessionPluginHandler::Observer::OnHungWaitTimer);
    }
  } else {
    hung_wait_timer_.Stop();
  }
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

KioskSessionPluginHandler::~KioskSessionPluginHandler() {}

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
