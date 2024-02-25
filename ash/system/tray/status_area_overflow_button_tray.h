// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TRAY_STATUS_AREA_OVERFLOW_BUTTON_TRAY_H_
#define ASH_SYSTEM_TRAY_STATUS_AREA_OVERFLOW_BUTTON_TRAY_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/system/tray/tray_background_view.h"
#include "ash/system/tray/tray_bubble_view.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/views/controls/image_view.h"

namespace gfx {
class SlideAnimation;
}  // namespace gfx

namespace ui {
class Event;
}  // namespace ui

namespace ash {
class Shelf;

// The collapse/expand tray button in tablet mode, which is shown when the
// status area contains more buttons than the maximum width. Tapping on this
// button will show/hide the overflown tray buttons.
class ASH_EXPORT StatusAreaOverflowButtonTray : public TrayBackgroundView {
  METADATA_HEADER(StatusAreaOverflowButtonTray, TrayBackgroundView)

 public:
  explicit StatusAreaOverflowButtonTray(Shelf* shelf);
  StatusAreaOverflowButtonTray(const StatusAreaOverflowButtonTray&) = delete;
  StatusAreaOverflowButtonTray& operator=(const StatusAreaOverflowButtonTray&) =
      delete;
  ~StatusAreaOverflowButtonTray() override;

  enum State { CLICK_TO_EXPAND = 0, CLICK_TO_COLLAPSE };

  // TrayBackgroundView:
  void ClickedOutsideBubble(const ui::LocatedEvent& event) override;
  // No need to override since this view doesn't have an active/inactive state.
  void UpdateTrayItemColor(bool is_active) override {}
  std::u16string GetAccessibleNameForTray() override;
  void HandleLocaleChange() override;
  void HideBubbleWithView(const TrayBubbleView* bubble_view) override;
  void HideBubble(const TrayBubbleView* bubble_view) override;
  void Initialize() override;
  void SetVisiblePreferred(bool visible_preferred) override;
  void UpdateAfterStatusAreaCollapseChange() override;

  // Callback called when this button is pressed.
  void OnButtonPressed(const ui::Event& event);

  // Resets the state back to be collapsed (i.e. CLICK_TO_EXPAND).
  void ResetStateToCollapsed();

  State state() const { return state_; }

 private:
  // The button icon of an animating arrow based on the collapse/expand state.
  class IconView : public views::ImageView, public gfx::AnimationDelegate {
    METADATA_HEADER(IconView, views::ImageView)

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

  // Owned by the views hierarchy.
  const raw_ptr<IconView> icon_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_TRAY_STATUS_AREA_OVERFLOW_BUTTON_TRAY_H_
