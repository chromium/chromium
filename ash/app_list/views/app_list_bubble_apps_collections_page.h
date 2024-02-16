// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_APP_LIST_BUBBLE_APPS_COLLECTIONS_PAGE_H_
#define ASH_APP_LIST_VIEWS_APP_LIST_BUBBLE_APPS_COLLECTIONS_PAGE_H_

#include "ash/ash_export.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "ui/views/view.h"

namespace views {
class ScrollView;
}

namespace ash {
class RoundedScrollBar;

// A page for the bubble / clamshell launcher. Contains a scroll view with
// subsections of apps, one per each category of the Apps Collections. Does not
// include the search box, which is owned by a parent view.
class ASH_EXPORT AppListBubbleAppsCollectionsPage : public views::View {
  METADATA_HEADER(AppListBubbleAppsCollectionsPage, views::View)

 public:
  AppListBubbleAppsCollectionsPage();
  AppListBubbleAppsCollectionsPage(const AppListBubbleAppsCollectionsPage&) =
      delete;
  AppListBubbleAppsCollectionsPage& operator=(
      const AppListBubbleAppsCollectionsPage&) = delete;
  ~AppListBubbleAppsCollectionsPage() override;

  // Starts the animation for showing the page, coming from another page.
  void AnimateShowPage();

  // Starts the animation for hiding the page, going to another page.
  void AnimateHidePage();

  // Aborts all layer animations, which invokes their cleanup callbacks.
  void AbortAllAnimations();

  // Which layer animates is an implementation detail.
  ui::Layer* GetPageAnimationLayerForTest();

  views::ScrollView* scroll_view() { return scroll_view_; }

 private:
  // A callback invoked to update the visibility of the page contents after an
  // animation is done.
  void SetVisibilityAfterAnimation(bool visible);

  raw_ptr<views::ScrollView> scroll_view_ = nullptr;
  raw_ptr<RoundedScrollBar> scroll_bar_ = nullptr;

  base::WeakPtrFactory<AppListBubbleAppsCollectionsPage> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_APP_LIST_VIEWS_APP_LIST_BUBBLE_APPS_COLLECTIONS_PAGE_H_
