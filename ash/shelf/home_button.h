// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_HOME_BUTTON_H_
#define ASH_SHELF_HOME_BUTTON_H_

#include <memory>

#include "ash/app_list/app_list_metrics.h"
#include "ash/ash_export.h"
#include "ash/shelf/home_button_controller.h"
#include "ash/shelf/shelf_button_delegate.h"
#include "ash/shelf/shelf_control_button.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/view_targeter_delegate.h"

namespace views {
class CircleLayerDelegate;
}  // namespace views

namespace ui {
class Layer;
}

namespace ash {

class ShelfButtonDelegate;
class ShelfNavigationWidget;

// Button used for the AppList icon on the shelf. It opens the app list (in
// clamshell mode) or home screen (in tablet mode). Because the clamshell-mode
// app list appears like a dismissable overlay, the button is highlighted while
// the app list is open in clamshell mode.
//
// If Assistant is enabled, the button is filled in; long-pressing it will
// launch Assistant.
class ASH_EXPORT HomeButton : public ShelfControlButton,
                              public ShelfButtonDelegate,
                              public views::ViewTargeterDelegate {
 public:
  class ScopedNoClipRect {
   public:
    explicit ScopedNoClipRect(ShelfNavigationWidget* shelf_navigation_widget);
    ScopedNoClipRect(const ScopedNoClipRect&) = delete;
    ScopedNoClipRect& operator=(const ScopedNoClipRect&) = delete;
    ~ScopedNoClipRect();

   private:
    ShelfNavigationWidget* const shelf_navigation_widget_;
    const gfx::Rect clip_rect_;
  };

  // An observer that can be used to track the nudge animation state. Currently
  // used in testing.
  class NudgeAnimationObserver : public base::CheckedObserver {
   public:
    NudgeAnimationObserver() = default;
    NudgeAnimationObserver(const NudgeAnimationObserver&) = delete;
    NudgeAnimationObserver& operator=(const NudgeAnimationObserver&) = delete;
    ~NudgeAnimationObserver() override = default;

    // Called when the nudge animation is started/ended.
    virtual void NudgeAnimationStarted(HomeButton* home_button) = 0;
    virtual void NudgeAnimationEnded(HomeButton* home_button) = 0;
  };

  static const char kViewClassName[];

  explicit HomeButton(Shelf* shelf);

  HomeButton(const HomeButton&) = delete;
  HomeButton& operator=(const HomeButton&) = delete;

  ~HomeButton() override;

  // views::Button:
  void OnGestureEvent(ui::GestureEvent* event) override;
  const char* GetClassName() const override;
  std::u16string GetTooltipText(const gfx::Point& p) const override;

  // ShelfButtonDelegate:
  void OnShelfButtonAboutToRequestFocusFromTabTraversal(ShelfButton* button,
                                                        bool reverse) override;
  void ButtonPressed(views::Button* sender,
                     const ui::Event& event,
                     views::InkDrop* ink_drop) override;

  // Called when the availability of a long-press gesture may have changed, e.g.
  // when Assistant becomes enabled.
  void OnAssistantAvailabilityChanged();

  // True if the app list is shown for the display containing this button.
  bool IsShowingAppList() const;

  // Called when a locale change is detected. Updates the button tooltip and
  // accessible name.
  void HandleLocaleChange();

  // Returns the display which contains this view.
  int64_t GetDisplayId() const;

  // Clip rect of this view's widget will be removed during the life time of the
  // returned ScopedNoClipRect.
  [[nodiscard]] std::unique_ptr<ScopedNoClipRect> CreateScopedNoClipRect();

  // Starts the launcher nudge animation.
  void StartNudgeAnimation();

  void AddNudgeAnimationObserverForTest(NudgeAnimationObserver* observer);
  void RemoveNudgeAnimationObserverForTest(NudgeAnimationObserver* observer);

 protected:
  // views::Button:
  void PaintButtonContents(gfx::Canvas* canvas) override;
  void OnThemeChanged() override;

 private:
  // views::ViewTargeterDelegate:
  bool DoesIntersectRect(const views::View* target,
                         const gfx::Rect& rect) const override;

  // Callback for the nudge animation.
  void OnNudgeAnimationStarted();
  void OnNudgeAnimationEnded();

  // The controller used to determine the button's behavior.
  HomeButtonController controller_;

  // The ripple layer in the launcher nudge animation. Only exists during the
  // nudge animation.
  std::unique_ptr<ui::Layer> nudge_ripple_layer_;

  // The delegate used by |nudge_ripple_layer_|. Only exists during the
  // nudge animation.
  std::unique_ptr<views::CircleLayerDelegate> ripple_layer_delegate_;

  std::unique_ptr<ScopedNoClipRect> scoped_no_clip_rect_;

  base::ObserverList<NudgeAnimationObserver> observers_;

  base::WeakPtrFactory<HomeButton> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_SHELF_HOME_BUTTON_H_
