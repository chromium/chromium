// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHORTCUT_VIEWER_VIEWS_KEYBOARD_SHORTCUT_ITEM_VIEW_H_
#define ASH_SHORTCUT_VIEWER_VIEWS_KEYBOARD_SHORTCUT_ITEM_VIEW_H_

#include "ash/public/cpp/keyboard_shortcut_item.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"

#include "ui/views/view.h"

namespace views {
class StyledLabel;
}  //  namespace views

namespace keyboard_shortcut_viewer {

// A view that displays a shortcut metadata.
class KeyboardShortcutItemView : public views::View {
 public:
  METADATA_HEADER(KeyboardShortcutItemView);
  KeyboardShortcutItemView(const ash::KeyboardShortcutItem& item,
                           ash::ShortcutCategory category);

  KeyboardShortcutItemView(const KeyboardShortcutItemView&) = delete;
  KeyboardShortcutItemView& operator=(const KeyboardShortcutItemView&) = delete;

  ~KeyboardShortcutItemView() override = default;

  // views::View:
  int GetHeightForWidth(int w) const override;
  void Layout() override;

  views::StyledLabel* description_label_view() {
    return description_label_view_;
  }

  views::StyledLabel* shortcut_label_view() { return shortcut_label_view_; }

  ash::ShortcutCategory category() const { return category_; }

  const ash::KeyboardShortcutItem* shortcut_item() const {
    return shortcut_item_;
  }

  // Clear the cache.
  static void ClearKeycodeToString16Cache();

 private:
  // A cache to avoid repeatly looking up std::u16string from ui::KeyboardCode.
  // Currently the Keyboard Shortcut Viewer (KSV) will not refresh its contents
  // when keyboard layout changes. The users must restart KSV again to get new
  // keys for the new layout. Also since GetStringForKeyboardCode is only called
  // for KSV to create the strings in the initialization process, clearing the
  // cache is not necessary when keyboard layout changes.
  static std::map<ui::KeyboardCode, std::u16string>*
  GetKeycodeToString16Cache();

  // Calculates how to layout child views for the given |width|.
  void CalculateLayout(int width) const;

  // Not owned. Pointer to the keyboard shortcut item.
  raw_ptr<const ash::KeyboardShortcutItem, ExperimentalAsh> shortcut_item_;

  const ash::ShortcutCategory category_;

  // View of the text describing what action the shortcut performs.
  raw_ptr<views::StyledLabel, ExperimentalAsh> description_label_view_;

  // View of the text listing the keys making up the shortcut.
  raw_ptr<views::StyledLabel, ExperimentalAsh> shortcut_label_view_;

  // Saves the results of the last CalculateLayout() call to avoid repeated
  // calculation.
  mutable gfx::Rect description_bounds_;
  mutable gfx::Rect shortcut_bounds_;
  mutable gfx::Size calculated_size_;
};

}  // namespace keyboard_shortcut_viewer

#endif  // ASH_SHORTCUT_VIEWER_VIEWS_KEYBOARD_SHORTCUT_ITEM_VIEW_H_
