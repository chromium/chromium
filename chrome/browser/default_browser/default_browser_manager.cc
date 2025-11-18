// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/default_browser/default_browser_manager.h"

#include <utility>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "chrome/browser/default_browser/default_browser_monitor.h"
#include "chrome/browser/default_browser/setters/shell_integration_default_browser_setter.h"
#include "chrome/browser/shell_integration.h"

namespace default_browser {

DefaultBrowserManager::DefaultBrowserManager() = default;

DefaultBrowserManager::~DefaultBrowserManager() = default;

// Static
void DefaultBrowserManager::GetDefaultBrowserState(
    DefaultBrowserCheckCompletionCallback callback) {
  auto worker = base::MakeRefCounted<shell_integration::DefaultBrowserWorker>();
  worker->StartCheckIsDefault(std::move(callback));
}

// Static
std::unique_ptr<DefaultBrowserController>
DefaultBrowserManager::CreateControllerFor(
    DefaultBrowserEntrypointType entrypoint) {
  return std::make_unique<DefaultBrowserController>(
      std::make_unique<ShellIntegrationDefaultBrowserSetter>(), entrypoint);
}

base::CallbackListSubscription
DefaultBrowserManager::RegisterDefaultBrowserChanged(
    base::RepeatingClosure callback) {
  if (!monitor_) {
    monitor_ = std::make_unique<DefaultBrowserMonitor>();
    monitor_->StartMonitor();
  }

  return monitor_->RegisterDefaultBrowserChanged(std::move(callback));
}

}  // namespace default_browser
