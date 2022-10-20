// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_IME_IME_DETAILED_VIEW_H_
#define ASH_SYSTEM_IME_IME_DETAILED_VIEW_H_

#include <stddef.h>

#include "ash/public/cpp/ime_info.h"
#include "ash/system/ime_menu/ime_list_view.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace views {
class ImageView;
}  // namespace views

namespace ash {
class ImeControllerImpl;

// A list of available IMEs shown in the IME detailed view of the system menu,
// along with other items in the title row (a settings button and optional
// enterprise-controlled icon).
class IMEDetailedView : public ImeListView {
 public:
  METADATA_HEADER(IMEDetailedView);

  IMEDetailedView(DetailedViewDelegate* delegate,
                  ImeControllerImpl* ime_controller);
  IMEDetailedView(const IMEDetailedView&) = delete;
  IMEDetailedView& operator=(const IMEDetailedView&) = delete;
  ~IMEDetailedView() override = default;

  // ImeListView:
  void Update(const std::string& current_ime_id,
              const std::vector<ImeInfo>& list,
              const std::vector<ImeMenuItem>& property_list,
              bool show_keyboard_toggle,
              SingleImeBehavior single_ime_behavior) override;

  views::ImageView* controlled_setting_icon() {
    return controlled_setting_icon_;
  }

 private:
  // ImeListView:
  void ResetImeListView() override;
  void CreateExtraTitleRowButtons() override;
  void ShowSettings();

  ImeControllerImpl* const ime_controller_;

  // Gear icon that takes the user to IME settings.
  views::Button* settings_button_ = nullptr;

  // This icon says that the IMEs are managed by policy.
  views::ImageView* controlled_setting_icon_ = nullptr;
};

}  // namespace ash

#endif  // ASH_SYSTEM_IME_IME_DETAILED_VIEW_H_
