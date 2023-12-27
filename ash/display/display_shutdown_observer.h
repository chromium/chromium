// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_DISPLAY_DISPLAY_SHUTDOWN_OBSERVER_H_
#define ASH_DISPLAY_DISPLAY_SHUTDOWN_OBSERVER_H_

#include "ash/public/cpp/session/session_observer.h"
#include "base/memory/raw_ptr.h"

namespace display {
class DisplayConfigurator;
}

namespace ash {

// Adds self as SessionObserver and listens for OnChromeTerminating() on
// behalf of |display_configurator_|.
class DisplayShutdownObserver : public SessionObserver {
 public:
  explicit DisplayShutdownObserver(
      display::DisplayConfigurator* display_configurator);

  DisplayShutdownObserver(const DisplayShutdownObserver&) = delete;
  DisplayShutdownObserver& operator=(const DisplayShutdownObserver&) = delete;

  ~DisplayShutdownObserver() override;

 private:
  // SessionObserver:
  void OnChromeTerminating() override;

  const raw_ptr<display::DisplayConfigurator> display_configurator_;
  ScopedSessionObserver scoped_session_observer_;
};

}  // namespace ash

#endif  // ASH_DISPLAY_DISPLAY_SHUTDOWN_OBSERVER_H_
