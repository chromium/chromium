// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CONTROLS_CONTEXTUAL_NUDGE_H_
#define ASH_CONTROLS_CONTEXTUAL_NUDGE_H_

#include "ash/ash_export.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/label.h"

namespace views {
class View;
}  // namespace views

namespace ash {

// The implementation of contextual nudge tooltip bubbles.
class ASH_EXPORT ContextualNudge : public views::BubbleDialogDelegateView {
 public:
  // Indicates whether the nudge should be shown below or above the anchor.
  enum class Position { kBottom, kTop };

  // |anchor| - The view to which the nudge bubble should be anchored. May be
  //     nullptr, in which case anchor bounds should be provided using
  //     UpdateAnchorRect().
  // |parent_window| - if set, the window that should parent the nudge native
  //     window. If not set, the shelf container in the anchor view's root
  //     window will be used.
  // |position| - The nudge position relative to the anchor rectangle.
  // |margins| - The margins added to the nudge bubble.
  // |text| - The nudge text.
  // |tap_callback| - If set, the callback called when the user taps the nudge.
  ContextualNudge(views::View* anchor,
                  aura::Window* parent_window,
                  Position position,
                  const gfx::Insets& margins,
                  const std::u16string& text,
                  const base::RepeatingClosure& tap_callback);
  ~ContextualNudge() override;

  ContextualNudge(const ContextualNudge&) = delete;
  ContextualNudge& operator=(const ContextualNudge&) = delete;

  views::Label* label() { return label_; }

  // Sets the nudge bubble anchor rect - should be used to set the anchor rect
  // if no valid anchor was passed to the nudge bubble.
  void UpdateAnchorRect(const gfx::Rect& rect);

  // BubbleDialogDelegateView:
  ui::LayerType GetLayerType() const override;
  void OnGestureEvent(ui::GestureEvent* event) override;

 private:
  base::RepeatingClosure tap_callback_;

  raw_ptr<views::Label> label_;
};

}  // namespace ash

#endif  // ASH_CONTROLS_CONTEXTUAL_NUDGE_H_
