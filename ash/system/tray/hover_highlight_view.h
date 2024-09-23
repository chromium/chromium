// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TRAY_HOVER_HIGHLIGHT_VIEW_H_
#define ASH_SYSTEM_TRAY_HOVER_HIGHLIGHT_VIEW_H_

#include <memory>

#include "ash/ash_export.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/gfx/font.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/controls/button/button.h"

namespace views {
class Border;
class ImageView;
class Label;
}  // namespace views

namespace ash {
class TriView;
class ViewClickListener;

// A view that changes background color on hover, and triggers a callback in the
// associated ViewClickListener on click. The view can also be forced to
// maintain a fixed height.
class ASH_EXPORT HoverHighlightView : public views::Button {
  METADATA_HEADER(HoverHighlightView, views::Button)

 public:
  enum class AccessibilityState {
    // The default accessibility view.
    DEFAULT,
    // This view is a checked checkbox.
    CHECKED_CHECKBOX,
    // This view is an unchecked checkbox.
    UNCHECKED_CHECKBOX
  };

  // If |listener| is null then no action is taken on click.
  explicit HoverHighlightView(ViewClickListener* listener);

  HoverHighlightView(const HoverHighlightView&) = delete;
  HoverHighlightView& operator=(const HoverHighlightView&) = delete;

  ~HoverHighlightView() override;

  // Convenience function for populating the view with an icon and a label. This
  // also sets the accessible name. Primarily used for scrollable rows in
  // detailed views.
  // New callers should use the function below which takes an ImageModel.
  // TODO(b/259490845): Change callers to pass an ImageModel and eliminate this.
  void AddIconAndLabel(const gfx::ImageSkia& image, const std::u16string& text);

  // The same as the above function with `ImageModel` parameter instead.
  void AddIconAndLabel(const ui::ImageModel& image, const std::u16string& text);

  // Convenience function for populating the view with an arbitrary view and a
  // label. This also sets the accessible name.
  void AddViewAndLabel(std::unique_ptr<views::View> view,
                       const std::u16string& text);

  // Populates the view with a text label, inset on the left by the horizontal
  // space that would normally be occupied by an icon.
  void AddLabelRow(const std::u16string& text);

  // Populates the view with a text label with custom start inset.
  void AddLabelRow(const std::u16string& text, int start_inset);

  // Adds an optional right icon to an already populated view. |icon_size| is
  // the size of the icon in DP.
  void AddRightIcon(const ui::ImageModel& image, int icon_size);

  // Adds an optional right view to an already populated view.
  void AddRightView(views::View* view,
                    std::unique_ptr<views::Border> border = nullptr);

  // Adds an additional right view next to the `right_view_`.
  // TODO (b/266761290): Remove this method when it's not needed.
  void AddAdditionalRightView(views::View* view);

  // Hides or shows the right view for an already populated view.
  void SetRightViewVisible(bool visible);

  // Sets the text of the sub label for an already populated view. |sub_text|
  // must not be empty and prior to calling this function, |text_label_| must
  // not be null.
  void SetSubText(const std::u16string& sub_text);

  // Allows view to expand its height. Size of unexapandable view is fixed and
  // equals to kTrayPopupItemHeight.
  void SetExpandable(bool expandable);

  // Changes the view's current accessibility state. This will fire an
  // accessibility event if needed.
  void SetAccessibilityState(AccessibilityState accessibility_state);

  // Removes current children of the view so that it can be re-populated.
  void Reset();

  bool is_populated() const { return is_populated_; }

  views::ImageView* icon() { return icon_; }
  views::Label* text_label() { return text_label_; }
  views::Label* sub_text_label() { return sub_text_label_; }
  views::View* left_view() { return left_view_; }
  views::View* right_view() { return right_view_; }
  views::View* sub_row() { return sub_row_; }
  TriView* tri_view() { return tri_view_; }

 protected:
  // Override from Button to also set the tooltip for all child elements.
  void OnSetTooltipText(const std::u16string& tooltip_text) override;

 private:
  friend class TrayAccessibilityTest;

  void PerformAction();

  // views::View:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;
  void OnFocus() override;

  // Adds a view that acts as a container for all views that are added into the
  // sub-row, i.e. the row below the label.
  void AddSubRowContainer();

  // views::Button:
  void OnEnabledChanged() override;

  void SetAndUpdateAccessibleDefaultAction();

  // Determines whether the view is populated or not. If it is, Reset() should
  // be called before re-populating the view.
  bool is_populated_ = false;

  const raw_ptr<ViewClickListener, DanglingUntriaged> listener_ = nullptr;
  raw_ptr<views::ImageView, DanglingUntriaged> icon_ = nullptr;
  raw_ptr<views::Label, DanglingUntriaged> text_label_ = nullptr;
  raw_ptr<views::Label, DanglingUntriaged> sub_text_label_ = nullptr;
  raw_ptr<views::View, DanglingUntriaged> left_view_ = nullptr;
  raw_ptr<views::View, DanglingUntriaged> right_view_ = nullptr;
  raw_ptr<views::View, DanglingUntriaged> sub_row_ = nullptr;
  raw_ptr<TriView, DanglingUntriaged> tri_view_ = nullptr;
  bool expandable_ = false;
  AccessibilityState accessibility_state_ = AccessibilityState::DEFAULT;
};

}  // namespace ash

#endif  // ASH_SYSTEM_TRAY_HOVER_HIGHLIGHT_VIEW_H_
