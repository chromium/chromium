// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/external_arc/message_center/mock_arc_notification_surface.h"

#include "ui/aura/window.h"

namespace ash {

MockArcNotificationSurface::MockArcNotificationSurface(
    const std::string& notification_key)
    : notification_key_(notification_key),
      ax_tree_id_(ui::AXTreeIDUnknown()),
      native_view_host_(nullptr),
      window_(
          std::make_unique<aura::Window>(nullptr,
                                         aura::client::WINDOW_TYPE_UNKNOWN)),
      content_window_(
          std::make_unique<aura::Window>(nullptr,
                                         aura::client::WINDOW_TYPE_UNKNOWN)) {
  window_->Init(ui::LAYER_NOT_DRAWN);
  content_window_->Init(ui::LAYER_NOT_DRAWN);
}

MockArcNotificationSurface::~MockArcNotificationSurface() = default;

gfx::Size MockArcNotificationSurface::GetSize() const {
  return gfx::Size();
}

aura::Window* MockArcNotificationSurface::GetWindow() const {
  return window_.get();
}

aura::Window* MockArcNotificationSurface::GetContentWindow() const {
  return content_window_.get();
}

const std::string& MockArcNotificationSurface::GetNotificationKey() const {
  return notification_key_;
}

void MockArcNotificationSurface::Attach(
    views::NativeViewHost* native_view_host) {
  native_view_host_ = native_view_host;
}

void MockArcNotificationSurface::Detach() {
  native_view_host_ = nullptr;
}

bool MockArcNotificationSurface::IsAttached() const {
  return native_view_host_ != nullptr;
}

views::NativeViewHost* MockArcNotificationSurface::GetAttachedHost() const {
  return native_view_host_;
}

void MockArcNotificationSurface::FocusSurfaceWindow() {}

void MockArcNotificationSurface::SetAXTreeId(ui::AXTreeID ax_tree_id) {
  ax_tree_id_ = ax_tree_id;
}

ui::AXTreeID MockArcNotificationSurface::GetAXTreeId() const {
  return ax_tree_id_;
}

}  // namespace ash
