// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_BUBBLE_APP_LIST_BUBBLE_VIEW_H_
#define ASH_APP_LIST_BUBBLE_APP_LIST_BUBBLE_VIEW_H_

#include "ash/ash_export.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

namespace aura {
class Window;
}  // namespace aura

namespace ash {

class BubbleAppsPage;
class BubbleAssistantPage;
class BubbleSearchPage;
enum class ShelfAlignment;

// Contains the views for the bubble version of the launcher.
class ASH_EXPORT AppListBubbleView : public views::BubbleDialogDelegateView {
 public:
  // Creates the bubble on the display for `root_window`. Anchors the bubble to
  // a corner of the screen based on `shelf_alignment`.
  AppListBubbleView(aura::Window* root_window, ShelfAlignment shelf_alignment);
  AppListBubbleView(const AppListBubbleView&) = delete;
  AppListBubbleView& operator=(const AppListBubbleView&) = delete;
  ~AppListBubbleView() override;

  // views::View:
  gfx::Size CalculatePreferredSize() const override;

 private:
  // Flips between the apps, search and assistant pages.
  // TODO(https://crbug.com/1204551): Delete this when search box is hooked up.
  void FlipPage();

  // TODO(https://crbug.com/1204551): Delete this when search box is hooked up.
  int visible_page_ = 0;

  BubbleAppsPage* apps_page_ = nullptr;
  BubbleSearchPage* search_page_ = nullptr;
  BubbleAssistantPage* assistant_page_ = nullptr;
};

}  // namespace ash

#endif  // ASH_APP_LIST_BUBBLE_APP_LIST_BUBBLE_VIEW_H_
