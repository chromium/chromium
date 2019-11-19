// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_MESSAGE_CENTER_ARC_ARC_NOTIFICATION_CONTENT_VIEW_H_
#define ASH_SYSTEM_MESSAGE_CENTER_ARC_ARC_NOTIFICATION_CONTENT_VIEW_H_

#include <memory>
#include <string>

#include "ash/system/message_center/arc/arc_notification_item.h"
#include "ash/system/message_center/arc/arc_notification_surface_manager.h"
#include "base/macros.h"
#include "ui/aura/window_observer.h"
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
 public:
  static const char kViewClassName[];

  ArcNotificationContentView(ArcNotificationItem* item,
                             const message_center::Notification& notification,
                             message_center::MessageView* message_view);
  ~ArcNotificationContentView() override;

  // views::View overrides:
  const char* GetClassName() const override;

  void Update(const message_center::Notification& notification);
  message_center::NotificationControlButtonsView* GetControlButtonsView();
  void UpdateControlButtonsVisibility();
  void UpdateCornerRadius(int top_radius, int bottom_radius);
  void OnSlideChanged(bool in_progress);
  void OnContainerAnimationStarted();
  void OnContainerAnimationEnded();
  void ActivateWidget(bool activate);

 private:
  friend class ArcNotificationViewTest;
  friend class ArcNotificationContentViewTest;
  FRIEND_TEST_ALL_PREFIXES(ArcNotificationContentViewTest,
                           ActivateWhenRemoteInputOpens);

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

  void ShowCopiedSurface();
  void HideCopiedSurface();

  // Generates a mask using |top_radius_| and |bottom_radius_| and installs it.
  void UpdateMask(bool force_update);

  // views::NativeViewHost
  void ViewHierarchyChanged(
      const views::ViewHierarchyChangedDetails& details) override;
  void Layout() override;
  void OnPaint(gfx::Canvas* canvas) override;
  void OnMouseEntered(const ui::MouseEvent& event) override;
  void OnMouseExited(const ui::MouseEvent& event) override;
  void OnFocus() override;
  void OnBlur() override;
  views::FocusTraversable* GetFocusTraversable() override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  void OnAccessibilityEvent(ax::mojom::Event event) override;
  void AddedToWidget() override;
  void RemovedFromWidget() override;

  // aura::WindowObserver
  void OnWindowBoundsChanged(aura::Window* window,
                             const gfx::Rect& old_bounds,
                             const gfx::Rect& new_bounds,
                             ui::PropertyChangeReason reason) override;
  void OnWindowDestroying(aura::Window* window) override;

  // views::WidgetObserver:
  void OnWidgetClosing(views::Widget* widget) override;
  void OnWidgetActivationChanged(views::Widget* widget, bool active) override;

  // ArcNotificationItem::Observer
  void OnItemDestroying() override;
  void OnItemContentChanged(
      arc::mojom::ArcNotificationShownContents content) override;
  void OnRemoteInputActivationChanged(bool activated) override;

  // ArcNotificationSurfaceManager::Observer:
  void OnNotificationSurfaceAdded(ArcNotificationSurface* surface) override;
  void OnNotificationSurfaceRemoved(ArcNotificationSurface* surface) override;

  // If |item_| is null, we may be about to be destroyed. In this case,
  // we have to be careful about what we do.
  ArcNotificationItem* item_;
  ArcNotificationSurface* surface_ = nullptr;
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
  message_center::MessageView* const message_view_;

  // This view is owned by client (this).
  message_center::NotificationControlButtonsView control_buttons_view_;

  // Protects from call loops between Layout and OnWindowBoundsChanged.
  bool in_layout_ = false;

  // Widget which this view tree is currently attached to.
  views::Widget* attached_widget_ = nullptr;

  base::string16 accessible_name_;

  // If it's true, the surface gets active when attached to this view.
  bool activate_on_attach_ = false;

  // Radiuses of rounded corners. These values are used in UpdateMask().
  int top_radius_ = 0;
  int bottom_radius_ = 0;

  // Current insets of mask layer.
  base::Optional<gfx::Insets> mask_insets_;

  std::unique_ptr<ui::LayerTreeOwner> surface_copy_;

  DISALLOW_COPY_AND_ASSIGN(ArcNotificationContentView);
};

}  // namespace ash

#endif  // ASH_SYSTEM_MESSAGE_CENTER_ARC_ARC_NOTIFICATION_CONTENT_VIEW_H_
