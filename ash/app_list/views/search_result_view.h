// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_SEARCH_RESULT_VIEW_H_
#define ASH_APP_LIST_VIEWS_SEARCH_RESULT_VIEW_H_

#include <memory>
#include <vector>

#include "ash/app_list/model/search/search_result.h"
#include "ash/app_list/views/search_result_actions_view_delegate.h"
#include "ash/app_list/views/search_result_base_view.h"
#include "ash/ash_export.h"
#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"

namespace views {
class FlexLayoutView;
class ImageView;
class Label;
}  // namespace views

namespace ash {

namespace test {
class SearchResultListViewTest;
}  // namespace test

class AppListViewDelegate;
class MaskedImageView;
class SearchResult;
class SearchResultListView;
class SearchResultPageDialogController;

// Search result view uses `views::FlexLayout` to show results in different
// configurations.
// +---------------------------------------------------------------+
// |`text_container_`                                              |
// | +----------------------+ +----------------------------------+ |
// | |`big_title_container_'| |`body_text_container_`            | |
// | |                      | | +------------------------------+ | |
// | |                      | | |`title_and_details_container_`| | |
// | |                      | | +------------------------------+ | |
// | |                      | | +------------------------------+ | |
// | |                      | | |`keyboard_shortcut_container_`| | |
// | |                      | | +------------------------------+ | |
// | +----------------------+ +----------------------------------+ |
// +---------------------------------------------------------------+
//
// The `title_and_details_container_` has two possible layouts depending on
// `view_type_` and whether `keyboard_shortcut_container_` has results
//
// Layout used when the view_type_ == SearchResultViewType::kDefault OR
// `keyboard_shortcut_layout_override_` is set
//
// +--------------------------------------------------------------------+
// |`title_and_details_container_`                                      |
// | +------------------+  +------------------+  +--------------------+ |
// | |'title_container_'|  |`separator_label_`|  |`details_container_`| |
// | +------------------+  +------------------+  +--------------------+ |
// +--------------------------------------------------------------------+
//
// layout used when view type is SearchResultViewType::kAnswerCard or
// SearchResultViewType::kClassic.
// +-------------------------------+
// |`title_and_details_container_` |
// | +------------------+          |
// | |'title_container_'|          |
// | +------------------+          |
// | +--------------------+        |
// | |'details_container_'|        |
// | +--------------------+        |
// +-------------------------------+

class ASH_EXPORT SearchResultView : public SearchResultBaseView,
                                    public SearchResultActionsViewDelegate {
 public:
  class LabelAndTag;
  enum class SearchResultViewType {
    // The default vew type used for the majority of search results.
    kDefault,
    // The classic view type continues support for pre-BubbleView launcher's
    // search UI.
    kClassic,
    // Inline Answer views are used to directly answer questions posed by the
    // search query.
    kAnswerCard,
  };

  enum class LabelType {
    kBigTitle,
    kTitle,
    kDetails,
    kKeyboardShortcut,
  };

  // Internal class name.
  static const char kViewClassName[];

  SearchResultView(SearchResultListView* list_view,
                   AppListViewDelegate* view_delegate,
                   SearchResultPageDialogController* dialog_controller,
                   SearchResultViewType view_type);

  SearchResultView(const SearchResultView&) = delete;
  SearchResultView& operator=(const SearchResultView&) = delete;

  ~SearchResultView() override;

  // Sets/gets SearchResult displayed by this view.
  void OnResultChanging(SearchResult* new_result) override;
  void OnResultChanged() override;

  void SetSearchResultViewType(SearchResultViewType type);
  SearchResultViewType view_type() { return view_type_; }

  views::LayoutOrientation TitleAndDetailsOrientationForTest();

  // Returns whether the result has changed since this method was last called.
  // Used to determine whether the result should be animated when the result
  // list changes.
  bool GetAndResetResultChanged();

 private:
  friend class test::SearchResultListViewTest;
  friend class SearchResultListView;

  int PreferredHeight() const;
  int PrimaryTextHeight() const;
  int SecondaryTextHeight() const;
  int ActionButtonRightMargin() const;

  std::vector<LabelAndTag> SetupContainerViewForTextVector(
      views::FlexLayoutView* parent,
      const std::vector<SearchResult::TextItem>& text_vector,
      LabelType label_type);
  void UpdateBadgeIcon();
  void UpdateBigTitleContainer();
  void UpdateTitleContainer();
  void UpdateDetailsContainer();
  void UpdateKeyboardShortcutContainer();
  void UpdateRating();

  void StyleLabel(views::Label* label,
                  bool is_title_label,
                  const SearchResult::Tags& tags);
  void StyleBigTitleContainer();
  void StyleTitleContainer();
  void StyleDetailsContainer();
  void StyleKeyboardShortcutContainer();

  // Callback for query suggstion removal confirmation.
  void OnQueryRemovalAccepted(bool accepted);

  // views::View overrides:
  const char* GetClassName() const override;
  gfx::Size CalculatePreferredSize() const override;
  void Layout() override;
  bool OnKeyPressed(const ui::KeyEvent& event) override;
  void PaintButtonContents(gfx::Canvas* canvas) override;
  void OnMouseEntered(const ui::MouseEvent& event) override;
  void OnMouseExited(const ui::MouseEvent& event) override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  void VisibilityChanged(View* starting_from, bool is_visible) override;
  void OnThemeChanged() override;

  // ui::EventHandler overrides:
  void OnGestureEvent(ui::GestureEvent* event) override;

  // SearchResultObserver overrides:
  void OnMetadataChanged() override;

  void OnButtonPressed(const ui::Event& event);

  void SetIconImage(const gfx::ImageSkia& source,
                    views::ImageView* const icon,
                    const gfx::Size& size);

  // SearchResultActionsViewDelegate overrides:
  void OnSearchResultActionActivated(size_t index) override;
  bool IsSearchResultHoveredOrSelected() override;

  // Parent list view. Owned by views hierarchy.
  SearchResultListView* const list_view_;

  AppListViewDelegate* const view_delegate_;

  SearchResultPageDialogController* const dialog_controller_;

  MaskedImageView* icon_ = nullptr;         // Owned by views hierarchy.
  views::ImageView* badge_icon_ = nullptr;  // Owned by views hierarchy.

  views::FlexLayoutView* text_container_ =
      nullptr;  // Owned by views hierarchy.
  views::FlexLayoutView* big_title_container_ =
      nullptr;  // Owned by views hierarchy.
  views::FlexLayoutView* body_text_container_ =
      nullptr;  // Owned by views hierarchy.
  views::FlexLayoutView* title_and_details_container_ =
      nullptr;  // Owned by views hierarchy.
  views::FlexLayoutView* title_container_ =
      nullptr;  // Owned by views hierarchy.
  views::FlexLayoutView* details_container_ =
      nullptr;  // Owned by views hierarchy.
  views::FlexLayoutView* keyboard_shortcut_container_ =
      nullptr;                                     // Owned by views hierarchy.
  std::vector<LabelAndTag> big_title_label_tags_;  // Owned by views hierarchy.
  std::vector<LabelAndTag> title_label_tags_;    // Owned by views hierarchy.
  std::vector<LabelAndTag> details_label_tags_;  // Owned by views hierarchy.
  std::vector<LabelAndTag>
      keyboard_shortcut_container_tags_;     // Owned by views hierarchy.
  views::Label* separator_label_ = nullptr;  // Owned by views hierarchy.
  views::Label* rating_ = nullptr;           // Owned by views hierarchy.
  views::ImageView* rating_star_ = nullptr;  // Owned by views hierarchy.

  // Whether a result change was detected. This will be set only if the ID of
  // the result shown by the view changes. Result will be considered unchanged
  // if its metadata (e.g. icon, or text style tags) changes.
  bool result_changed_ = false;

  // Whether the removal confirmation dialog is invoked by long press touch.
  bool confirm_remove_by_long_press_ = false;

  // Separator label is shown for `kDefault` when details text is not empty,
  bool should_show_separator_label_ = false;

  // Used to override `title_and_details_container_` layout when
  // `keyboard_shortcut_container_` is populated.
  bool has_keyboard_shortcut_contents_ = false;

  SearchResultViewType view_type_;

  base::WeakPtrFactory<SearchResultView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_APP_LIST_VIEWS_SEARCH_RESULT_VIEW_H_
