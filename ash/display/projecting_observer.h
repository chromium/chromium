// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_DISPLAY_PROJECTING_OBSERVER_H_
#define ASH_DISPLAY_PROJECTING_OBSERVER_H_

#include "ash/ash_export.h"
#include "ash/shell_observer.h"
#include "base/memory/raw_ptr.h"
#include "ui/display/manager/display_configurator.h"

namespace ash {

class ASH_EXPORT ProjectingObserver
    : public display::DisplayConfigurator::Observer,
      public ShellObserver {
 public:
  // |display_configurator| must outlive this instance. May be null in tests.
  explicit ProjectingObserver(
      display::DisplayConfigurator* display_configurator);

  ProjectingObserver(const ProjectingObserver&) = delete;
  ProjectingObserver& operator=(const ProjectingObserver&) = delete;

  ~ProjectingObserver() override;

  // DisplayConfigurator::Observer implementation:
  void OnDisplayConfigurationChanged(
      const display::DisplayConfigurator::DisplayStateList& outputs) override;

  // ash::ShellObserver implementation:
  void OnCastingSessionStartedOrStopped(bool started) override;

  // Returns whether device is projecting (docked).
  bool is_projecting() const { return is_projecting_; }

 private:
  friend class ProjectingObserverTest;

  // Sends the current projecting state to power manager.
  void SetIsProjecting();

  raw_ptr<display::DisplayConfigurator> display_configurator_;  // Unowned

  // True if at least one output is internal. This value is updated when
  // |OnDisplayModeChanged| is called.
  bool has_internal_output_ = false;

  // Keeps track of the number of connected outputs.
  int output_count_ = 0;

  // Number of outstanding casting sessions.
  int casting_session_count_ = 0;

  bool is_projecting_ = false;
};

}  // namespace ash

#endif  // ASH_DISPLAY_PROJECTING_OBSERVER_H_
