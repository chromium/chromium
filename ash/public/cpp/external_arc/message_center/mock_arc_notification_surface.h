// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_EXTERNAL_ARC_MESSAGE_CENTER_MOCK_ARC_NOTIFICATION_SURFACE_H_
#define ASH_PUBLIC_CPP_EXTERNAL_ARC_MESSAGE_CENTER_MOCK_ARC_NOTIFICATION_SURFACE_H_

#include "ash/public/cpp/external_arc/message_center/arc_notification_surface.h"
#include "base/memory/raw_ptr.h"

namespace ash {

class MockArcNotificationSurface : public ArcNotificationSurface {
 public:
  explicit MockArcNotificationSurface(const std::string& notification_key);

  MockArcNotificationSurface(const MockArcNotificationSurface&) = delete;
  MockArcNotificationSurface& operator=(const MockArcNotificationSurface&) =
      delete;

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
  raw_ptr<views::NativeViewHost> native_view_host_;
  const std::unique_ptr<aura::Window> window_;
  const std::unique_ptr<aura::Window> content_window_;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_EXTERNAL_ARC_MESSAGE_CENTER_MOCK_ARC_NOTIFICATION_SURFACE_H_
