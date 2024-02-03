// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/capture_mode/key_combo_view.h"

#include <iterator>
#include <memory>
#include <string>
#include <vector>

#include "ash/accelerators/keyboard_code_util.h"
#include "ash/capture_mode/key_item_view.h"
#include "ash/resources/vector_icons/vector_icons.h"
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

// Returns the vector icons for keys that have icons on the keyboard.
const gfx::VectorIcon* GetVectorIconForDemoTools(ui::KeyboardCode key_code) {
  switch (key_code) {
    case ui::VKEY_COMMAND:
      return GetSearchOrLauncherVectorIcon();
    case ui::VKEY_ASSISTANT:
      return &kCaptureModeDemoToolsAssistantIcon;
    case ui::VKEY_SETTINGS:
      return &kCaptureModeDemoToolsMenuIcon;
    default:
      return GetVectorIconForKeyboardCode(key_code);
  }
}

std::unique_ptr<KeyItemView> CreateKeyItemView(ui::KeyboardCode key_code) {
  std::unique_ptr key_item_view = std::make_unique<KeyItemView>(key_code);
  const gfx::VectorIcon* vector_icon = GetVectorIconForDemoTools(key_code);
  if (vector_icon) {
    key_item_view->SetIcon(*vector_icon);
  } else {
    std::u16string key_item_string =
        GetStringForKeyboardCode(key_code, /*remap_positional_key=*/false);
    key_item_view->SetText(key_item_string);
  }
  return key_item_view;
}

ui::KeyboardCode DecodeModifier(int modifier) {
  switch (modifier) {
    case ui::EF_CONTROL_DOWN:
      return ui::VKEY_CONTROL;
    case ui::EF_ALT_DOWN:
      return ui::VKEY_MENU;
    case ui::EF_SHIFT_DOWN:
      return ui::VKEY_SHIFT;
    case ui::EF_COMMAND_DOWN:
      return ui::VKEY_COMMAND;
    default:
      return ui::VKEY_UNKNOWN;
  }
}

bool operator>(const ui::KeyboardCode lhs, const ui::KeyboardCode rhs) {
  auto digitize_modifier_key_code = [&](ui::KeyboardCode key_code) {
    switch (key_code) {
      case ui::VKEY_CONTROL:
        return 0;
      case ui::VKEY_MENU:
        return 1;
      case ui::VKEY_SHIFT:
        return 2;
      case ui::VKEY_COMMAND:
        return 3;
      default:
        return 1000;
    }
  };

  return digitize_modifier_key_code(lhs) > digitize_modifier_key_code(rhs);
}

}  // namespace

// -----------------------------------------------------------------------------
// ModifiersContainerView:

// The container view that hosts the modifiers key item views.
class ModifiersContainerView : public views::View {
  METADATA_HEADER(ModifiersContainerView, views::View)

 public:
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

  void RebuildModifiersContainerView(int new_modifiers) {
    // Use XOR to filter out the modifiers that changed, i.e. the 1s in the
    // `diff` bit fields that correspond to the modifiers that changed.
    const int diff = current_modifiers_ ^ new_modifiers;
    for (const auto modifier : {ui::EF_CONTROL_DOWN, ui::EF_ALT_DOWN,
                                ui::EF_SHIFT_DOWN, ui::EF_COMMAND_DOWN}) {
      if ((modifier & diff) == 0) {
        continue;
      }

      const ui::KeyboardCode key_code = DecodeModifier(modifier);

      // Use AND to decide whether we want to do adding or removal operation. If
      // this `modifier` is set in the `new_modifiers` that means it has been
      // recently pressed, which means a new `KeyItemView` needs to be added.
      // Otherwise, it means a key has been recently released, and we should
      // remove its reoccoresponding `KeyItemView`.
      if ((modifier & new_modifiers) != 0) {
        AddModifier(key_code);
      } else {
        RemoveModifier(key_code);
      }
    }

    current_modifiers_ = new_modifiers;
  }

 private:
  void RemoveModifier(ui::KeyboardCode key_code) {
    for (views::View* child : children()) {
      if (static_cast<KeyItemView*>(child)->key_code() == key_code) {
        RemoveChildViewT(child);
        return;
      }
    }
  }

  void AddModifier(ui::KeyboardCode key_code) {
    // We're trying to find the view whose `key_code()` is greater than the
    // given `key_code` so that we can insert a new `KeyItemView` at that index,
    // or at the end if no such view was found. This keeps the modifiers sorted
    // as `Ctrl < Alt < Shift < Search`.
    const auto iter =
        base::ranges::find_if(children(), [key_code](views::View* child) {
          return static_cast<KeyItemView*>(child)->key_code() > key_code;
        });

    AddChildViewAt(CreateKeyItemView(key_code),
                   std::distance(children().begin(), iter));
  }

  int current_modifiers_ = 0;
};

BEGIN_METADATA(ModifiersContainerView)
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
      RemoveChildViewT(non_modifier_view_.get());
      non_modifier_view_ = nullptr;
    }

    if (last_non_modifier_key != ui::VKEY_UNKNOWN) {
      non_modifier_view_ =
          AddChildView(CreateKeyItemView(last_non_modifier_key_));

      // Ensure to trigger a relayout so that the newly added
      // `non_modifier_view_` will be visible.
      InvalidateLayout();
    }
  }
}

std::vector<ui::KeyboardCode> KeyComboView::GetModifierKeycodeVector() const {
  std::vector<ui::KeyboardCode> key_codes;
  base::ranges::for_each(
      modifiers_container_view_->children(), [&](views::View* view) {
        key_codes.push_back(static_cast<KeyItemView*>(view)->key_code());
      });
  return key_codes;
}

BEGIN_METADATA(KeyComboView)
END_METADATA

}  // namespace ash
