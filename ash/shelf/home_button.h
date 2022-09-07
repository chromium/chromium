// Copyright 2014 The Chromium Authors
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
class AnimationBuilder;
class CircleLayerDelegate;
class Label;
}  // namespace views

namespace ui {
class LayerOwner;
}

namespace ash {

class Shelf;
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

    // Called when the nudge label is animated to fully shown.
    virtual void NudgeLabelShown(HomeButton* home_button) = 0;
  };

  static const char kViewClassName[];

  explicit HomeButton(Shelf* shelf);

  HomeButton(const HomeButton&) = delete;
  HomeButton& operator=(const HomeButton&) = delete;

  ~HomeButton() override;

  // views::View:
  gfx::Size CalculatePreferredSize() const override;
  void Layout() override;

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

  // Checks if the `nudge_label_` can be shown for the launcher nudge.
  // NOTE: This must be called after `CreateNudgeLabel()`, where the
  // `nudge_label_` is created. This is because whether the nudge can be shown
  // depends on nudge_label_'s preferred size.
  bool CanShowNudgeLabel() const;

  // Starts the launcher nudge animation.
  void StartNudgeAnimation();

  void AddNudgeAnimationObserverForTest(NudgeAnimationObserver* observer);
  void RemoveNudgeAnimationObserverForTest(NudgeAnimationObserver* observer);

  views::View* label_container_for_test() const { return label_container_; }

 protected:
  // views::Button:
  void PaintButtonContents(gfx::Canvas* canvas) override;
  void OnThemeChanged() override;

 private:
  // Creates `nudge_label_` for launcher nudge.
  void CreateNudgeLabel();

  // Animation functions for launcher nudge.
  void AnimateNudgeRipple(views::AnimationBuilder& builder);
  void AnimateNudgeBounce(views::AnimationBuilder& builder);
  void AnimateNudgeLabelSlideIn(views::AnimationBuilder& builder);
  void AnimateNudgeLabelSlideOut();
  void AnimateNudgeLabelFadeOut();

  // Callbacks for the nudge animation.
  void OnNudgeAnimationStarted();
  void OnNudgeAnimationEnded();
  void OnLabelSlideInAnimationEnded();
  void OnLabelFadeOutAnimationEnded();

  // Removes the nudge label from the view hierarchy.
  void RemoveNudgeLabel();

  // views::ViewTargeterDelegate:
  bool DoesIntersectRect(const views::View* target,
                         const gfx::Rect& rect) const override;

  Shelf* const shelf_;

  // The controller used to determine the button's behavior.
  HomeButtonController controller_;

  // The ripple layer in the launcher nudge animation. Only exists during the
  // nudge animation.
  ui::LayerOwner nudge_ripple_layer_;

  // The label view and for launcher nudge animation.
  views::Label* nudge_label_ = nullptr;

  // The container of `nudge_label_`. This is also responsible for painting the
  // background of the label.
  views::View* label_container_ = nullptr;

  // The timer that counts down to hide the nudge_label_ from showing state.
  base::OneShotTimer label_nudge_timer_;

  // The delegate used by |nudge_ripple_layer_|. Only exists during the
  // nudge animation.
  std::unique_ptr<views::CircleLayerDelegate> ripple_layer_delegate_;

  std::unique_ptr<ScopedNoClipRect> scoped_no_clip_rect_;

  base::ObserverList<NudgeAnimationObserver> observers_;

  base::WeakPtrFactory<HomeButton> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_SHELF_HOME_BUTTON_H_
