// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_UNIFIED_QUICK_SETTINGS_FOOTER_H_
#define ASH_SYSTEM_UNIFIED_QUICK_SETTINGS_FOOTER_H_

#include "ash/ash_export.h"
#include "components/prefs/pref_change_registrar.h"
#include "ui/views/view.h"

class PrefRegistrySimple;

namespace ash {

class IconButton;
class UnifiedSystemTrayController;

// The footer view shown on the the bottom of the `QuickSettingsView`.
class ASH_EXPORT QuickSettingsFooter : public views::View {
 public:
  METADATA_HEADER(QuickSettingsFooter);

  explicit QuickSettingsFooter(UnifiedSystemTrayController* controller);
  QuickSettingsFooter(const QuickSettingsFooter&) = delete;
  QuickSettingsFooter& operator=(const QuickSettingsFooter&) = delete;
  ~QuickSettingsFooter() override;

  // Registers preferences used by this class in the provided `registry`.
  static void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

 private:
  friend class QuickSettingsFooterTest;

  // Disables/Enables the `settings_button_` based on `kSettingsIconEnabled`
  // pref.
  void UpdateSettingsButtonState();

  // Owned.
  IconButton* settings_button_ = nullptr;

  // The registrar used to watch prefs changes.
  PrefChangeRegistrar local_state_pref_change_registrar_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_UNIFIED_QUICK_SETTINGS_FOOTER_H_
