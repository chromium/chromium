// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TRAY_TRAY_DETAILED_VIEW_H_
#define ASH_SYSTEM_TRAY_TRAY_DETAILED_VIEW_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/system/tray/view_click_listener.h"
#include "ash/system/tray/zero_state_view.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/view.h"

namespace gfx {
struct VectorIcon;
}  // namespace gfx

namespace views {
class BoxLayout;
class Button;
class ProgressBar;
class ScrollView;
}  // namespace views

namespace ash {

class DetailedViewDelegate;
class HoverHighlightView;
class TriView;

class ASH_EXPORT TrayDetailedView : public views::View,
                                    public ViewClickListener {
  METADATA_HEADER(TrayDetailedView, views::View)

 public:
  explicit TrayDetailedView(DetailedViewDelegate* delegate);

  TrayDetailedView(const TrayDetailedView&) = delete;
  TrayDetailedView& operator=(const TrayDetailedView&) = delete;

  ~TrayDetailedView() override;

  // views::View:
  void Layout(PassKey) override;
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;

  // ViewClickListener:
  // Don't override this --- override HandleViewClicked.
  void OnViewClicked(views::View* sender) final;

  // Setter for `progress_bar_` accessibility label.
  void OverrideProgressBarAccessibleName(const std::u16string& name);

  views::ScrollView* scroll_view_for_testing() { return scroller_; }
  ZeroStateView* zero_state_view_for_testing() { return zero_state_view_; }

  // Adds a targetable row to `container` containing `icon`, `text`, and a
  // checkbox. `checked` determines whether the checkbox is checked or not.
  // `enterprise_managed` determines whether or not there will be an enterprise
  // managed icon for that item.
  // The `container` should be a RoundedContainer.
  HoverHighlightView* AddScrollListCheckableItem(
      views::View* container,
      const gfx::VectorIcon& icon,
      const std::u16string& text,
      bool checked,
      bool enterprise_managed = false);

 protected:
  // Exposes the layout manager of this view to give control to subclasses.
  views::BoxLayout* box_layout() { return box_layout_; }

  // Creates the row containing the back button and title.
  void CreateTitleRow(int string_id);

  // Creates a scrollable list. The list has a border at the bottom if there is
  // any other view between the list and the footer row at the bottom.
  void CreateScrollableList();

  // Creates a zero state view.
  void CreateZeroStateView(std::unique_ptr<ZeroStateView> view);

  // Adds a targetable row to `container` containing `icon` and `text`.
  // The `container` should be a RoundedContainer.
  HoverHighlightView* AddScrollListItem(views::View* container,
                                        const gfx::VectorIcon& icon,
                                        const std::u16string& text);

  // Removes (and destroys) all child views.
  void Reset();

  // Shows or hides the progress bar below the title row. It occupies the same
  // space as the created placeholder. If `progress_bar_` doesn't already exist
  // it will be created.
  void ShowProgress(double value, bool visible);

  // Sets the visibility of the zero state view.
  void SetZeroStateViewVisibility(bool visible);

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
  views::View* zero_state_view() const { return zero_state_view_; }

 private:
  friend class TrayDetailedViewTest;

  // Overridden to handle clicks on subclass-specific views.
  virtual void HandleViewClicked(views::View* view);

  // Returns the TriView used for the title row. A label with `string_id` is
  // added to the CENTER view. Left aligns the label contained in the CENTER
  // view.
  std::unique_ptr<TriView> CreateTitleTriView(int string_id);

  // Creates and adds subclass-specific buttons to the title row.
  virtual void CreateExtraTitleRowButtons();

  // Transition to main view from detailed view.
  void TransitionToMainView();

  const raw_ptr<DetailedViewDelegate, DanglingUntriaged> delegate_;
  raw_ptr<views::BoxLayout, DanglingUntriaged> box_layout_ = nullptr;
  raw_ptr<views::ScrollView, DanglingUntriaged> scroller_ = nullptr;
  raw_ptr<views::View, DanglingUntriaged> scroll_content_ = nullptr;
  raw_ptr<views::ProgressBar> progress_bar_ = nullptr;
  raw_ptr<ZeroStateView> zero_state_view_ = nullptr;

  // The container view for the top-most title row. Owned by views hierarchy.
  raw_ptr<TriView, DanglingUntriaged> tri_view_ = nullptr;

  // The back button that appears in the title row. Owned by views hierarchy.
  raw_ptr<views::Button, DanglingUntriaged> back_button_ = nullptr;

  // The accessible name for the `progress_bar_`.
  std::optional<std::u16string> progress_bar_accessible_name_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_TRAY_TRAY_DETAILED_VIEW_H_
