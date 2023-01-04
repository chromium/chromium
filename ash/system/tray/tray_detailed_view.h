// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TRAY_TRAY_DETAILED_VIEW_H_
#define ASH_SYSTEM_TRAY_TRAY_DETAILED_VIEW_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/view_click_listener.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/view.h"

namespace gfx {
struct VectorIcon;
}  // namespace gfx

namespace views {
class BoxLayout;
class Button;
class ImageView;
class Label;
class ProgressBar;
class ScrollView;
class Separator;
}  // namespace views

namespace ash {

class DetailedViewDelegate;
class HoverHighlightView;
class TriView;

class ASH_EXPORT TrayDetailedView : public views::View,
                                    public ViewClickListener {
 public:
  explicit TrayDetailedView(DetailedViewDelegate* delegate);

  TrayDetailedView(const TrayDetailedView&) = delete;
  TrayDetailedView& operator=(const TrayDetailedView&) = delete;

  ~TrayDetailedView() override;

  // ViewClickListener:
  // Don't override this --- override HandleViewClicked.
  void OnViewClicked(views::View* sender) final;

  // Setter for `progress_bar_` accessibility label.
  void OverrideProgressBarAccessibleName(const std::u16string& name);

 protected:
  // views::View:
  void Layout() override;
  int GetHeightForWidth(int width) const override;
  const char* GetClassName() const override;
  void OnThemeChanged() override;

  // Exposes the layout manager of this view to give control to subclasses.
  views::BoxLayout* box_layout() { return box_layout_; }

  // Creates the row containing the back button and title. Optionally omits the
  // back button if `create_back_button` is false.
  void CreateTitleRow(int string_id, bool create_back_button = true);

  // Creates a scrollable list. The list has a border at the bottom if there is
  // any other view between the list and the footer row at the bottom.
  void CreateScrollableList();

  // Adds a targetable row to `container` containing `icon` and `text`.
  // Pre-QsRevamp the `container` should be scroll_content().
  // Post-QsRevamp the `container` may be a RoundedContainer.
  HoverHighlightView* AddScrollListItem(views::View* container,
                                        const gfx::VectorIcon& icon,
                                        const std::u16string& text);

  // Adds a targetable row to `container` containing `icon`, `text`, and a
  // checkbox. `checked` determines whether the checkbox is checked or not.
  // `enterprise_managed` determines whether or not there will be an enterprise
  // managed icon for that item.
  // Pre-QsRevamp the `container` should be scroll_content().
  // Post-QsRevamp the `container` may be a RoundedContainer.
  HoverHighlightView* AddScrollListCheckableItem(
      views::View* container,
      const gfx::VectorIcon& icon,
      const std::u16string& text,
      bool checked,
      bool enterprise_managed = false);

  // Adds a sticky sub header to `container` containing `icon` and a text
  // represented by `text_id` resource id.
  TriView* AddScrollListSubHeader(views::View* container,
                                  const gfx::VectorIcon& icon,
                                  int text_id);

  // Removes (and destroys) all child views.
  void Reset();

  // Shows or hides the progress bar below the title row. It occupies the same
  // space as the separator, so when shown the separator is hidden. If
  // |progress_bar_| doesn't already exist it will be created.
  void ShowProgress(double value, bool visible);

  // Helper functions which create and return the settings and help buttons,
  // respectively, used in the material design top-most header row. The caller
  // assumes ownership of the returned buttons.
  virtual views::Button* CreateInfoButton(
      views::Button::PressedCallback callback,
      int info_accessible_name_id);

  views::Button* CreateSettingsButton(views::Button::PressedCallback callback,
                                      int setting_accessible_name_id);
  views::Button* CreateHelpButton(views::Button::PressedCallback callback);

  // Closes the bubble that contains the detailed view.
  void CloseBubble();

  TriView* tri_view() { return tri_view_; }
  views::ScrollView* scroller() const { return scroller_; }
  views::View* scroll_content() const { return scroll_content_; }

  // Gets called in the constructor of the `CalendarView`, or any other views in
  // the future that don't have a separator to modify the value of
  // `has_separator` to false.
  void IgnoreSeparator();

 private:
  friend class TrayDetailedViewTest;

  // Overridden to handle clicks on subclass-specific views.
  virtual void HandleViewClicked(views::View* view);

  // Returns the TriView used for the title row. A label with `string_id` is
  // added to the CENTER view.
  std::unique_ptr<TriView> CreateTitleTriView(int string_id);

  // Returns the separator used between the title row and the contents.
  std::unique_ptr<views::Separator> CreateTitleSeparator();

  // Creates and adds subclass-specific buttons to the title row.
  virtual void CreateExtraTitleRowButtons();

  // Transition to main view from detailed view.
  void TransitionToMainView();

  DetailedViewDelegate* const delegate_;
  views::BoxLayout* box_layout_ = nullptr;
  views::ScrollView* scroller_ = nullptr;
  views::View* scroll_content_ = nullptr;
  views::ProgressBar* progress_bar_ = nullptr;

  // The container view for the top-most title row. Owned by views hierarchy.
  TriView* tri_view_ = nullptr;

  // The back button that appears in the title row. Owned by views hierarchy.
  views::Button* back_button_ = nullptr;

  // The label in the title row. Owned by views hierarchy.
  views::Label* title_label_ = nullptr;

  // Owned by views hierarchy.
  views::Label* sub_header_label_ = nullptr;
  views::ImageView* sub_header_image_view_ = nullptr;

  // Owned by vector icon cache.
  const gfx::VectorIcon* sub_header_icon_ = nullptr;

  // The separator under the title row. Not all views have a separator. Owned by
  // views hierarchy.
  views::Separator* title_separator_ = nullptr;

  // Gets modified to false in the constructor of the view if it doesn't have a
  // separator.
  bool has_separator_ = true;

  // The accessible name for the `progress_bar_`.
  absl::optional<std::u16string> progress_bar_accessible_name_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_TRAY_TRAY_DETAILED_VIEW_H_
