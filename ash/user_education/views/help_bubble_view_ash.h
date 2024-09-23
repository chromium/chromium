// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_USER_EDUCATION_VIEWS_HELP_BUBBLE_VIEW_ASH_H_
#define ASH_USER_EDUCATION_VIEWS_HELP_BUBBLE_VIEW_ASH_H_

#include <cstddef>
#include <memory>
#include <optional>
#include <vector>

#include "ash/ash_export.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/user_education/common/help_bubble_params.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/button/label_button.h"

namespace views {
class ImageView;
class Label;
}  // namespace views

namespace ash {

enum class HelpBubbleId;

namespace internal {

// Describes how a help bubble should be anchored to a Views element, beyond
// what is specified by the HelpBubbleParams. Should only be instantiated by
// classes derived from HelpBubbleFactory (or in tests).
struct HelpBubbleAnchorParams {
  // This is the View to be anchored to (mandatory).
  raw_ptr<views::View> view = nullptr;
};

}  // namespace internal

// The HelpBubbleView is a special BubbleDialogDelegateView for
// in-product help which educates users about certain Chrome features in
// a deferred context.
class ASH_EXPORT HelpBubbleViewAsh : public views::BubbleDialogDelegateView {
  METADATA_HEADER(HelpBubbleViewAsh, views::BubbleDialogDelegateView)

 public:
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kHelpBubbleElementIdForTesting);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kDefaultButtonIdForTesting);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kFirstNonDefaultButtonIdForTesting);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kBodyIconIdForTesting);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kBodyTextIdForTesting);

  HelpBubbleViewAsh(HelpBubbleId id,
                    const internal::HelpBubbleAnchorParams& anchor,
                    user_education::HelpBubbleParams params);
  HelpBubbleViewAsh(const HelpBubbleViewAsh&) = delete;
  HelpBubbleViewAsh& operator=(const HelpBubbleViewAsh&) = delete;
  ~HelpBubbleViewAsh() override;

  // Returns whether the given dialog is a help bubble.
  static bool IsHelpBubble(views::DialogDelegate* dialog);

  bool IsFocusInHelpBubble() const;

  views::LabelButton* GetDefaultButtonForTesting() const;
  views::LabelButton* GetNonDefaultButtonForTesting(int index) const;

  // Gets the `gfx::Rect` representing the area of this view's widget/window for
  // which located events should be targeted to this view.
  gfx::Rect GetHitRect() const;

  HelpBubbleId id() const { return id_; }

 protected:
  // views::BubbleDialogDelegateView:
  std::unique_ptr<views::NonClientFrameView> CreateNonClientFrameView(
      views::Widget* widget) override;
  void OnAnchorBoundsChanged() override;
  std::u16string GetAccessibleWindowTitle() const override;
  void OnWidgetActivationChanged(views::Widget* widget, bool active) override;
  void OnWidgetBoundsChanged(views::Widget* widget, const gfx::Rect&) override;
  void OnThemeChanged() override;
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;
  gfx::Rect GetAnchorRect() const override;
  void GetWidgetHitTestMask(SkPath* mask) const override;
  bool WidgetHasHitTestMask() const override;

 private:
  FRIEND_TEST_ALL_PREFIXES(HelpBubbleViewTimeoutTest,
                           RespectsProvidedTimeoutAfterActivate);
  friend class HelpBubbleViewsTest;

  void MaybeStartAutoCloseTimer();

  void OnTimeout();

  // Updates the help bubble's rounded corners based on its position relative to
  // its anchor. When the help bubble is not center aligned with its anchor, the
  // corner closest to the anchor has a smaller radius.
  void UpdateRoundedCorners();

  const HelpBubbleId id_;

  raw_ptr<views::ImageView> icon_view_ = nullptr;
  std::vector<raw_ptr<views::Label, VectorExperimental>> labels_;

  // If the bubble has buttons, it must be focusable.
  std::vector<raw_ptr<views::LabelButton, VectorExperimental>>
      non_default_buttons_;
  raw_ptr<views::LabelButton> default_button_ = nullptr;
  raw_ptr<views::Button> close_button_ = nullptr;

  // This is the base accessible name of the window.
  std::u16string accessible_name_;

  // This is any additional hint text to read.
  std::u16string screenreader_hint_text_;

  // Track the number of times the widget has been activated; if it's greater
  // than 1 we won't re-read the screenreader hint again.
  int activate_count_ = 0;

  // Prevents the widget we're anchored to from disappearing when it loses
  // focus, even if it's marked as close_on_deactivate.
  std::unique_ptr<CloseOnDeactivatePin> anchor_pin_;

  // Auto close timeout. If the value is 0 (default), the bubble never times
  // out.
  base::TimeDelta timeout_;
  base::OneShotTimer auto_close_timer_;

  base::OnceClosure timeout_callback_;
};

}  // namespace ash

#endif  // ASH_USER_EDUCATION_VIEWS_HELP_BUBBLE_VIEW_ASH_H_
