// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHORTCUT_VIEWER_VIEWS_KEYBOARD_SHORTCUT_VIEW_H_
#define ASH_SHORTCUT_VIEWER_VIEWS_KEYBOARD_SHORTCUT_VIEW_H_

#include <map>
#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/timer/timer.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/widget/widget_delegate.h"

namespace aura {
class Window;
}

namespace views {
class TabbedPane;
class Widget;
}  // namespace views

namespace ash {
enum class ShortcutCategory;
}  // namespace ash

namespace keyboard_shortcut_viewer {

class KeyboardShortcutItemView;
class KSVSearchBoxView;

// The UI container for Ash and Chrome keyboard shortcuts.
class KeyboardShortcutView : public views::WidgetDelegateView {
 public:
  METADATA_HEADER(KeyboardShortcutView);
  KeyboardShortcutView(const KeyboardShortcutView&) = delete;
  KeyboardShortcutView& operator=(const KeyboardShortcutView&) = delete;

  ~KeyboardShortcutView() override;

  // Toggle the Keyboard Shortcut Viewer window.
  // 1. Show the window if it is not open.
  // 2. Activate the window if it is open but not active.
  // 3. Close the window if it is open and active.
  // |context| is used to determine which display to place the Window on.
  static views::Widget* Toggle(aura::Window* context);

  // views::View:
  std::u16string GetAccessibleWindowTitle() const override;
  bool AcceleratorPressed(const ui::Accelerator& accelerator) override;
  void Layout() override;
  gfx::Size CalculatePreferredSize() const override;
  void OnPaint(gfx::Canvas* canvas) override;
  void OnThemeChanged() override;

  // Handles search box query changes.
  void QueryChanged(const std::u16string& query);

 private:
  friend class KeyboardShortcutViewTest;

  KeyboardShortcutView();

  void InitViews();

  // Initialize |categories_tabbed_pane_| with category tabs and containers of
  // |shortcut_views_|, called on construction and when exiting search mode.
  // If |initial_category| has value, we will initialize the specified category,
  // otherwise all the categories will be intialized.
  void InitCategoriesTabbedPane(
      absl::optional<ash::ShortcutCategory> initial_category);

  // Update views' layout based on search box status.
  void UpdateViewsLayout();

  // Show search results in |search_results_container_|.
  void ShowSearchResults(const std::u16string& search_query);

  // views::WidgetDelegate:
  views::ClientView* CreateClientView(views::Widget* widget) override;

  static KeyboardShortcutView* GetInstanceForTesting();
  size_t GetTabCountForTesting() const;
  const std::vector<std::unique_ptr<KeyboardShortcutItemView>>&
  GetShortcutViewsForTesting() const;
  KSVSearchBoxView* GetSearchBoxViewForTesting();
  const std::vector<KeyboardShortcutItemView*>&
  GetFoundShortcutItemsForTesting() const;

  // Determine correct color based on dark mode flag and preference.
  void UpdateBackgroundColor();
  void UpdateActiveAndInactiveFrameColor();

  // Owned by views hierarchy.
  // The container for category tabs and lists of KeyboardShortcutItemViews.
  raw_ptr<views::TabbedPane, ExperimentalAsh> categories_tabbed_pane_ = nullptr;
  // The container for KeyboardShortcutItemViews matching a user's query.
  raw_ptr<views::View, ExperimentalAsh> search_results_container_ = nullptr;

  // Owned by views hierarchy.
  raw_ptr<KSVSearchBoxView, ExperimentalAsh> search_box_view_ = nullptr;

  // Contains all the shortcut item views from all categories. This list is also
  // used for searching. The views are not owned by the Views hierarchy to avoid
  // KeyboardShortcutItemView layout when switching between tabs and search.
  std::vector<std::unique_ptr<KeyboardShortcutItemView>> shortcut_views_;

  // Contains all the found shortcut items.
  std::vector<KeyboardShortcutItemView*> found_shortcut_items_;

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
};

}  // namespace keyboard_shortcut_viewer

#endif  // ASH_SHORTCUT_VIEWER_VIEWS_KEYBOARD_SHORTCUT_VIEW_H_
