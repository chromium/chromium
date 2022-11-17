// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/capture_mode/key_combo_view.h"

#include <memory>
#include <string>
#include <vector>

#include "ash/accelerators/keyboard_code_util.h"
#include "ash/capture_mode/key_item_view.h"
#include "key_item_view.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view.h"

namespace ash {

namespace {

constexpr auto kBetweenKeyItemSpace = 8;

std::vector<ui::KeyboardCode> DecodeModifiers(int modifiers) {
  std::vector<ui::KeyboardCode> modifier_vector;
  if (modifiers == 0)
    return modifier_vector;

  if ((modifiers & ui::EF_COMMAND_DOWN) != 0)
    modifier_vector.push_back(ui::VKEY_COMMAND);
  if ((modifiers & ui::EF_CONTROL_DOWN) != 0)
    modifier_vector.push_back(ui::VKEY_CONTROL);
  if ((modifiers & ui::EF_ALT_DOWN) != 0)
    modifier_vector.push_back(ui::VKEY_MENU);
  if ((modifiers & ui::EF_SHIFT_DOWN) != 0)
    modifier_vector.push_back(ui::VKEY_SHIFT);

  return modifier_vector;
}

std::unique_ptr<KeyItemView> CreateKeyItemView(ui::KeyboardCode key_code) {
  std::unique_ptr key_item_view = std::make_unique<KeyItemView>();
  const gfx::VectorIcon* vector_icon = GetVectorIconForKeyboardCode(key_code);
  if (vector_icon) {
    key_item_view->SetIcon(*vector_icon);
  } else {
    std::u16string key_item_string =
        GetStringForKeyboardCode(key_code, /*remap_positional_key=*/false);
    key_item_view->SetText(key_item_string);
  }
  return key_item_view;
}

}  // namespace

// -----------------------------------------------------------------------------
// ModifiersContainerView:

// The container view that hosts the modifiers key item views.
class ModifiersContainerView : public views::View {
 public:
  METADATA_HEADER(ModifiersContainerView);

  explicit ModifiersContainerView() {
    views::BoxLayout* layout_manager =
        SetLayoutManager(std::make_unique<views::BoxLayout>(
            views::BoxLayout::Orientation::kHorizontal, gfx::Insets::VH(0, 0),
            kBetweenKeyItemSpace));
    layout_manager->set_cross_axis_alignment(
        views::BoxLayout::CrossAxisAlignment::kCenter);
  }

  ModifiersContainerView(const ModifiersContainerView&) = delete;
  ModifiersContainerView& operator=(const ModifiersContainerView&) = delete;
  ~ModifiersContainerView() override = default;

  const std::vector<ui::KeyboardCode>& modifier_key_codes() const {
    return modifier_key_codes_;
  }

  // Rebuilds the modifier container view based on the given `modifiers`.
  void RebuildModifiersContainerView(int modifiers) {
    RemoveAllChildViews();
    modifier_key_codes_ = DecodeModifiers(modifiers);
    for (auto key_code : modifier_key_codes_) {
      AddChildView((CreateKeyItemView(key_code)));
    }
  }

 private:
  std::vector<ui::KeyboardCode> modifier_key_codes_;
};

BEGIN_METADATA(ModifiersContainerView, views::View)
END_METADATA

// -----------------------------------------------------------------------------
// KeyComboView:

KeyComboView::KeyComboView() {
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, gfx::Insets(),
      kBetweenKeyItemSpace));
}

KeyComboView::~KeyComboView() = default;

void KeyComboView::RefreshView(int modifiers,
                               ui::KeyboardCode last_non_modifier_key) {
  if (modifiers_ != modifiers) {
    modifiers_ = modifiers;
    if (!modifiers_container_view_) {
      modifiers_container_view_ = AddChildViewAt(
          std::make_unique<ModifiersContainerView>(), /*index=*/0);
    }

    modifiers_container_view_->RebuildModifiersContainerView(modifiers_);
  }

  if (last_non_modifier_key != last_non_modifier_key_) {
    last_non_modifier_key_ = last_non_modifier_key;
    if (non_modifier_view_) {
      RemoveChildViewT(non_modifier_view_);
      non_modifier_view_ = nullptr;
    }

    if (last_non_modifier_key != ui::VKEY_UNKNOWN) {
      non_modifier_view_ =
          AddChildView(CreateKeyItemView(last_non_modifier_key_));
    }
  }
}

std::vector<ui::KeyboardCode> KeyComboView::GetModifierKeycodeVector() const {
  return modifiers_container_view_
             ? modifiers_container_view_->modifier_key_codes()
             : std::vector<ui::KeyboardCode>();
}

BEGIN_METADATA(KeyComboView, views::View)
END_METADATA

}  // namespace ash