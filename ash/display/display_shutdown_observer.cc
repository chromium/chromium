// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/display_shutdown_observer.h"

#include "ui/display/manager/display_configurator.h"

namespace ash {

DisplayShutdownObserver::DisplayShutdownObserver(
    display::DisplayConfigurator* display_configurator)
    : display_configurator_(display_configurator),
      scoped_session_observer_(this) {}

DisplayShutdownObserver::~DisplayShutdownObserver() = default;

void DisplayShutdownObserver::OnChromeTerminating() {
  // Stop handling display configuration events once the shutdown
  // process starts. http://crbug.com/177014.
  display_configurator_->PrepareForExit();
}

}  // namespace ash
