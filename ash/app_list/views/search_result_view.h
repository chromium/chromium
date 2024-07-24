// Copyright 2012 The Chromium Authors
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
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/progress_bar.h"

namespace views {
class FlexLayoutView;
class ImageView;
class Label;
}  // namespace views

namespace ash {

namespace test {
class SearchResultListViewTest;
class SearchResultViewWidgetTest;
}  // namespace test

class AppListViewDelegate;
class MaskedImageView;
class SearchResult;
class SearchResultListView;
class SearchResultPageDialogController;

// Search result view uses `views::FlexLayout` to show results in different
// configurations.
// +----------------------------------------------------------------------+
// |`text_container_`                                                     |
// | +----------------------+ +-----------------------------------------+ |
// | |`big_title_container_'| |`body_text_container_`                   | |
// | |                      | | +------------------------------+        | |
// | |                      | | |`title_and_details_container_`|        | |
// | |                      | | +------------------------------+        | |
// | |                      | | +------------------------------+        | |
// | |                      | | |`keyboard_shortcut_container_`|        | |
// | |                      | | +------------------------------+        | |
// | |                      | | +------------------------------+        | |
// | |                      | | |`progress_bar_container_`     |        | |
// | |                      | | +------------------------------+        | |
// | |                      | | +------------------------------+        | |
// | |                      | | |`system_details_container_`   |        | |
// | |                      | | +------------------------------+        | |
// | +----------------------+ +-----------------------------------------+ |
// +----------------------------------------------------------------------+
//
// +-------------------------------------------------------------------------+
// |`big_title_container_`                                                   |
// | +--------------------------------+ +----------------------------------+ |
// | |'big_title_main_text_container_'| |`big_title_superscript_container_`| |
// | +--------------------------------+ +----------------------------------+ |
// +-------------------------------------------------------------------------+
//
// The `title_and_details_container_` has three possible layouts depending on
// `view_type_` and whether `keyboard_shortcut_container_` has results
//
// Layout used when the view_type_ == SearchResultViewType::kDefault OR
// `has_keyboard_shortcut_contents_` is set.
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
//
// Layout used when `is_progress_bar_answer_card_` is true.
// +-------------------------------+
// |`title_and_details_container_` |
// | +---------------------------+ |
// | | `progress_bar_container_` | |
// | +-------------------------- + |
// | +--------------------+        |
// | |'details_container_'|        |
// | +--------------------+        |
// +-------------------------------+
//
// Layout used when the result is a System Info Answer Card and the result
// contains a right hand description.
// +---------------------------------------------------------------+
// |`title_and_details_container_`                                 |
// | +---------------------------+                                 |
// | | `progress_bar_container_` |                                 |
// | +-------------------------- +                                 |
// | +-----------------------------------------------------------+ |
// | | `system_details_container_`                               | |
// | | +-------------------------+  +--------------------------+ | |
// | | |`left_details_container_`|  |`right_details_container_`| | |
// | | +-------------------------+  +--------------------------+ | |
// | +-----------------------------------------------------------+ |
// +---------------------------------------------------------------+

class ASH_EXPORT SearchResultView : public SearchResultBaseView,
                                    public SearchResultActionsViewDelegate {
  METADATA_HEADER(SearchResultView, SearchResultBaseView)

 public:
  class LabelAndTag {
   public:
    LabelAndTag(views::Label* label, SearchResult::Tags tags);

    LabelAndTag(const LabelAndTag& other);
    LabelAndTag& operator=(const LabelAndTag& other);
    ~LabelAndTag();

    views::Label* GetLabel() const { return label_; }
    SearchResult::Tags GetTags() const { return tags_; }

   private:
    raw_ptr<views::Label, DanglingUntriaged>
        label_;  // Owned by views hierarchy.
    SearchResult::Tags tags_;
  };

  enum class SearchResultViewType {
    // The default view type used for the majority of search results.
    kDefault,
    // Inline Answer views are used to directly answer questions posed by the
    // search query.
    kAnswerCard,
  };

  enum class LabelType {
    kBigTitle,
    kBigTitleSuperscript,
    kTitle,
    kDetails,
    kKeyboardShortcut,
  };

  SearchResultView(SearchResultListView* list_view,
                   AppListViewDelegate* view_delegate,
                   SearchResultPageDialogController* dialog_controller,
                   SearchResultViewType view_type);

  SearchResultView(const SearchResultView&) = delete;
  SearchResultView& operator=(const SearchResultView&) = delete;

  ~SearchResultView() override;

  // Sets/gets SearchResult displayed by this view.
  void OnResultChanged() override;

  void SetSearchResultViewType(SearchResultViewType type);
  void ClearBigTitleContainer();
  SearchResultViewType view_type() { return view_type_; }

  views::LayoutOrientation TitleAndDetailsOrientationForTest();

  // Calculates the width of the `title_container_` and 'details_container_'
  // for SearchResultView's custom eliding behavior.
  // total_width is the total width allocated to `title_and_details_container_`
  static int GetTargetTitleWidth(int total_width,
                                 int separator_width,
                                 int target_details_width);
  static int GetMinimumDetailsWidth(int total_width,
                                    int details_width,
                                    int details_no_elide_width);

  // Set flex layout weights for title and details containers to support custom
  // eliding behavior.
  static void SetFlexBehaviorForTextContents(
      int total_width,
      int separator_width,
      int non_elided_details_width,
      views::FlexLayoutView* title_container,
      views::FlexLayoutView* details_container);

  void set_multi_line_details_height_for_test(int height) {
    multi_line_details_height_ = height;
  }

  void set_multi_line_title_height_for_test(int height) {
    multi_line_title_height_ = height;
  }

  views::FlexLayoutView* get_keyboard_shortcut_container_for_test() {
    return keyboard_shortcut_container_;
  }

  views::FlexLayoutView* get_title_container_for_test() {
    return title_container_;
  }

  views::FlexLayoutView* get_details_container_for_test() {
    return details_container_;
  }

  views::Label* get_result_text_separator_label_for_test() {
    return result_text_separator_label_;
  }

  views::FlexLayoutView* get_progress_bar_container_for_test() {
    return progress_bar_container_;
  }

 private:
  friend class test::SearchResultListViewTest;
  friend class SearchResultListView;
  friend class SearchResultViewWidgetTest;

  int PreferredHeight() const;
  int PrimaryTextHeight() const;
  int SecondaryTextHeight() const;
  int ActionButtonRightMargin() const;

  std::vector<LabelAndTag> SetupContainerViewForTextVector(
      views::FlexLayoutView* parent,
      const std::vector<SearchResult::TextItem>& text_vector,
      LabelType label_type,
      bool has_keyboard_shortcut_contents,
      bool is_multi_line);
  void UpdateBadgeIcon();
  void UpdateBigTitleContainer();
  void UpdateBigTitleSuperscriptContainer();
  void UpdateIconAndBadgeIcon();
  void UpdateTitleContainer();
  void UpdateDetailsContainer();
  void UpdateKeyboardShortcutContainer();
  void UpdateProgressBarContainer();
  void UpdateRating();

  void StyleLabel(views::Label* label, const SearchResult::Tags& tags);
  void StyleBigTitleContainer();
  void StyleBigTitleSuperscriptContainer();
  void StyleTitleContainer();
  void StyleDetailsContainer();
  void StyleKeyboardShortcutContainer();

  // Callback for query suggstion removal confirmation.
  void OnQueryRemovalAccepted(bool accepted);

  // Called when the result selection controller selects a new result.
  void OnSelectedResultChanged();

  // views::View overrides:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;
  void Layout(PassKey) override;
  bool OnKeyPressed(const ui::KeyEvent& event) override;
  void PaintButtonContents(gfx::Canvas* canvas) override;
  void OnMouseEntered(const ui::MouseEvent& event) override;
  void OnMouseExited(const ui::MouseEvent& event) override;
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

  bool IsInlineSearchResult();

  // Uses icon view bounds to calculate the host badge view bounds.
  gfx::Rect GetIconBadgeViewBounds(const gfx::Rect& icon_view_bounds) const;
  gfx::Size CalculateRegularIconImageSize(
      const gfx::ImageSkia& icon_image) const;

  // Parent list view. Owned by views hierarchy.
  const raw_ptr<SearchResultListView> list_view_;

  const raw_ptr<AppListViewDelegate> view_delegate_;

  const raw_ptr<SearchResultPageDialogController, DanglingUntriaged>
      dialog_controller_;

  raw_ptr<MaskedImageView> icon_view_ = nullptr;  // Owned by views hierarchy.
  raw_ptr<views::ImageView> badge_icon_view_ =
      nullptr;  // Owned by views hierarchy.

  raw_ptr<views::FlexLayoutView> text_container_ =
      nullptr;  // Owned by views hierarchy.
  raw_ptr<views::FlexLayoutView> big_title_container_ =
      nullptr;  // Owned by views hierarchy.
  raw_ptr<views::FlexLayoutView> big_title_main_text_container_ =
      nullptr;  // Owned by views hierarchy.
  raw_ptr<views::FlexLayoutView> big_title_superscript_container_ =
      nullptr;  // Owned by views hierarchy.
  raw_ptr<views::FlexLayoutView> body_text_container_ =
      nullptr;  // Owned by views hierarchy.
  raw_ptr<views::FlexLayoutView> title_and_details_container_ =
      nullptr;  // Owned by views hierarchy.
  raw_ptr<views::FlexLayoutView> title_container_ =
      nullptr;  // Owned by views hierarchy.
  raw_ptr<views::FlexLayoutView> details_container_ =
      nullptr;  // Owned by views hierarchy.
  raw_ptr<views::FlexLayoutView> keyboard_shortcut_container_ =
      nullptr;  // Owned by views hierarchy.
  raw_ptr<views::FlexLayoutView> progress_bar_container_ =
      nullptr;  // Owned by views hierarchy.
  raw_ptr<views::FlexLayoutView> system_details_container_ =
      nullptr;  // Owned by views hierarchy.
  raw_ptr<views::FlexLayoutView> left_details_container_ =
      nullptr;  // Owned by views hierarchy.
  raw_ptr<views::FlexLayoutView> right_details_container_ =
      nullptr;  // Owned by views hierarchy.

  std::vector<LabelAndTag> big_title_label_tags_;  // Owned by views hierarchy.
  std::vector<LabelAndTag>
      big_title_superscript_label_tags_;         // Owned by views hierarchy.
  std::vector<LabelAndTag> title_label_tags_;    // Owned by views hierarchy.
  std::vector<LabelAndTag> details_label_tags_;  // Owned by views hierarchy.
  std::vector<LabelAndTag>
      keyboard_shortcut_container_tags_;  // Owned by views hierarchy.
  std::vector<LabelAndTag>
      right_details_label_tags_;  // Owned by views hierarchy.

  raw_ptr<views::Label> result_text_separator_label_ =
      nullptr;  // Owned by views hierarchy.
  raw_ptr<views::Label> rating_separator_label_ =
      nullptr;                              // Owned by views hierarchy.
  raw_ptr<views::Label> rating_ = nullptr;  // Owned by views hierarchy.
  raw_ptr<views::ImageView> rating_star_ =
      nullptr;  // Owned by views hierarchy.
  raw_ptr<views::ProgressBar, DanglingUntriaged> progress_bar_ = nullptr;

  // Whether the removal confirmation dialog is invoked by long press touch.
  bool confirm_remove_by_long_press_ = false;

  // Separator label is shown for `kDefault` when details text is not empty,
  bool should_show_result_text_separator_label_ = false;

  // Used to override `title_and_details_container_` layout when
  // `keyboard_shortcut_container_` is populated.
  bool has_keyboard_shortcut_contents_ = false;

  // Used to insert a `progress_bar_container_` within the
  // `title_and_details_container_` when the result has a set bar chart.
  bool is_progress_bar_answer_card_ = false;

  SearchResultViewType view_type_;

  // Search result view can have one non-elided label. Cache the its for flex
  // layout weight calculations.
  int non_elided_details_label_width_ = 0;

  // Search result view can have one multi-line title label. Cache its height
  // for calculating PreferredHeight() and PrimaryTextHeight().
  int multi_line_title_height_ = 0;

  // Search result view can have one multi-line details label. Cache its height
  // for calculating PreferredHeight() and SecondaryTextHeight().
  int multi_line_details_height_ = 0;

  base::WeakPtrFactory<SearchResultView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_APP_LIST_VIEWS_SEARCH_RESULT_VIEW_H_
