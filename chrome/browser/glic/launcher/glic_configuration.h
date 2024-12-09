// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_LAUNCHER_GLIC_CONFIGURATION_H_
#define CHROME_BROWSER_GLIC_LAUNCHER_GLIC_CONFIGURATION_H_

#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "components/prefs/pref_change_registrar.h"
#include "ui/base/accelerators/accelerator.h"

class PrefRegistrySimple;
namespace glic {

// This class observes and reports changes to glic prefs such as the
// enabled/disabled state, and the hotkey for launching the UI. Owned by
// GlicBackgroundModeManager.
class GlicConfiguration {
 public:
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnEnabledChanged(bool enabled) {}
    virtual void OnGlobalHotkeyChanged(ui::Accelerator hotkey) {}
  };

  static constexpr char kHotkeyKeyCode[] = "keycode";
  static constexpr char kHotkeyModifiers[] = "modifiers";

  explicit GlicConfiguration(Observer* manager);
  ~GlicConfiguration();

  static void RegisterPrefs(PrefRegistrySimple* registry);

  bool IsEnabled();

  ui::Accelerator GetGlobalHotkey();

 private:
  void OnEnabledPrefChanged();
  void OnGlobalHotkeyPrefChanged();

  PrefChangeRegistrar pref_registrar_;

  raw_ptr<Observer> manager_;
};
}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_LAUNCHER_GLIC_CONFIGURATION_H_
