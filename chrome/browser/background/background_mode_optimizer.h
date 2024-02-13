// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BACKGROUND_BACKGROUND_MODE_OPTIMIZER_H_
#define CHROME_BROWSER_BACKGROUND_BACKGROUND_MODE_OPTIMIZER_H_

#include <memory>

#include "chrome/browser/ui/browser_list_observer.h"
#include "components/keep_alive_registry/keep_alive_state_observer.h"

class Browser;

// BackgroundModeOptimizer is responsible for applying some optimizations to
// save resources (memory) when Chrome runs in background-only mode.
// It tries to restart the browser to release accumulated memory when it
// is considered non distruptive.
class BackgroundModeOptimizer : public KeepAliveStateObserver,
                                BrowserListObserver {
 public:
  // Creates a new BackgroundModeOptimizer. Can return null if optimizations
  // are not supported.
  static std::unique_ptr<BackgroundModeOptimizer> Create();

  BackgroundModeOptimizer(const BackgroundModeOptimizer&) = delete;
  BackgroundModeOptimizer& operator=(const BackgroundModeOptimizer&) = delete;

  ~BackgroundModeOptimizer() override;

  // KeepAliveStateObserver implementation
  void OnKeepAliveStateChanged(bool is_keeping_alive) override;
  void OnKeepAliveRestartStateChanged(bool can_restart) override;

  // BrowserListObserver implementation.
  void OnBrowserAdded(Browser* browser) override;

 private:
  friend class DummyBackgroundModeOptimizer;

  // Use `Create()` above.
  BackgroundModeOptimizer();

  // Calls DoRestart() if the current state of the process allows it.
  void TryBrowserRestart();

  // Stop the browser and restart it in background mode.
  // Virtual for testing purposes.
  virtual void DoRestart();

  bool browser_was_added_ = false;
};

#endif  // CHROME_BROWSER_BACKGROUND_BACKGROUND_MODE_OPTIMIZER_H_
