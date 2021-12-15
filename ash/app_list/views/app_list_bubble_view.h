// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_APP_LIST_BUBBLE_VIEW_H_
#define ASH_APP_LIST_VIEWS_APP_LIST_BUBBLE_VIEW_H_

#include <memory>

#include "ash/app_list/views/app_list_folder_controller.h"
#include "ash/ash_export.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "ash/search_box/search_box_view_delegate.h"
#include "base/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "ui/views/view.h"

namespace ash {

class ApplicationDragAndDropHost;
class AppListA11yAnnouncer;
class AppListBubbleAppsPage;
class AppListBubbleAssistantPage;
class AppListBubbleSearchPage;
class AppListFolderItem;
class AppListFolderView;
class AppListViewDelegate;
class FolderBackgroundView;
class SearchBoxView;
class SearchResultPageDialogController;
class ViewShadow;

// Contains the views for the bubble version of the launcher. It looks like a
// system tray bubble. It does not derive from TrayBubbleView because it takes
// focus by default, uses a different EventHandler for closing, and isn't tied
// to the system tray area.
class ASH_EXPORT AppListBubbleView : public views::View,
                                     public SearchBoxViewDelegate,
                                     public AppListFolderController {
 public:
  AppListBubbleView(AppListViewDelegate* view_delegate,
                    ApplicationDragAndDropHost* drag_and_drop_host);
  AppListBubbleView(const AppListBubbleView&) = delete;
  AppListBubbleView& operator=(const AppListBubbleView&) = delete;
  ~AppListBubbleView() override;

  // If |drag_and_drop_host| is not nullptr it will be called upon drag and drop
  // operations outside the app list (e.g. to the shelf).
  void SetDragAndDropHostOfCurrentAppList(
      ApplicationDragAndDropHost* drag_and_drop_host);

  // Starts the bubble show animation.
  void StartShowAnimation();

  // Starts the bubble hide animation.
  void StartHideAnimation(base::OnceClosure on_hide_animation_ended);

  // Aborts all layer animations started by StartShowAnimation() or
  // StartHideAnimation(). This invokes their cleanup callbacks.
  void AbortAllAnimations();

  // Handles back action if it we have a use for it besides dismissing.
  bool Back();

  // Shows a sub-page.
  void ShowPage(AppListBubblePage page);

  // Returns true if the assistant page is showing.
  bool IsShowingEmbeddedAssistantUI() const;

  // Shows the assistant page.
  void ShowEmbeddedAssistantUI();

  // Returns the required height for this view in DIPs to show all apps in the
  // apps grid. Used for computing the bubble height on large screens.
  int GetHeightToFitAllApps() const;

  // views::View:
  const char* GetClassName() const override;
  bool AcceleratorPressed(const ui::Accelerator& accelerator) override;
  void OnThemeChanged() override;
  void Layout() override;

  // SearchBoxViewDelegate:
  void QueryChanged(SearchBoxViewBase* sender) override;
  void AssistantButtonPressed() override;
  void BackButtonPressed() override {}
  void CloseButtonPressed() override;
  void ActiveChanged(SearchBoxViewBase* sender) override {}
  void OnSearchBoxKeyEvent(ui::KeyEvent* event) override;
  bool CanSelectSearchResults() override;

  // AppListFolderController:
  void ShowFolderForItemView(AppListItemView* folder_item_view) override;
  void ShowApps(AppListItemView* folder_item_view, bool select_folder) override;
  void ReparentFolderItemTransit(AppListFolderItem* folder_item) override;
  void ReparentDragEnded() override;

  AppListBubbleAppsPage* apps_page() { return apps_page_; }

  ViewShadow* view_shadow_for_test() { return view_shadow_.get(); }
  views::View* separator_for_test() { return separator_; }
  bool showing_folder_for_test() { return showing_folder_; }
  AppListBubbleAppsPage* apps_page_for_test() { return apps_page_; }

 private:
  friend class AppListTestHelper;
  friend class AssistantTestApiImpl;

  // Initializes the main contents (search box, apps page, and search page).
  void InitContentsView(ApplicationDragAndDropHost* drag_and_drop_host);

  // Initializes the folder view, which appears on top of all other views.
  void InitFolderView(ApplicationDragAndDropHost* drag_and_drop_host);

  // Makes the root apps grid view and other top-level views unfocusable if
  // `disabled` is true, such that focus is contained in the folder view.
  void DisableFocusForShowingActiveFolder(bool disabled);

  // Called when the show animation ends or aborts.
  void OnShowAnimationEnded(const gfx::Rect& layer_bounds);

  // Called when the hide animation ends or aborts.
  void OnHideAnimationEnded(const gfx::Rect& layer_bounds);

  AppListViewDelegate* const view_delegate_;

  std::unique_ptr<AppListA11yAnnouncer> a11y_announcer_;

  // Controller for showing a modal dialog in search results page.
  std::unique_ptr<SearchResultPageDialogController>
      search_page_dialog_controller_;

  std::unique_ptr<ViewShadow> view_shadow_;
  SearchBoxView* search_box_view_ = nullptr;
  views::View* separator_ = nullptr;
  AppListBubbleAppsPage* apps_page_ = nullptr;
  AppListBubbleSearchPage* search_page_ = nullptr;
  AppListBubbleAssistantPage* assistant_page_ = nullptr;

  // Lives in this class because it can overlap the search box.
  AppListFolderView* folder_view_ = nullptr;

  // Used to close an open folder view.
  FolderBackgroundView* folder_background_view_ = nullptr;

  // Whether we're showing the folder view. This is different from
  // folder_view_->GetVisible() because the view is "visible" but hidden when
  // dragging an item out of a folder.
  bool showing_folder_ = false;

  // Called after the hide animation ends or aborts.
  base::OnceClosure on_hide_animation_ended_;

  base::WeakPtrFactory<AppListBubbleView> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_APP_LIST_VIEWS_APP_LIST_BUBBLE_VIEW_H_
