// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TRAY_STATUS_AREA_OVERFLOW_BUTTON_TRAY_H_
#define ASH_SYSTEM_TRAY_STATUS_AREA_OVERFLOW_BUTTON_TRAY_H_

#include "ash/ash_export.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/tray/tray_background_view.h"
#include "ash/system/tray/tray_bubble_view.h"
#include "ash/system/tray/tray_bubble_wrapper.h"
#include "base/macros.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/gfx/transform.h"
#include "ui/views/controls/image_view.h"

namespace gfx {
class SlideAnimation;
}  // namespace gfx

namespace ash {

// The collapse/expand tray button in tablet mode, which is shown when the
// status area contains more buttons than the maximum width. Tapping on this
// button will show/hide the overflown tray buttons.
class ASH_EXPORT StatusAreaOverflowButtonTray : public TrayBackgroundView {
 public:
  explicit StatusAreaOverflowButtonTray(Shelf* shelf);
  ~StatusAreaOverflowButtonTray() override;

  enum State { CLICK_TO_EXPAND = 0, CLICK_TO_COLLAPSE };

  // TrayBackgroundView:
  void ClickedOutsideBubble() override;
  base::string16 GetAccessibleNameForTray() override;
  void HideBubbleWithView(const TrayBubbleView* bubble_view) override;
  void Initialize() override;
  bool PerformAction(const ui::Event& event) override;
  const char* GetClassName() const override;

  State state() const { return state_; }

 private:
  // The button icon of an animating arrow based on the collapse/expand state.
  class IconView : public views::ImageView, public gfx::AnimationDelegate {
   public:
    IconView();
    ~IconView() override;

    void ToggleState(State state);

   private:
    // gfx::AnimationDelegate:
    void AnimationEnded(const gfx::Animation* animation) override;
    void AnimationProgressed(const gfx::Animation* animation) override;
    void AnimationCanceled(const gfx::Animation* animation) override;

    void UpdateRotation();

    const std::unique_ptr<gfx::SlideAnimation> slide_animation_;
  };

  State state_ = CLICK_TO_EXPAND;

  IconView* const icon_;

  DISALLOW_COPY_AND_ASSIGN(StatusAreaOverflowButtonTray);
};

}  // namespace ash

#endif  // ASH_SYSTEM_TRAY_STATUS_AREA_OVERFLOW_BUTTON_TRAY_H_
