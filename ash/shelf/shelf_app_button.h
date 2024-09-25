// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_SHELF_APP_BUTTON_H_
#define ASH_SHELF_SHELF_APP_BUTTON_H_

#include "ash/ash_export.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/shelf/shelf_button.h"
#include "ash/shelf/shelf_button_delegate.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/gfx/shadow_value.h"
#include "ui/views/animation/ink_drop_observer.h"
#include "ui/views/animation/ink_drop_state.h"
#include "ui/views/controls/image_view.h"

namespace ash {
struct ShelfItem;
class DotIndicator;
class ProgressIndicator;
class ShelfView;

// Button used for app shortcuts on the shelf.
class ASH_EXPORT ShelfAppButton : public ShelfButton,
                                  public views::InkDropObserver,
                                  public ui::ImplicitAnimationObserver {
  METADATA_HEADER(ShelfAppButton, ShelfButton)

 public:

  // Used to indicate the current state of the button.
  enum State {
    // Nothing special. Usually represents an app shortcut item with no running
    // instance.
    STATE_NORMAL = 0,
    // Button has mouse hovering on it.
    STATE_HOVERED = 1 << 0,
    // Underlying ShelfItem has a running instance.
    STATE_RUNNING = 1 << 1,
    // Underlying ShelfItem needs user's attention.
    STATE_ATTENTION = 1 << 2,
    // Hide the status (temporarily for some animations).
    STATE_HIDDEN = 1 << 3,
    // Button is being dragged.
    STATE_DRAGGING = 1 << 4,
    // App has at least 1 notification.
    STATE_NOTIFICATION = 1 << 5,
    // Underlying ShelfItem owns the window that is currently active.
    STATE_ACTIVE = 1 << 6,
  };

  // Returns whether |event| should be handled by a ShelfAppButton if a context
  // menu for the view is shown. Note that the context menu controller will
  // redirect gesture events to the hotseat widget if the context menu was shown
  // for a ShelfAppButton). The hotseat widget uses this method to determine
  // whether such events can/should be dropped without handling.
  static bool ShouldHandleEventFromContextMenu(const ui::GestureEvent* event);

  ShelfAppButton(ShelfView* shelf_view,
                 ShelfButtonDelegate* shelf_button_delegate);

  ShelfAppButton(const ShelfAppButton&) = delete;
  ShelfAppButton& operator=(const ShelfAppButton&) = delete;

  ~ShelfAppButton() override;

  // Updates the icon image and maybe host badge icon image to display for this
  // entry.
  void UpdateMainAndMaybeHostBadgeIconImage();

  // Retrieve the image to show proxy operations.
  gfx::ImageSkia GetImage() const;

  // Gets the resized icon image represented by `icon_image_model_` without the
  // shadow, assuming the provided `icon_scale`.
  gfx::ImageSkia GetIconImage(float icon_scale) const;

  const ui::ImageModel& icon_image_model() const { return icon_image_model_; }

  views::ImageView* icon_view() { return icon_view_; }

  // Returns the badge icon image for the app assuming the provided
  // `icon_scale`. Returns an empty image if the app does not have a badge icon.
  gfx::ImageSkia GetBadgeIconImage(float icon_scale) const;

  // Sets the `icon_image_model_`, and maybe `host_badge_image_` depending on
  // `has_host_badge` for this entry. If |is_placeholder_icon| is true, the
  // |main_image| will be ignored and this entry will be assigned a placeholder
  // vector icon.
  void SetMainAndMaybeHostBadgeImage(const gfx::ImageSkia& main_image,
                                     bool is_placeholder_icon,
                                     const gfx::ImageSkia& host_badge_image);

  // |state| is or'd into the current state.
  void AddState(State state);
  void ClearState(State state);
  int state() const { return state_; }

  // Clears drag drag state that might have been set by gesture handling when a
  // gesture ends. No-op if the drag state has already been cleared.
  void ClearDragStateOnGestureEnd();

  // Returns the bounds of the icon.
  gfx::Rect GetIconBounds() const;

  // Returns the ideal icon bounds within the button view of the provided size,
  // and with the provided icon scale.
  gfx::Rect GetIdealIconBounds(const gfx::Size& button_size,
                               float icon_scale) const;

  views::InkDrop* GetInkDropForTesting();

  // Called when user started dragging the shelf button.
  void OnDragStarted(const ui::LocatedEvent* event);

  // Callback used when a menu for this ShelfAppButton is closed.
  void OnMenuClosed();

  // views::Button overrides:
  void ShowContextMenu(const gfx::Point& p,
                       ui::MenuSourceType source_type) override;
  bool ShouldEnterPushedState(const ui::Event& event) override;

  // views::View overrides:
  bool OnMousePressed(const ui::MouseEvent& event) override;
  void OnMouseReleased(const ui::MouseEvent& event) override;
  void OnMouseCaptureLost() override;
  bool OnMouseDragged(const ui::MouseEvent& event) override;
  void Layout(PassKey) override;
  void ChildPreferredSizeChanged(views::View* child) override;
  void OnThemeChanged() override;

  // Update button state from ShelfItem.
  void ReflectItemStatus(const ShelfItem& item);

  // Returns whether the icon size is up to date.
  bool IsIconSizeCurrent();

  // Called when the request for the context menu model is canceled.
  void OnContextMenuModelRequestCanceled();

  bool FireDragTimerForTest();
  void FireRippleActivationTimerForTest();

  // Return the bounds in the local coordinates enclosing the small ripple area.
  gfx::Rect CalculateSmallRippleArea() const;

  // Sets up the button to simulate promise app UI (icon scaled down, with
  // progress indicator shown), and animates the button into the normal app UI
  // (hides the progress indicator, and scales the app icon up).
  // Used to animate the button in when it's replacing a promise icon.
  // `fallback_icon` - the promise app icon that should be used while the app
  // button is animating in. The installed app icon is loaded asynchronously, so
  // there is a noticeable delay before the icon becomes available. Using
  // fallback icon during animation prevents jankiness in the time period the
  // app icon is loading. The jankiness manifests itself as the app icon
  // disappearing for a moment after the promise icon is installed.
  // `callback` - callback run when the animation completes.
  void AnimateInFromPromiseApp(const ui::ImageModel& fallback_icon,
                               const base::RepeatingClosure& callback);

  void SetNotificationBadgeColor(SkColor color);

  float progress() { return progress_; }

  AppStatus app_status() { return app_status_; }
  const std::string& package_id() const { return package_id_; }
  bool is_promise_app() const { return is_promise_app_; }

  ProgressIndicator* GetProgressIndicatorForTest() const;

  void UpdateAccessibleName();

 protected:
  gfx::ImageSkia GetHostBadgeImageForTest() { return host_badge_image_; }

  // ui::EventHandler:
  void OnGestureEvent(ui::GestureEvent* event) override;

  // ui::ImplicitAnimationObserver:
  void OnImplicitAnimationsCompleted() override;

  // Sets the icon image with a shadow.
  void SetShadowedImage(const gfx::ImageSkia& bitmap);

 private:
  class AppNotificationIndicatorView;
  class AppStatusIndicatorView;
  friend class ShelfViewWebAppShortcutTest;

  // views::View:
  bool HandleAccessibleAction(const ui::AXActionData& action_data) override;

  // views::InkDropObserver:
  void InkDropAnimationStarted() override;
  void InkDropRippleAnimationEnded(views::InkDropState state) override;

  // Updates the parts of the button to reflect the current |state_| and
  // alignment. This may add or remove views, layout and paint.
  void UpdateState();

  // Invoked when |touch_drag_timer_| fires to show dragging UI.
  void OnTouchDragTimer();

  // Invoked when |ripple_activation_timer_| fires to activate the ink drop.
  void OnRippleTimer();

  // Calculates the preferred size of the icon for the provided `icon_scale`.
  // This is the actual size of the main app icon that is painted in the grid.
  // with the adjusted scale.
  gfx::Size GetPreferredIconSize(const ui::ImageModel& image_model,
                                 float icon_scale) const;

  // Scales up app icon if |scale_up| is true, otherwise scales it back to
  // normal size.
  void ScaleAppIcon(bool scale_up);

  // Calculates the expected icon bounds for an icon view scaled by
  // |icon_scale|.
  gfx::Rect GetIconViewBounds(const gfx::Rect& button_bounds,
                              float icon_scale,
                              bool ignore_shadow_insets) const;

  // Calculates the bounds for either the shortcut icon container or shortcut
  // icon scaled by `icon_scale`.
  gfx::Rect GetShortcutViewBounds(const gfx::Rect& button_bounds,
                                  float icon_scale,
                                  const float icon_size) const;

  // Calculates the notification indicator bounds when scaled by |scale|.
  gfx::Rect GetNotificationIndicatorBounds(float scale);

  // Calculates the transform between the icon scaled by |icon_scale| and the
  // normal size icon.
  gfx::Transform GetScaleTransform(float icon_scale);

  // Marks whether the ink drop animation has started or not.
  void SetInkDropAnimationStarted(bool started);

  // Maybe hides the ink drop at the end of gesture handling.
  void MaybeHideInkDropWhenGestureEnds();

  // Updates the layer bounds for the `progress_indicator_` if any is currently
  // active.
  void UpdateProgressRingBounds();

  // Sets the host badge image to display for this entry
  void SetHostBadgeImage(const gfx::ImageSkia& host_badge_image);

  // Whether the image view has a placeholder icon in place. The placeholder
  // icon is represented as a VectorIcon in the ImageModel. Depending on the
  // case, the icon may use the `icon_image_model` or the
  // `fallback_icon_image_model` (ie, when an animation in for the promise app
  // is happening) for this calceulation.
  bool ImageModelHasPlaceholderIcon() const;

  // Returns the preferred icon size for promise icons depending on this
  // button's `app_state_`. Different from `GetPreferredIconSize()` since
  // `GetIconDimensionByAppState()` is used to adjust padding for the promise
  // ring.
  float GetIconDimensionByAppState() const;

  // Called when the app button completes animating in from a promise app state.
  void OnAnimatedInFromPromiseApp(base::RepeatingClosure callback);

  void UpdateAccessibleDescription();

  // The icon part of a button can be animated independently of the rest.
  raw_ptr<views::ImageView> icon_view_ = nullptr;

  // The ShelfView showing this ShelfAppButton. Owned by RootWindowController.
  const raw_ptr<ShelfView> shelf_view_;

  // Draws an indicator underneath the image to represent the state of the
  // application.
  const raw_ptr<AppStatusIndicatorView> indicator_;

  // Draws an indicator in the top right corner of the image to represent an
  // active notification.
  raw_ptr<DotIndicator> notification_indicator_ = nullptr;

  // The current application state, a bitfield of State enum values.
  int state_ = STATE_NORMAL;

  gfx::ShadowValues icon_shadows_;

  // The model image for this app button.
  ui::ImageModel icon_image_model_;

  // The bitmap image for the host badge icon if this is an App Shortcut.
  gfx::ImageSkia host_badge_image_;

  // The scaling factor for displaying the app icon.
  float icon_scale_ = 1.0f;

  // App status.
  AppStatus app_status_ = AppStatus::kReady;

  // Item progress. Only applicable if `is_promise_app_` is true.
  float progress_ = -1.0f;

  // Indicates whether the ink drop animation starts.
  bool ink_drop_animation_started_ = false;

  // A timer to defer showing drag UI when the shelf button is pressed.
  base::OneShotTimer drag_timer_;

  // A timer to activate the ink drop ripple during a long press.
  base::OneShotTimer ripple_activation_timer_;

  // The target visibility of the shelf app's context menu.
  // NOTE: when `context_menu_target_visibility_` is true, the context menu may
  // not show yet due to the async request for the menu model.
  bool context_menu_target_visibility_ = false;

  // An object that draws and updates the progress ring around promise app
  // icons.
  std::unique_ptr<ProgressIndicator> progress_indicator_;

  std::unique_ptr<ShelfButtonDelegate::ScopedActiveInkDropCount>
      ink_drop_count_;

  // Whether the app is a promise app  (i.e. an app with pending or installing
  // app status).
  bool is_promise_app_ = false;

  // The package id that is associated with this shelf app.
  std::string package_id_;

  // Whether the app has a host badge (i.e. an App Shortcut).
  bool has_host_badge_ = false;

  // The fallback icon used as the app button image when the app is animated in
  // from a promise icon. The fallback icon will be used at least until the
  // actual app icon has been loaded. This prevents a flash of an empty icon
  // when the app icon replaces a promise icon.
  ui::ImageModel fallback_icon_image_model_;

  // Whether the fallback icon should be used even if the actual app icon is
  // available. This will be set animating the app button in from promise app
  // state to prevent app icon changes mid animation.
  bool force_fallback_icon_ = false;

  std::optional<float> forced_progress_indicator_value_;

  // Whether the non-placeholder app icon has been loaded for the app.
  bool has_icon_image_ = false;

  // Used to track whether the menu was deleted while running. Must be last.
  base::WeakPtrFactory<ShelfAppButton> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_SHELF_SHELF_APP_BUTTON_H_
