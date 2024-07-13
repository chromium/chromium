// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_UNIFIED_QUICK_SETTINGS_FOOTER_H_
#define ASH_SYSTEM_UNIFIED_QUICK_SETTINGS_FOOTER_H_

#include "ash/ash_export.h"
#include "ash/style/pill_button.h"
#include "ash/system/power/power_status.h"
#include "ash/system/unified/power_button.h"
#include "base/memory/raw_ptr.h"
#include "components/prefs/pref_change_registrar.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

class PrefRegistrySimple;

namespace ash {

class IconButton;
class PillButton;
class UnifiedSystemTrayController;

// A base class for both `QsBatteryLabelView` and `QsBatteryIconView`. This view
// is a Jellyroll `PillButton` component that has a different icon label spacing
// and right padding than `BatteryInfoViewBase`. It updates by observing
// `PowerStatus`.
class ASH_EXPORT QsBatteryInfoViewBase : public PillButton,
                                         public PowerStatus::Observer {
  METADATA_HEADER(QsBatteryInfoViewBase, PillButton)

 public:
  explicit QsBatteryInfoViewBase(UnifiedSystemTrayController* controller,
                                 const Type type = Type::kFloatingWithoutIcon,
                                 gfx::VectorIcon* icon = nullptr);
  QsBatteryInfoViewBase(const QsBatteryInfoViewBase&) = delete;
  QsBatteryInfoViewBase& operator=(const QsBatteryInfoViewBase&) = delete;
  ~QsBatteryInfoViewBase() override;

  // Updates the subclass view's ui including button text/background color, text
  // content, icons, etc.It can be applied to changes such as theme change,
  // power status change,etc.
  virtual void Update() = 0;
  // Updates battery icon and text with battery saver mode check.
  void UpdateIconAndText(bool bsm_active = false);
  // Builds the battery icon image.
  void ConfigureIcon(bool bsm_active = false);

 private:
  void ChildPreferredSizeChanged(views::View* child) override;
  void ChildVisibilityChanged(views::View* child) override;

  // PowerStatus::Observer:
  void OnPowerStatusChanged() override;

  // PillButton:
  void OnThemeChanged() override;
};

// A view that shows battery status.
class ASH_EXPORT QsBatteryLabelView : public QsBatteryInfoViewBase {
  METADATA_HEADER(QsBatteryLabelView, QsBatteryInfoViewBase)

 public:
  explicit QsBatteryLabelView(UnifiedSystemTrayController* controller);
  QsBatteryLabelView(const QsBatteryLabelView&) = delete;
  QsBatteryLabelView& operator=(const QsBatteryLabelView&) = delete;
  ~QsBatteryLabelView() override;

 private:
  // QsBatteryInfoViewBase:
  void Update() override;
};

// A view that shows battery icon and charging state when smart charging is
// enabled.
class ASH_EXPORT QsBatteryIconView : public QsBatteryInfoViewBase {
  METADATA_HEADER(QsBatteryIconView, QsBatteryInfoViewBase)

 public:
  explicit QsBatteryIconView(UnifiedSystemTrayController* controller);
  QsBatteryIconView(const QsBatteryIconView&) = delete;
  QsBatteryIconView& operator=(const QsBatteryIconView&) = delete;
  ~QsBatteryIconView() override;

 private:
  // QsBatteryInfoViewBase:
  void Update() override;
};

// The footer view shown on the the bottom of the `QuickSettingsView`.
class ASH_EXPORT QuickSettingsFooter : public views::View {
  METADATA_HEADER(QuickSettingsFooter, views::View)

 public:
  explicit QuickSettingsFooter(UnifiedSystemTrayController* controller);
  QuickSettingsFooter(const QuickSettingsFooter&) = delete;
  QuickSettingsFooter& operator=(const QuickSettingsFooter&) = delete;
  ~QuickSettingsFooter() override;

  // Registers preferences used by this class in the provided `registry`.
  static void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

  PowerButton* power_button_for_testing() { return power_button_; }

 private:
  friend class QuickSettingsFooterTest;

  // Disables/Enables the `settings_button_` based on `kSettingsIconEnabled`
  // pref.
  void UpdateSettingsButtonState();

  // Creates the container to carry the battery and settings button if there's
  // any.
  views::View* CreateEndContainer();

  // Owned.
  raw_ptr<IconButton> settings_button_ = nullptr;

  // Owned by views hierarchy.
  raw_ptr<PowerButton> power_button_ = nullptr;
  raw_ptr<PillButton> sign_out_button_ = nullptr;

  // The registrar used to watch prefs changes.
  PrefChangeRegistrar local_state_pref_change_registrar_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_UNIFIED_QUICK_SETTINGS_FOOTER_H_
