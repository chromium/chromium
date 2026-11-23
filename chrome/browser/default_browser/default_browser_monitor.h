// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEFAULT_BROWSER_DEFAULT_BROWSER_MONITOR_H_
#define CHROME_BROWSER_DEFAULT_BROWSER_DEFAULT_BROWSER_MONITOR_H_

#include <memory>

#include "base/callback_list.h"
#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_WIN)
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/timer/timer.h"
#endif  // BUILDFLAG(IS_WIN)

class RegistryWatcher;

namespace default_browser {

// Provides a platform-agnostic object for observing changes to the system's
// default browser setting. Observers are notified on the DefaultBrowserManager
// thread.
class DefaultBrowserMonitor {
 public:
#if BUILDFLAG(IS_WIN)
  using RegistryWatcherFactory =
      base::RepeatingCallback<std::unique_ptr<RegistryWatcher>(
          const std::vector<std::wstring>& key_paths,
          base::OnceClosure on_change_callback)>;
#endif  // BUILDFLAG(IS_WIN)

  DefaultBrowserMonitor();
  virtual ~DefaultBrowserMonitor();

  DefaultBrowserMonitor(const DefaultBrowserMonitor&) = delete;
  const DefaultBrowserMonitor& operator=(const DefaultBrowserMonitor&) = delete;

  // Starts the monitoring process. The implementation will be platform
  // specific.
  virtual void StartMonitor();

#if BUILDFLAG(IS_WIN)
  void SetRegistryWatcherFactoryForTesting(
      RegistryWatcherFactory watcher_factory);
#endif  // BUILDFLAG(IS_WIN)

  // Registers a callback to be run on the DefaultBrowserManager thread when a
  // change is detected.
  base::CallbackListSubscription RegisterDefaultBrowserChanged(
      base::RepeatingClosure callback);

 protected:
  void NotifyObservers();

 private:
#if BUILDFLAG(IS_WIN)
  // The callback executed on the DefaultBrowserManager thread when the Worker
  // detects a change.
  void OnDefaultBrowserChangedWin();

  // Optional factory injected by unit tests to observe watch registration and
  // trigger change callbacks without opening HKEY_CURRENT_USER handles.
  RegistryWatcherFactory watcher_factory_;

  // The monitor owns the watcher.
  std::unique_ptr<RegistryWatcher> registry_watcher_;

  // Coalesces bursts of subtree writes under UrlAssociations before notifying
  // observers.
  base::OneShotTimer notify_debounce_timer_;
#endif  //  BUILDFLAG(IS_WIN)

  base::RepeatingClosureList callback_list_;

  // Enforces that this class is used on same sequence.
  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<DefaultBrowserMonitor> weak_ptr_factory_{this};
};

}  // namespace default_browser

#endif  // CHROME_BROWSER_DEFAULT_BROWSER_DEFAULT_BROWSER_MONITOR_H_
