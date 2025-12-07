// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/default_browser/default_browser_monitor.h"

#include <utility>

#include "base/functional/callback.h"
#include "chrome/browser/win/registry_watcher.h"

namespace default_browser {

DefaultBrowserMonitor::DefaultBrowserMonitor() = default;

DefaultBrowserMonitor::~DefaultBrowserMonitor() = default;

base::CallbackListSubscription
DefaultBrowserMonitor::RegisterDefaultBrowserChanged(
    base::RepeatingClosure callback) {
  return callback_list_.Add(std::move(callback));
}

void DefaultBrowserMonitor::NotifyObservers() {
  callback_list_.Notify();
}

}  // namespace default_browser
