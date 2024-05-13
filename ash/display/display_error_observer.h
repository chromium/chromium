// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_DISPLAY_DISPLAY_ERROR_OBSERVER_H_
#define ASH_DISPLAY_DISPLAY_ERROR_OBSERVER_H_

#include "ash/ash_export.h"
#include "base/compiler_specific.h"
#include "ui/display/manager/display_configurator.h"

namespace ash {

// The class to observe the output failures and shows the error dialog when
// necessary.
class ASH_EXPORT DisplayErrorObserver
    : public display::DisplayConfigurator::Observer {
 public:
  DisplayErrorObserver();

  DisplayErrorObserver(const DisplayErrorObserver&) = delete;
  DisplayErrorObserver& operator=(const DisplayErrorObserver&) = delete;

  ~DisplayErrorObserver() override;

  // display::DisplayConfigurator::Observer overrides:
  void OnDisplayConfigurationChangeFailed(
      const display::DisplayConfigurator::DisplayStateList& displays,
      display::MultipleDisplayState failed_new_state) override;
};

}  // namespace ash

#endif  // ASH_DISPLAY_DISPLAY_ERROR_OBSERVER_H_
