// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_MESSAGE_CENTER_ARC_MOCK_ARC_NOTIFICATION_SURFACE_H_
#define ASH_SYSTEM_MESSAGE_CENTER_ARC_MOCK_ARC_NOTIFICATION_SURFACE_H_

#include "ash/system/message_center/arc/arc_notification_surface.h"

namespace ash {

class MockArcNotificationSurface : public ArcNotificationSurface {
 public:
  explicit MockArcNotificationSurface(const std::string& notification_key);
  ~MockArcNotificationSurface() override;

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

 private:
  const std::string notification_key_;
  ui::AXTreeID ax_tree_id_;
  views::NativeViewHost* native_view_host_;
  const std::unique_ptr<aura::Window> window_;
  const std::unique_ptr<aura::Window> content_window_;

  DISALLOW_COPY_AND_ASSIGN(MockArcNotificationSurface);
};

}  // namespace ash

#endif  // ASH_SYSTEM_MESSAGE_CENTER_ARC_MOCK_ARC_NOTIFICATION_SURFACE_H_
