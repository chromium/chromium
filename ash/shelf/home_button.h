// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_HOME_BUTTON_H_
#define ASH_SHELF_HOME_BUTTON_H_

#include <memory>

#include "ash/app_list/app_list_metrics.h"
#include "ash/app_list/app_list_model_provider.h"
#include "ash/app_list/quick_app_access_model.h"
#include "ash/ash_export.h"
#include "ash/public/cpp/app_list/app_list_controller_observer.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/shelf/home_button_controller.h"
#include "ash/shelf/shelf_button_delegate.h"
#include "ash/shelf/shelf_control_button.h"
#include "ash/shell_observer.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/events/devices/input_device_event_observer.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/views/view_targeter_delegate.h"

namespace views {
class AnimationBuilder;
class CircleLayerDelegate;
class ImageButton;
class Label;
}  // namespace views

namespace ui {
class LayerOwner;
}

namespace ash {

class Shelf;
class ShelfButtonDelegate;
class ShelfNavigationWidget;
class Shell;

// Button used for the AppList icon on the shelf. It opens the app list (in
// clamshell mode) or home screen (in tablet mode). Because the clamshell-mode
// app list appears like a dismissable overlay, the button is highlighted while
// the app list is open in clamshell mode.
//
// If Assistant is enabled, the button is filled in; long-pressing it will
// launch Assistant.
class ASH_EXPORT HomeButton : public ShelfControlButton,
                              public ShelfButtonDelegate,
                              public views::ViewTargeterDelegate,
                              public ShellObserver,
                              public ShelfConfig::Observer,
                              public AppListModelProvider::Observer,
                              public QuickAppAccessModel::Observer,
                              public ui::InputDeviceEventObserver {
  METADATA_HEADER(HomeButton, ShelfControlButton)

 public:
  class ScopedNoClipRect {
   public:
    explicit ScopedNoClipRect(ShelfNavigationWidget* shelf_navigation_widget);
    ScopedNoClipRect(const ScopedNoClipRect&) = delete;
    ScopedNoClipRect& operator=(const ScopedNoClipRect&) = delete;
    ~ScopedNoClipRect();

   private:
    const raw_ptr<ShelfNavigationWidget> shelf_navigation_widget_;
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

  explicit HomeButton(Shelf* shelf);

  HomeButton(const HomeButton&) = delete;
  HomeButton& operator=(const HomeButton&) = delete;

  ~HomeButton() override;

  // views::View:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;
  void Layout(PassKey) override;

  // views::Button:
  void OnGestureEvent(ui::GestureEvent* event) override;
  std::u16string GetTooltipText(const gfx::Point& p) const override;

  // ShelfButtonDelegate:
  void OnShelfButtonAboutToRequestFocusFromTabTraversal(ShelfButton* button,
                                                        bool reverse) override;
  void ButtonPressed(views::Button* sender,
                     const ui::Event& event,
                     views::InkDrop* ink_drop) override;

  // ShelfConfig::Observer:
  void OnShelfConfigUpdated() override;

  // ui::InputDeviceEventObserver:
  void OnInputDeviceConfigurationChanged(uint8_t input_device_types) override;
  void OnDeviceListsComplete() override;

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

  // Sets the button's "toggled" state - the button is toggled when the bubble
  // launcher is shown.
  void SetToggled(bool toggled);

  void AddNudgeAnimationObserverForTest(NudgeAnimationObserver* observer);
  void RemoveNudgeAnimationObserverForTest(NudgeAnimationObserver* observer);

  views::View* expandable_container_for_test() const {
    return expandable_container_;
  }

  views::Label* nudge_label_for_test() const { return nudge_label_; }

  views::ImageButton* quick_app_button_for_test() const {
    return quick_app_button_;
  }

 protected:
  // views::Button:
  void OnThemeChanged() override;

 private:
  class ButtonImageView;

  // Creates `nudge_label_` for launcher nudge.
  void CreateNudgeLabel();

  // Creates the `expandable_container_` which holds either the `nudge_label_`
  // or the `quick_app_button_`.
  void CreateExpandableContainer();

  // Creates the `quick_app_button_` to be shown next to the home button.
  void CreateQuickAppButton();

  // Called when the quick app button is pressed.
  void QuickAppButtonPressed();

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

  // Removes the quick app button from the view hierarchy.
  void RemoveQuickAppButton();

  // views::ViewTargeterDelegate:
  bool DoesIntersectRect(const views::View* target,
                         const gfx::Rect& rect) const override;

  // ShellObserver:
  void OnShellDestroying() override;

  // AppListModelProvider::Observer:
  void OnActiveAppListModelsChanged(AppListModel* model,
                                    SearchModel* search_model) override;

  // QuickAppAccessModel::Observer:
  void OnQuickAppShouldShowChanged(bool quick_app_shown) override;
  void OnQuickAppIconChanged() override;

  // Create and animate in the quick app button from behind the home button.
  void AnimateQuickAppButtonIn();

  // Animate out the quick app button, deleting the quick app button when
  // completed.
  void AnimateQuickAppButtonOut();

  // Callback for the quick app button slide out animation.
  void OnQuickAppButtonSlideOutDone();

  // Returns a transform which will translate the child of the
  // `expandable_container` to be placed behind the home button.
  gfx::Transform GetTransformForContainerChildBehindHomeButton();

  // Returns a clip rect which will clip the `expandable_container` to the
  // bounds of the home button.
  gfx::Rect GetExpandableContainerClipRectToHomeButton();

  base::ScopedObservation<QuickAppAccessModel, QuickAppAccessModel::Observer>
      quick_app_model_observation_{this};

  base::ScopedObservation<Shell, ShellObserver> shell_observation_{this};

  base::ScopedObservation<AppListModelProvider, AppListModelProvider::Observer>
      app_list_model_observation_{this};

  const raw_ptr<Shelf> shelf_;

  // The view that paints the home button content. In its own view to ensure
  // the background is stacked above `expandable_container_`.
  raw_ptr<ButtonImageView> button_image_view_ = nullptr;

  // The container of `nudge_label_` or `quick_app_button_`. This is also
  // responsible for painting the background of the contents. This container can
  // expand visually by animation.
  raw_ptr<views::View> expandable_container_ = nullptr;

  // The app button which is shown next to the home button. Only shown when
  // set by SetQuickApp().
  raw_ptr<views::ImageButton> quick_app_button_ = nullptr;

  // The controller used to determine the button's behavior.
  HomeButtonController controller_;

  // The delegate used by |nudge_ripple_layer_|. Only exists during the
  // nudge animation.
  std::unique_ptr<views::CircleLayerDelegate> ripple_layer_delegate_;

  // The ripple layer in the launcher nudge animation. Only exists during the
  // nudge animation.
  ui::LayerOwner nudge_ripple_layer_;

  // The label view and for launcher nudge animation.
  raw_ptr<views::Label> nudge_label_ = nullptr;

  // The timer that counts down to hide the nudge_label_ from showing state.
  base::OneShotTimer label_nudge_timer_;

  std::unique_ptr<ScopedNoClipRect> scoped_no_clip_rect_;

  base::ObserverList<NudgeAnimationObserver> observers_;

  base::WeakPtrFactory<HomeButton> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_SHELF_HOME_BUTTON_H_
