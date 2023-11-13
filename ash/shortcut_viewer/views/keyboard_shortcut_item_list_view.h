// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHORTCUT_VIEWER_VIEWS_KEYBOARD_SHORTCUT_ITEM_LIST_VIEW_H_
#define ASH_SHORTCUT_VIEWER_VIEWS_KEYBOARD_SHORTCUT_ITEM_LIST_VIEW_H_

#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace keyboard_shortcut_viewer {

// Displays a list of KeyboardShortcutItemView. In search result page, all
// KeyboardShortcutItemView are grouped by ShortcutCategory and can be scrolled
// in a single page. A text label to indicate the ShortcutCategory will be
// diplayed at the beginning of the group of KeyboardShortcutItemView.
class KeyboardShortcutItemListView : public views::View {
  METADATA_HEADER(KeyboardShortcutItemListView, views::View)

 public:
  KeyboardShortcutItemListView();

  KeyboardShortcutItemListView(const KeyboardShortcutItemListView&) = delete;
  KeyboardShortcutItemListView& operator=(const KeyboardShortcutItemListView&) =
      delete;

  ~KeyboardShortcutItemListView() override = default;

  // In search result page, a text label is added at the beginning of the group
  // of KeyboardShortcutItemView to indicate the ShortcutCategory.
  void AddCategoryLabel(const std::u16string& text);

  // Add a horizontal line to separate the KeyboardShortcutItemView. The last
  // item in the list is not followed by the horizontal line.
  void AddHorizontalSeparator();
};

}  // namespace keyboard_shortcut_viewer

#endif  // ASH_SHORTCUT_VIEWER_VIEWS_KEYBOARD_SHORTCUT_ITEM_LIST_VIEW_H_
