// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_IME_TRAY_IME_CHROMEOS_H_
#define ASH_SYSTEM_IME_TRAY_IME_CHROMEOS_H_

#include <stddef.h>

#include "ash/public/mojom/ime_info.mojom.h"
#include "ash/system/ime_menu/ime_list_view.h"
#include "base/macros.h"

namespace views {
class ImageView;
}  // namespace views

namespace ash {
class ImeController;

namespace tray {

// A list of available IMEs shown in the IME detailed view of the system menu,
// along with other items in the title row (a settings button and optional
// enterprise-controlled icon).
class IMEDetailedView : public ImeListView {
 public:
  IMEDetailedView(DetailedViewDelegate* delegate,
                  ImeController* ime_controller);
  ~IMEDetailedView() override = default;

  void Update(const std::string& current_ime_id,
              const std::vector<mojom::ImeInfo>& list,
              const std::vector<mojom::ImeMenuItem>& property_list,
              bool show_keyboard_toggle,
              SingleImeBehavior single_ime_behavior) override;

  views::ImageView* controlled_setting_icon() {
    return controlled_setting_icon_;
  }

 private:
  // ImeListView:
  void ResetImeListView() override;
  void HandleButtonPressed(views::Button* sender,
                           const ui::Event& event) override;
  void CreateExtraTitleRowButtons() override;
  void ShowSettings();
  const char* GetClassName() const override;

  ImeController* const ime_controller_;

  // Gear icon that takes the user to IME settings.
  views::Button* settings_button_ = nullptr;

  // This icon says that the IMEs are managed by policy.
  views::ImageView* controlled_setting_icon_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(IMEDetailedView);
};
}

}  // namespace ash

#endif  // ASH_SYSTEM_IME_TRAY_IME_CHROMEOS_H_
