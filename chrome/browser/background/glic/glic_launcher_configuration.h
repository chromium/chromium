// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BACKGROUND_GLIC_GLIC_LAUNCHER_CONFIGURATION_H_
#define CHROME_BROWSER_BACKGROUND_GLIC_GLIC_LAUNCHER_CONFIGURATION_H_

#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "components/prefs/pref_change_registrar.h"
#include "ui/base/accelerators/accelerator.h"

namespace glic {

// This class observes and reports changes to glic prefs such as the
// enabled/disabled state, and the hotkey for launching the UI. Owned by
// GlicBackgroundModeManager.
class GlicLauncherConfiguration {
 public:
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnEnabledChanged(bool enabled) {}
    virtual void OnGlobalHotkeyChanged(ui::Accelerator hotkey) {}
  };

  explicit GlicLauncherConfiguration(Observer* manager);
  ~GlicLauncherConfiguration();

  // Returns whether the glic launcher is enabled. If `is_default_value` is
  // provided, then it will be updated to reflect if the glic launcher enabled
  // pref is the default value.
  static bool IsEnabled(bool* is_default_value = nullptr);

  static ui::Accelerator GetGlobalHotkey();

  // Returns the default hotkey for the glic launcher.
  static ui::Accelerator GetDefaultHotkey();

 private:
  void OnEnabledPrefChanged();
  void OnGlobalHotkeyPrefChanged();

  PrefChangeRegistrar pref_registrar_;

  raw_ptr<Observer> manager_;
};
}  // namespace glic

#endif  // CHROME_BROWSER_BACKGROUND_GLIC_GLIC_LAUNCHER_CONFIGURATION_H_
