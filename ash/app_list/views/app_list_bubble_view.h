// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_APP_LIST_BUBBLE_VIEW_H_
#define ASH_APP_LIST_VIEWS_APP_LIST_BUBBLE_VIEW_H_

#include <memory>
#include <set>

#include "ash/app_list/app_list_view_provider.h"
#include "ash/app_list/views/app_list_folder_controller.h"
#include "ash/app_list/views/search_box_view_delegate.h"
#include "ash/ash_export.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace views {
class ViewShadow;
}  // namespace views

namespace ash {

class AppListA11yAnnouncer;
class AppListBubbleAppsPage;
class AppListBubbleAppsCollectionsPage;
class AppListBubbleAssistantPage;
class AppListBubbleSearchPage;
class AppListFolderItem;
class AppListFolderView;
class AppListViewDelegate;
class ButtonFocusSkipper;
class FolderBackgroundView;
class SearchBoxView;
class SearchResultPageDialogController;

// Contains the views for the bubble version of the launcher. It looks like a
// system tray bubble. It does not derive from TrayBubbleView because it takes
// focus by default, uses a different EventHandler for closing, and isn't tied
// to the system tray area.
class ASH_EXPORT AppListBubbleView : public views::View,
                                     public SearchBoxViewDelegate,
                                     public AppListFolderController {
  METADATA_HEADER(AppListBubbleView, views::View)

 public:
  explicit AppListBubbleView(AppListViewDelegate* view_delegate);
  AppListBubbleView(const AppListBubbleView&) = delete;
  AppListBubbleView& operator=(const AppListBubbleView&) = delete;
  ~AppListBubbleView() override;

  // Updates continue tasks and recent apps.
  void UpdateSuggestions();

  // Starts the bubble show animation. Pass `is_side_shelf` true for left or
  // right aligned shelf.
  void StartShowAnimation(bool is_side_shelf);

  // Starts the bubble hide animation. Pass `is_side_shelf` true for left or
  // right aligned shelf. `on_hide_animation_ended` is called on end or abort.
  void StartHideAnimation(bool is_side_shelf,
                          base::OnceClosure on_hide_animation_ended);

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

  // Updates the continue section visibility based on user preference.
  void UpdateContinueSectionVisibility();

  // Handles `AppListController::UpdateAppListWithNewSortingOrder()` for the
  // app list bubble view.
  void UpdateForNewSortingOrder(
      const std::optional<AppListSortOrder>& new_order,
      bool animate,
      base::OnceClosure update_position_closure);

  // views::View:
  bool AcceleratorPressed(const ui::Accelerator& accelerator) override;
  void Layout(PassKey) override;
  bool GetDropFormats(int* formats,
                      std::set<ui::ClipboardFormatType>* format_types) override;
  bool CanDrop(const OSExchangeData& data) override;
  int OnDragUpdated(const ui::DropTargetEvent& event) override;
  void OnDragEntered(const ui::DropTargetEvent& event) override;
  void OnDragExited() override;
  DropCallback GetDropCallback(const ui::DropTargetEvent& event) override;

  // SearchBoxViewDelegate:
  void QueryChanged(const std::u16string& trimmed_query,
                    bool initiated_by_user) override;
  void AssistantButtonPressed() override;
  void CloseButtonPressed() override;
  void ActiveChanged(SearchBoxViewBase* sender) override {}
  void OnSearchBoxKeyEvent(ui::KeyEvent* event) override;
  bool CanSelectSearchResults() override;

  // AppListFolderController:
  void ShowFolderForItemView(AppListItemView* folder_item_view,
                             bool focus_name_input,
                             base::OnceClosure hide_callback) override;
  void ShowApps(AppListItemView* folder_item_view, bool select_folder) override;
  void ReparentFolderItemTransit(AppListFolderItem* folder_item) override;
  void ReparentDragEnded() override;

  // Initialize Assistant UIs for bubble view. Assistant UIs
  // (AppListAssistantMainStage, SuggestionContainerView) expect that their
  // OnUiVisibilityChanged methods get called via value update in
  // AssistantUiModel.
  //
  // But it does not happen for bubble view as AppListBubblePresenter have an
  // async call for OnZeroStateSearchDone. AppListBubbleView is instantiated
  // after the async call and those UIs will miss the event.
  //
  // This is a helper method to manually trigger the UI initialization.
  //
  // This method is designed to be explicitly called from AppListBubblePresenter
  // (i.e. instead of doing this in the constructor of AppListBubbleView) to
  // make the intention clear.
  //
  // TODO(b/239754561): Clean up: refactor Assistant UI initialization
  void InitializeUIForBubbleView();

  AppListBubblePage current_page_for_test() { return current_page_; }
  views::ViewShadow* view_shadow_for_test() { return view_shadow_.get(); }
  SearchBoxView* search_box_view_for_test() { return search_box_view_; }
  views::View* separator_for_test() { return separator_; }
  bool showing_folder_for_test() { return showing_folder_; }
  AppListBubbleAppsPage* apps_page_for_test() { return apps_page_; }
  AppListBubbleSearchPage* search_page() { return search_page_; }
  AppListFolderView* folder_view_for_test() { return folder_view_; }

 private:
  friend class AppListTestHelper;
  friend class AssistantTestApiImpl;

  // Initializes the main contents (search box, apps page, and search page).
  void InitContentsView();

  // Initializes the folder view, which appears on top of all other views.
  void InitFolderView();

  // Makes the root apps grid view and other top-level views unfocusable if
  // `disabled` is true, such that focus is contained in the folder view.
  void DisableFocusForShowingActiveFolder(bool disabled);

  // Called when the show animation ends or aborts.
  void OnShowAnimationEnded(const gfx::Rect& layer_bounds);

  // Called when the hide animation ends or aborts.v
  void OnHideAnimationEnded(const gfx::Rect& layer_bounds);

  // Hides the folder view if it's currently shown. It can be called if the
  // folder is not currently shown.
  // `animate` - Whether the folder view should be hidden using an animation.
  // `hide_for_reparent` - Whether the folder view is being hidden to initiate
  // item reparent user action (e.g. when dragging folder item out of the folder
  // view bounds).
  void HideFolderView(bool animate, bool hide_for_reparent);

  // Called when the reorder animation completes.
  void OnAppListReorderAnimationDone();

  // Focuses the search box if the view is not hiding.
  void MaybeFocusAndActivateSearchBox();

  const raw_ptr<AppListViewDelegate> view_delegate_;

  std::unique_ptr<AppListA11yAnnouncer> a11y_announcer_;

  // Controller for showing a modal dialog in search results page.
  std::unique_ptr<SearchResultPageDialogController>
      search_page_dialog_controller_;

  // Explicitly store the current page because multiple pages can be visible
  // during animations.
  AppListBubblePage current_page_ = AppListBubblePage::kNone;

  std::unique_ptr<views::ViewShadow> view_shadow_;

  // The individual views are implementation details and are intentionally not
  // exposed via getters (except for tests).
  raw_ptr<SearchBoxView> search_box_view_ = nullptr;
  raw_ptr<views::View> separator_ = nullptr;
  raw_ptr<AppListBubbleAppsPage> apps_page_ = nullptr;
  raw_ptr<AppListBubbleSearchPage> search_page_ = nullptr;
  raw_ptr<AppListBubbleAssistantPage> assistant_page_ = nullptr;
  raw_ptr<AppListBubbleAppsCollectionsPage> apps_collections_page_ = nullptr;

  // Lives in this class because it can overlap the search box.
  raw_ptr<AppListFolderView, DanglingUntriaged> folder_view_ = nullptr;

  // Used to close an open folder view.
  raw_ptr<FolderBackgroundView> folder_background_view_ = nullptr;

  // Whether we're showing the folder view. This is different from
  // folder_view_->GetVisible() because the view is "visible" but hidden when
  // dragging an item out of a folder.
  bool showing_folder_ = false;

  // Whether the view is animating hidden.
  bool is_hiding_ = false;

  // Called after the hide animation ends or aborts.
  base::OnceClosure on_hide_animation_ended_;

  // See class comment in .cc file.
  std::unique_ptr<ButtonFocusSkipper> button_focus_skipper_;

  base::WeakPtrFactory<AppListBubbleView> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_APP_LIST_VIEWS_APP_LIST_BUBBLE_VIEW_H_
