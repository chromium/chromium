// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEFAULT_BROWSER_DEFAULT_BROWSER_MONITOR_H_
#define CHROME_BROWSER_DEFAULT_BROWSER_DEFAULT_BROWSER_MONITOR_H_

#include <memory>

#include "base/callback_list.h"
#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "build/build_config.h"

class RegistryWatcher;

namespace default_browser {

// Provides a platform-agnostic object for observing changes to the system's
// default browser setting. Observers are notified on the DefaultBrowserManager
// thread.
class DefaultBrowserMonitor {
 public:
  DefaultBrowserMonitor();
  ~DefaultBrowserMonitor();

  DefaultBrowserMonitor(const DefaultBrowserMonitor&) = delete;
  const DefaultBrowserMonitor& operator=(const DefaultBrowserMonitor&) = delete;

  // Starts the monitoring process. The implementation will be platform
  // specific.
  void StartMonitor();

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

  // The monitor owns the watcher.
  std::unique_ptr<RegistryWatcher> registry_watcher_;
#endif  //  BUILDFLAG(IS_WIN)

  base::RepeatingClosureList callback_list_;

  // Enforces that this class is used on same sequence.
  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<DefaultBrowserMonitor> weak_ptr_factory_{this};
};

}  // namespace default_browser

#endif  // CHROME_BROWSER_DEFAULT_BROWSER_DEFAULT_BROWSER_MONITOR_H_
