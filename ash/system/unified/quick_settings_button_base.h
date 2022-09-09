// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_UNIFIED_QUICK_SETTINGS_BUTTON_BASE_H_
#define ASH_SYSTEM_UNIFIED_QUICK_SETTINGS_BUTTON_BASE_H_

#include "ash/ash_export.h"
#include "ash/constants/quick_settings_catalogs.h"
#include "base/memory/weak_ptr.h"
#include "ui/views/controls/button/button.h"

namespace ash {

// The base button delegate for `QuickSettingsView`. This class creates the
// view based on the `BuildButton` function implementation. Based on the
// `button_catalog_name`, it handles the UMA `OnButtonActivated` tracking and
// adds the corresponding `ViewID` to the button.
class ASH_EXPORT QuickSettingsButtonDelegate {
 public:
  QuickSettingsButtonDelegate(
      const QsButtonCatalogName button_catalog_name,
      views::Button::PressedCallback on_button_activated_callback);
  QuickSettingsButtonDelegate(const QuickSettingsButtonDelegate&) = delete;
  QuickSettingsButtonDelegate& operator=(const QuickSettingsButtonDelegate&) =
      delete;
  virtual ~QuickSettingsButtonDelegate();

  // Creates the view based on the `BuildButton` function implementation. This
  // view will be used as a child view of the quick settings view.
  std::unique_ptr<views::Button> CreateButton();

  QsButtonCatalogName catalog_name() const { return catalog_name_; }

 protected:
  // Each sub-class should override `BuildButton()` to build their button, the
  // `callback` is provided by the base class and is the same for all
  // implementations, used to ensure `OnButtonActivated()` is called prior to
  // executing the buttons business logic. Business logic is registered in the
  // base-class ctor as `on_button_activated_callback`.
  //
  // Example:
  //   // QuickSettingsButtonDelegate:
  //   std::unique_ptr<views::Button> BuildButton(
  //       views::Button::PressedCallback callback) override {
  //       return std::make_unique<IconButton>(std::move(callback),
  //                                           IconButton::Type::kSmall,
  //                                           &kUnifiedMenuPowerIcon);
  virtual std::unique_ptr<views::Button> BuildButton(
      views::Button::PressedCallback callback) = 0;

 private:
  // Callback used in `BuildButton()`. It handles 2 things:
  // 1, Track the UMA,
  // 2, Run the passed in `callback_`.
  void OnButtonActivated(const ui::Event& event);

  const QsButtonCatalogName catalog_name_;

  // The passed in `PressedCallback` of the button, called `OnButtonActivated`
  // after metrics emission.
  views::Button::PressedCallback callback_;

  base::WeakPtrFactory<QuickSettingsButtonDelegate> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_UNIFIED_QUICK_SETTINGS_BUTTON_BASE_H_
