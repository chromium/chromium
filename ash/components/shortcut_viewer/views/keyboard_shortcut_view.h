// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_SHORTCUT_VIEWER_VIEWS_KEYBOARD_SHORTCUT_VIEW_H_
#define ASH_COMPONENTS_SHORTCUT_VIEWER_VIEWS_KEYBOARD_SHORTCUT_VIEW_H_

#include <map>
#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/optional.h"
#include "base/timer/timer.h"
#include "ui/chromeos/search_box/search_box_view_delegate.h"
#include "ui/views/widget/widget_delegate.h"

namespace aura {
class Window;
}

namespace views {
class TabbedPane;
class Widget;
}  // namespace views

namespace keyboard_shortcut_viewer {

class KeyboardShortcutItemView;
class KSVSearchBoxView;
enum class ShortcutCategory;

// The UI container for Ash and Chrome keyboard shortcuts.
class KeyboardShortcutView : public views::WidgetDelegateView,
                             public search_box::SearchBoxViewDelegate {
 public:
  ~KeyboardShortcutView() override;

  // Toggle the Keyboard Shortcut Viewer window.
  // 1. Show the window if it is not open.
  // 2. Activate the window if it is open but not active.
  // 3. Close the window if it is open and active.
  // |context| is used to determine which display to place the Window on.
  static views::Widget* Toggle(aura::Window* context);

  // views::View:
  const char* GetClassName() const override;
  ax::mojom::Role GetAccessibleWindowRole() override;
  base::string16 GetAccessibleWindowTitle() const override;
  bool AcceleratorPressed(const ui::Accelerator& accelerator) override;
  void Layout() override;
  gfx::Size CalculatePreferredSize() const override;
  void OnPaint(gfx::Canvas* canvas) override;

  // search_box::SearchBoxViewDelegate:
  void QueryChanged(search_box::SearchBoxViewBase* sender) override;
  void AssistantButtonPressed() override {}
  void BackButtonPressed() override;
  void ActiveChanged(search_box::SearchBoxViewBase* sender) override;
  void SearchBoxFocusChanged(search_box::SearchBoxViewBase* sender) override {}

 private:
  friend class KeyboardShortcutViewTest;

  KeyboardShortcutView();

  void InitViews();

  // Initialize |categories_tabbed_pane_| with category tabs and containers of
  // |shortcut_views_|, called on construction and when exiting search mode.
  // If |initial_category| has value, we will initialize the specified category,
  // otherwise all the categories will be intialized.
  void InitCategoriesTabbedPane(
      base::Optional<ShortcutCategory> initial_category);

  // Update views' layout based on search box status.
  void UpdateViewsLayout(bool is_search_box_active);

  // Show search results in |search_results_container_|.
  void ShowSearchResults(const base::string16& search_query);

  // views::WidgetDelegate:
  bool CanMaximize() const override;
  bool CanMinimize() const override;
  bool CanResize() const override;
  bool ShouldShowWindowTitle() const override;
  views::ClientView* CreateClientView(views::Widget* widget) override;

  static KeyboardShortcutView* GetInstanceForTesting();
  size_t GetTabCountForTesting() const;
  const std::vector<std::unique_ptr<KeyboardShortcutItemView>>&
  GetShortcutViewsForTesting() const;
  KSVSearchBoxView* GetSearchBoxViewForTesting();

  // Owned by views hierarchy.
  // The container for category tabs and lists of KeyboardShortcutItemViews.
  views::TabbedPane* categories_tabbed_pane_ = nullptr;
  // The container for KeyboardShortcutItemViews matching a user's query.
  views::View* search_results_container_ = nullptr;

  // SearchBoxViewBase is a WidgetDelegateView, which owns itself and cannot be
  // deleted from the views hierarchy automatically.
  std::unique_ptr<KSVSearchBoxView> search_box_view_;

  // Contains all the shortcut item views from all categories. This list is also
  // used for searching. The views are not owned by the Views hierarchy to avoid
  // KeyboardShortcutItemView layout when switching between tabs and search.
  std::vector<std::unique_ptr<KeyboardShortcutItemView>> shortcut_views_;

  // An illustration to indicate no search results found. Since this view need
  // to be added and removed frequently from the |search_results_container_|, it
  // is not owned by view hierarchy to avoid recreating it.
  std::unique_ptr<views::View> search_no_result_view_;

  // Cached value of search box text status. When the status changes, need to
  // update views' layout.
  bool is_search_box_empty_ = true;

  // Cached value of active tab index before entering search mode.
  size_t active_tab_index_ = 0;

  // Debounce for search queries.
  base::OneShotTimer debounce_timer_;

  // Ture if need to initialize all the categories.
  // False if only initialize the first category.
  bool needs_init_all_categories_ = false;
  // Indicates if recieved the first OnPaint event. Used to schedule
  // initialization of background panes in the following frame.
  bool did_first_paint_ = false;

  base::WeakPtrFactory<KeyboardShortcutView> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(KeyboardShortcutView);
};

}  // namespace keyboard_shortcut_viewer

#endif  // ASH_COMPONENTS_SHORTCUT_VIEWER_VIEWS_KEYBOARD_SHORTCUT_VIEW_H_
