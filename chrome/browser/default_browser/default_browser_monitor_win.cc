// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/default_browser/default_browser_monitor.h"

#include <windows.h>

#include <array>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "base/functional/bind.h"
#include "chrome/browser/win/registry_watcher.h"

namespace default_browser {

namespace {

// The registry paths for the default browser choice for http and https.
constexpr std::array<std::wstring_view, 2> kDefaultBrowserSchemes = {
    L"Software\\Microsoft\\Windows\\Shell\\Associations\\UrlAssociations\\http"
    L"\\UserChoice",
    L"Software\\Microsoft\\Windows\\Shell\\Associations\\UrlAssociations\\https"
    L"\\UserChoice"};

}  // namespace

void DefaultBrowserMonitor::StartMonitor() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Create a new watcher, transferring ownership to the unique_ptr.
  // The watcher will call OnDefaultBrowserChanged when a change is detected.
  registry_watcher_ = std::make_unique<RegistryWatcher>(
      std::vector<std::wstring>(kDefaultBrowserSchemes.begin(),
                                kDefaultBrowserSchemes.end()),
      base::BindOnce(&DefaultBrowserMonitor::OnDefaultBrowserChangedWin,
                     // base::Unretained is safe because `this` monitor owns
                     // the `registry_watcher_`. The watcher will be destroyed
                     // before the monitor is, so the callback can't be called
                     // on a dangling pointer.
                     base::Unretained(this)));
}

void DefaultBrowserMonitor::OnDefaultBrowserChangedWin() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // The old watcher has fired and done its job.
  registry_watcher_.reset();

  // Notify observers that a change occurred.
  NotifyObservers();

  // Start a new watch to be notified of the *next* change.
  StartMonitor();
}

}  // namespace default_browser
