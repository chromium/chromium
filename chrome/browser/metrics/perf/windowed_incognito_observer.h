// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_PERF_WINDOWED_INCOGNITO_OBSERVER_H_
#define CHROME_BROWSER_METRICS_PERF_WINDOWED_INCOGNITO_OBSERVER_H_

#include "base/memory/raw_ptr.h"
#include "base/no_destructor.h"
#include "base/sequence_checker.h"
#include "base/synchronization/lock.h"
#include "chrome/browser/ui/browser_list_observer.h"

class Browser;

namespace metrics {

class WindowedIncognitoMonitor;

// WindowedIncognitoObserver provides an interface for getting the current
// status of incognito windows and monitoring any new incognito windows after
// the observer is created. It can be used from any sequence.
//
// Example:
//
// // Initialize the WindowedIncognitoMonitor on the UI thread:
// WindowedIncognitoMonitor::Init();
//
// // Use an observer from any sequence:
// auto observer = WindowedIncognitoMonitor::CreateObserver();
// bool active = observer->IncognitoActive();
// // |active| will be true if there is any active incognito window.
//
// // An incognito window is opened.
// bool launched = observer->IncognitoLaunched();
// EXPECT_TRUE(launched);
class WindowedIncognitoObserver {
 public:
  explicit WindowedIncognitoObserver(WindowedIncognitoMonitor* monitor,
                                     uint64_t incognito_open_count);

  WindowedIncognitoObserver(const WindowedIncognitoObserver&) = delete;
  WindowedIncognitoObserver& operator=(const WindowedIncognitoObserver&) =
      delete;

  virtual ~WindowedIncognitoObserver() = default;

  // Made virtual for override in test.
  virtual bool IncognitoLaunched() const;
  bool IncognitoActive() const;

 private:
  raw_ptr<WindowedIncognitoMonitor> windowed_incognito_monitor_;

  // The number of incognito windows that has been opened when the observer is
  // created.
  uint64_t num_incognito_window_opened_;
};

// WindowedIncognitoMonitor watches for any incognito window being opened or
// closed from the time it is instantiated to the time it is destroyed. The
// monitor is affine to the UI thread: instantiation, destruction and the
// BrowserListObserver callbacks are called on the UI thread. The other methods
// for creating and serving WindowedIncognitoObserver are thread-safe.
class WindowedIncognitoMonitor : public BrowserListObserver {
 public:
  // Must be called on the UI thread before any observers are created.
  static void Init();

  // Returns an instance of WindowedIncognitoObserver that represents the
  // request for monitoring any incognito window launches from now on.
  static std::unique_ptr<WindowedIncognitoObserver> CreateObserver();

  WindowedIncognitoMonitor(const WindowedIncognitoMonitor&) = delete;
  WindowedIncognitoMonitor& operator=(const WindowedIncognitoMonitor&) = delete;

 protected:
  static WindowedIncognitoMonitor* Get();

  friend class base::NoDestructor<WindowedIncognitoMonitor>;
  WindowedIncognitoMonitor();
  ~WindowedIncognitoMonitor() override;

  void RegisterInstance();
  void UnregisterInstance();
  std::unique_ptr<WindowedIncognitoObserver> CreateIncognitoObserver();

  // Making IncognitoActive() and IncognitoLaunched() only accessible from
  // WindowedIncognitoObserver;
  friend class WindowedIncognitoObserver;

  // Returns whether there is any active incognito window.
  bool IncognitoActive() const;
  // Returns whether there was any incognito window opened since an observer was
  // created. Returns true if |num_prev_incognito_opened|, which is passed by
  // the calling observer, is less than |num_incognito_window_opened_| of the
  // monitor.
  bool IncognitoLaunched(uint64_t num_prev_incognito_opened) const;

  // BrowserListObserver implementation.
  void OnBrowserAdded(Browser* browser) override;
  void OnBrowserRemoved(Browser* browser) override;

  // For testing.
  int num_active_incognito_windows() const {
    return num_active_incognito_windows_;
  }
  uint64_t num_incognito_window_opened() const {
    return num_incognito_window_opened_;
  }

 private:
  // Number of initialization attempts.
  int running_sessions_ = 0;

  // Protects access to |num_active_incognito_windows_| and
  // |num_incognito_window_opened_|.
  mutable base::Lock lock_;

  // The number of active incognito window(s).
  int num_active_incognito_windows_;
  // The number of incognito windows we have ever seen.
  uint64_t num_incognito_window_opened_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace metrics

#endif  // CHROME_BROWSER_METRICS_PERF_WINDOWED_INCOGNITO_OBSERVER_H_
