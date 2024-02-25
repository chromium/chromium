// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_EXTERNAL_ARC_MESSAGE_CENTER_ARC_NOTIFICATION_SURFACE_IMPL_H_
#define ASH_PUBLIC_CPP_EXTERNAL_ARC_MESSAGE_CENTER_ARC_NOTIFICATION_SURFACE_IMPL_H_

#include <memory>

#include "ash/public/cpp/external_arc/message_center/arc_notification_surface.h"
#include "base/memory/raw_ptr.h"

namespace exo {
class NotificationSurface;
}

namespace ash {

// Handles notification surface role of a given surface.
class ArcNotificationSurfaceImpl : public ArcNotificationSurface {
 public:
  explicit ArcNotificationSurfaceImpl(exo::NotificationSurface* surface);

  ArcNotificationSurfaceImpl(const ArcNotificationSurfaceImpl&) = delete;
  ArcNotificationSurfaceImpl& operator=(const ArcNotificationSurfaceImpl&) =
      delete;

  ~ArcNotificationSurfaceImpl() override;

  // ArcNotificationSurface overrides:
  gfx::Size GetSize() const override;
  aura::Window* GetWindow() const override;
  aura::Window* GetContentWindow() const override;
  const std::string& GetNotificationKey() const override;
  void Attach(views::NativeViewHost* native_view_host) override;
  void Detach() override;
  bool IsAttached() const override;
  views::NativeViewHost* GetAttachedHost() const override;
  void FocusSurfaceWindow() override;
  void SetAXTreeId(ui::AXTreeID ax_tree_id) override;
  ui::AXTreeID GetAXTreeId() const override;

  exo::NotificationSurface* surface() const { return surface_; }

 private:
  raw_ptr<exo::NotificationSurface> surface_;
  raw_ptr<views::NativeViewHost> native_view_host_ = nullptr;
  std::unique_ptr<aura::Window> native_view_;
  ui::AXTreeID ax_tree_id_ = ui::AXTreeIDUnknown();
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_EXTERNAL_ARC_MESSAGE_CENTER_ARC_NOTIFICATION_SURFACE_IMPL_H_
