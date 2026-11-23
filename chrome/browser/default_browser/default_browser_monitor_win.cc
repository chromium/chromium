// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/default_browser/default_browser_monitor.h"

#include <windows.h>

#include <algorithm>
#include <array>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/time/time.h"
#include "chrome/browser/default_browser/default_browser_features.h"
#include "chrome/browser/win/registry_watcher.h"

namespace default_browser {

namespace {

// Legacy UserChoice registry paths for http and https default browser choices.
constexpr std::array<std::wstring_view, 2> kLegacyUserChoiceKeyPaths = {
    L"Software\\Microsoft\\Windows\\Shell\\Associations\\UrlAssociations\\http"
    L"\\UserChoice",
    L"Software\\Microsoft\\Windows\\Shell\\Associations\\UrlAssociations\\https"
    L"\\UserChoice"};

// Parent UrlAssociations registry paths for http and https. Watching these
// parent keys with subtree notification captures writes to both legacy
// UserChoice and sibling UserChoiceLatest subtrees.
constexpr std::array<std::wstring_view, 2> kUrlAssociationKeyPaths = {
    L"Software\\Microsoft\\Windows\\Shell\\Associations\\UrlAssociations\\http",
    L"Software\\Microsoft\\Windows\\Shell\\Associations\\UrlAssociations"
    L"\\https"};

}  // namespace

void DefaultBrowserMonitor::SetRegistryWatcherFactoryForTesting(
    RegistryWatcherFactory watcher_factory) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  watcher_factory_ = std::move(watcher_factory);
}

void DefaultBrowserMonitor::StartMonitor() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const bool monitor_url_associations =
      IsDefaultBrowserMonitorUrlAssociationsEnabled();
  const auto& key_paths = monitor_url_associations ? kUrlAssociationKeyPaths
                                                   : kLegacyUserChoiceKeyPaths;

  if (registry_watcher_ &&
      (!monitor_url_associations ||
       registry_watcher_->GetRegistryKeyCount() == key_paths.size())) {
    return;
  }

  std::vector<std::wstring> paths(key_paths.begin(), key_paths.end());
  base::OnceClosure on_change =
      base::BindOnce(&DefaultBrowserMonitor::OnDefaultBrowserChangedWin,
                     weak_ptr_factory_.GetWeakPtr());
  registry_watcher_ =
      watcher_factory_
          ? watcher_factory_.Run(paths, std::move(on_change))
          : std::make_unique<RegistryWatcher>(paths, std::move(on_change));
}

void DefaultBrowserMonitor::OnDefaultBrowserChangedWin() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // The old watcher has fired and done its job.
  registry_watcher_.reset();

  if (!IsDefaultBrowserMonitorUrlAssociationsEnabled()) {
    // Notify observers that a change occurred.
    NotifyObservers();

    // Start a new watch to be notified of the *next* change.
    StartMonitor();
    return;
  }

  // Re-arm immediately so subsequent subtree writes during the debounce window
  // are not missed.
  StartMonitor();

  const base::TimeDelta delay =
      std::max(base::TimeDelta(), kMonitorDebounceDelay.Get());
  notify_debounce_timer_.Start(
      FROM_HERE, delay,
      // `base::Unretained(this)` is safe because `this` owns
      // `notify_debounce_timer_`, which cancels any pending task on
      // destruction.
      base::BindOnce(&DefaultBrowserMonitor::NotifyObservers,
                     base::Unretained(this)));
}

}  // namespace default_browser
