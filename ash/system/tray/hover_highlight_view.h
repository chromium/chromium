// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TRAY_HOVER_HIGHLIGHT_VIEW_H_
#define ASH_SYSTEM_TRAY_HOVER_HIGHLIGHT_VIEW_H_

#include <memory>

#include "ash/system/tray/actionable_view.h"
#include "ash/system/tray/tray_popup_item_style.h"
#include "base/bind.h"
#include "base/macros.h"
#include "ui/gfx/font.h"
#include "ui/gfx/text_constants.h"

namespace views {
class Border;
class ImageView;
class Label;
}

namespace ash {
class TriView;
class ViewClickListener;

// A view that changes background color on hover, and triggers a callback in the
// associated ViewClickListener on click. The view can also be forced to
// maintain a fixed height.
class HoverHighlightView : public ActionableView {
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
  // The former uses default for |use_unified_theme|.
  explicit HoverHighlightView(ViewClickListener* listener);
  HoverHighlightView(ViewClickListener* listener, bool use_unified_theme);
  ~HoverHighlightView() override;

  // Convenience function for populating the view with an icon and a label. This
  // also sets the accessible name. Primarily used for scrollable rows in
  // detailed views.
  void AddIconAndLabel(const gfx::ImageSkia& image, const base::string16& text);

  // Populates the view with a text label, inset on the left by the horizontal
  // space that would normally be occupied by an icon.
  void AddLabelRow(const base::string16& text);

  // Adds an optional right icon to an already populated view. |icon_size| is
  // the size of the icon in DP.
  void AddRightIcon(const gfx::ImageSkia& image, int icon_size);

  // Adds an optional right view to an already populated view.
  void AddRightView(views::View* view,
                    std::unique_ptr<views::Border> border = nullptr);

  // Hides or shows the right view for an already populated view.
  void SetRightViewVisible(bool visible);

  // Sets the text of the sub label for an already populated view. |sub_text|
  // must not be empty and prior to calling this function, |text_label_| must
  // not be null.
  void SetSubText(const base::string16& sub_text);

  // Allows view to expand its height. Size of unexapandable view is fixed and
  // equals to kTrayPopupItemHeight.
  void SetExpandable(bool expandable);

  // Changes the view's current accessibility state. This will fire an
  // accessibility event if needed.
  void SetAccessibilityState(AccessibilityState accessibility_state);

  // Removes current children of the view so that it can be re-populated.
  void Reset();

  bool is_populated() const { return is_populated_; }

  views::Label* text_label() { return text_label_; }
  views::Label* sub_text_label() { return sub_text_label_; }
  views::ImageView* left_icon() { return left_icon_; }
  views::View* right_view() { return right_view_; }

 protected:
  // Override from Button to also set the tooltip for all child elements.
  void OnSetTooltipText(const base::string16& tooltip_text) override;

  // views::View:
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  const char* GetClassName() const override;

  TriView* tri_view() { return tri_view_; }

 private:
  // Adds the image and label to the row with the label being styled using
  // |font_style|.
  void DoAddIconAndLabel(const gfx::ImageSkia& image,
                         const base::string16& text,
                         TrayPopupItemStyle::FontStyle font_style);

  // ActionableView:
  bool PerformAction(const ui::Event& event) override;

  // views::View:
  gfx::Size CalculatePreferredSize() const override;
  int GetHeightForWidth(int width) const override;
  void OnFocus() override;

  void OnEnabledChanged();

  // Determines whether the view is populated or not. If it is, Reset() should
  // be called before re-populating the view.
  bool is_populated_ = false;

  ViewClickListener* const listener_ = nullptr;
  views::Label* text_label_ = nullptr;
  views::Label* sub_text_label_ = nullptr;
  views::ImageView* left_icon_ = nullptr;
  views::View* right_view_ = nullptr;
  TriView* tri_view_ = nullptr;
  bool expandable_ = false;
  const bool use_unified_theme_;
  AccessibilityState accessibility_state_ = AccessibilityState::DEFAULT;
  views::PropertyChangedSubscription enabled_changed_subscription_ =
      AddEnabledChangedCallback(
          base::BindRepeating(&HoverHighlightView::OnEnabledChanged,
                              base::Unretained(this)));

  DISALLOW_COPY_AND_ASSIGN(HoverHighlightView);
};

}  // namespace ash

#endif  // ASH_SYSTEM_TRAY_HOVER_HIGHLIGHT_VIEW_H_
