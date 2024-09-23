// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_EXTERNAL_ARC_MESSAGE_CENTER_ARC_NOTIFICATION_CONTENT_VIEW_H_
#define ASH_PUBLIC_CPP_EXTERNAL_ARC_MESSAGE_CENTER_ARC_NOTIFICATION_CONTENT_VIEW_H_

#include <memory>
#include <string>

#include "ash/public/cpp/external_arc/message_center/arc_notification_item.h"
#include "ash/public/cpp/external_arc/message_center/arc_notification_surface_manager.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "ui/aura/window_observer.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/message_center/views/notification_background_painter.h"
#include "ui/message_center/views/notification_control_buttons_view.h"
#include "ui/views/controls/native/native_view_host.h"
#include "ui/views/widget/widget_observer.h"

namespace message_center {
class Notification;
class NotificationControlButtonsView;
}  // namespace message_center

namespace ui {
class LayerTreeOwner;
}

namespace views {
class FocusTraversable;
class Widget;
}  // namespace views

namespace ash {

class ArcNotificationSurface;

// ArcNotificationContentView is a view to host NotificationSurface and show the
// content in itself. This is implemented as a child of ArcNotificationView.
class ArcNotificationContentView
    : public views::NativeViewHost,
      public aura::WindowObserver,
      public ArcNotificationItem::Observer,
      public ArcNotificationSurfaceManager::Observer,
      public views::WidgetObserver {
  METADATA_HEADER(ArcNotificationContentView, views::NativeViewHost)

 public:
  static int GetNotificationContentViewWidth();

  ArcNotificationContentView(ArcNotificationItem* item,
                             const message_center::Notification& notification,
                             message_center::MessageView* message_view);
  ArcNotificationContentView(const ArcNotificationContentView&) = delete;
  ArcNotificationContentView& operator=(const ArcNotificationContentView&) = delete;
  ~ArcNotificationContentView() override;

  void Update(const message_center::Notification& notification);
  message_center::NotificationControlButtonsView* GetControlButtonsView();
  void UpdateControlButtonsVisibility();
  void UpdateCornerRadius(float top_radius, float bottom_radius);
  void OnSlideChanged(bool in_progress);
  void OnContainerAnimationStarted();
  void OnContainerAnimationEnded();
  void ActivateWidget(bool activate);

  bool slide_in_progress() const { return slide_in_progress_; }

  // views::NativeViewHost
  void SetVisible(bool visible) override;

 private:
  friend class ArcNotificationViewTest;
  friend class ArcNotificationContentViewTest;
  FRIEND_TEST_ALL_PREFIXES(ArcNotificationContentViewTest,
                           ActivateWhenRemoteInputOpens);
  FRIEND_TEST_ALL_PREFIXES(ArcNotificationContentViewTest,
                           AccessibleProperties);

  class EventForwarder;
  class MouseEnterExitHandler;
  class SettingsButton;
  class SlideHelper;

  void CreateCloseButton();
  void CreateSettingsButton();
  void MaybeCreateFloatingControlButtons();
  void SetSurface(ArcNotificationSurface* surface);
  void UpdatePreferredSize();
  void UpdateSnapshot();
  void AttachSurface();
  void SetExpanded(bool expanded);
  bool IsExpanded() const;
  void SetManuallyExpandedOrCollapsed(bool value);
  bool IsManuallyExpandedOrCollapsed() const;
  void EnsureSurfaceAttached();
  void EnsureSurfaceDetached();

  void ShowCopiedSurface();
  void HideCopiedSurface();

  // views::NativeViewHost
  void ViewHierarchyChanged(
      const views::ViewHierarchyChangedDetails& details) override;
  void Layout(PassKey) override;
  void OnPaint(gfx::Canvas* canvas) override;
  void OnMouseEntered(const ui::MouseEvent& event) override;
  void OnMouseExited(const ui::MouseEvent& event) override;
  void OnFocus() override;
  void OnBlur() override;
  void OnThemeChanged() override;
  views::FocusTraversable* GetFocusTraversable() override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  void OnAccessibilityEvent(ax::mojom::Event event) override;
  void AddedToWidget() override;
  void RemovedFromWidget() override;
  void VisibilityChanged(View* starting_from, bool is_visible) override;

  // aura::WindowObserver
  void OnWindowBoundsChanged(aura::Window* window,
                             const gfx::Rect& old_bounds,
                             const gfx::Rect& new_bounds,
                             ui::PropertyChangeReason reason) override;
  void OnWindowDestroying(aura::Window* window) override;

  // views::WidgetObserver:
  void OnWidgetDestroying(views::Widget* widget) override;
  void OnWidgetActivationChanged(views::Widget* widget, bool active) override;

  // ArcNotificationItem::Observer
  void OnItemDestroying() override;
  void OnItemContentChanged(
      arc::mojom::ArcNotificationShownContents content) override;
  void OnRemoteInputActivationChanged(bool activated) override;

  // ArcNotificationSurfaceManager::Observer:
  void OnNotificationSurfaceAdded(ArcNotificationSurface* surface) override;
  void OnNotificationSurfaceRemoved(ArcNotificationSurface* surface) override;
  void OnNotificationSurfaceAXTreeIdChanged(
      ArcNotificationSurface* surface) override;

  void UpdateAccessibleRole();

  // If |item_| is null, we may be about to be destroyed. In this case,
  // we have to be careful about what we do.
  raw_ptr<ArcNotificationItem> item_;
  raw_ptr<ArcNotificationSurface> surface_ = nullptr;
  arc::mojom::ArcNotificationShownContents shown_content_ =
      arc::mojom::ArcNotificationShownContents::CONTENTS_SHOWN;

  // The flag to prevent an infinite loop of changing the visibility.
  bool updating_control_buttons_visibility_ = false;

  const std::string notification_key_;

  // A pre-target event handler to forward events on the surface to this view.
  // Using a pre-target event handler instead of a target handler on the surface
  // window because it has descendant aura::Window and the events on them need
  // to be handled as well.
  // TODO(xiyuan): Revisit after exo::Surface no longer has an aura::Window.
  std::unique_ptr<EventForwarder> event_forwarder_;

  // A handler which observes mouse entered and exited events for the floating
  // control buttons widget.
  std::unique_ptr<ui::EventHandler> mouse_enter_exit_handler_;

  // A helper to observe slide transform/animation and use surface layer copy
  // when a slide is in progress and restore the surface when it finishes.
  std::unique_ptr<SlideHelper> slide_helper_;

  // Whether the notification is being slid or is at the origin. This stores the
  // latest value of the |in_progress| from OnSlideChanged callback, which is
  // called during both manual swipe and automatic slide on dismissing or
  // resetting back to the origin.
  // This value is synced with the visibility of the copied surface. If the
  // value is true, the copied surface is visible instead of the original
  // surface itself. Copied surgace doesn't have control buttons so they must be
  // hidden if it's true.
  // This value is stored in case of the change of surface. When a new surface
  // sets, if this value is true, the copy of the new surface gets visible
  // instead of the copied surface itself.
  bool slide_in_progress_ = false;

  // A control buttons on top of NotificationSurface. Needed because the
  // aura::Window of NotificationSurface is added after hosting widget's
  // RootView thus standard notification control buttons are always below
  // it.
  std::unique_ptr<views::Widget> floating_control_buttons_widget_;

  // The message view which wrapps thie view. This must be the parent of this
  // view.
  const raw_ptr<message_center::MessageView> message_view_;

  // This view is owned by client (this).
  message_center::NotificationControlButtonsView control_buttons_view_;

  // Protects from call loops between Layout and OnWindowBoundsChanged.
  bool in_layout_ = false;

  // Widget which this view tree is currently attached to.
  raw_ptr<views::Widget> attached_widget_ = nullptr;

  // If it's true, the surface gets active when attached to this view.
  bool activate_on_attach_ = false;

  gfx::RoundedCornersF contents_radii_;

  std::unique_ptr<ui::LayerTreeOwner> surface_copy_;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_EXTERNAL_ARC_MESSAGE_CENTER_ARC_NOTIFICATION_CONTENT_VIEW_H_
