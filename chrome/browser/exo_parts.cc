// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/exo_parts.h"

#include <memory>

#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/external_arc/keyboard/arc_input_method_surface_manager.h"
#include "ash/public/cpp/external_arc/message_center/arc_notification_surface_manager_impl.h"
#include "ash/public/cpp/external_arc/overlay/arc_overlay_manager.h"
#include "ash/public/cpp/external_arc/toast/arc_toast_surface_manager.h"
#include "ash/public/cpp/keyboard/arc/arc_input_method_bounds_tracker.h"
#include "ash/shell.h"
#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/ash/exo/chrome_data_exchange_delegate.h"
#include "chrome/browser/ash/exo/chrome_security_delegate.h"
#include "components/exo/server/wayland_server_controller.h"

// static
std::unique_ptr<ExoParts> ExoParts::CreateIfNecessary() {
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          ash::switches::kAshEnableWaylandServer)) {
    return nullptr;
  }

  return base::WrapUnique(new ExoParts());
}

ExoParts::~ExoParts() {
  ash::Shell::Get()->UntrackTrackInputMethodBounds(
      ash::ArcInputMethodBoundsTracker::Get());
  wayland_server_.reset();
}

ExoParts::ExoParts()
    : arc_overlay_manager_(std::make_unique<ash::ArcOverlayManager>()) {
  wayland_server_ = exo::WaylandServerController::CreateIfNecessary(
      std::make_unique<ash::ChromeDataExchangeDelegate>(),
      std::make_unique<ash::ChromeSecurityDelegate>(),
      std::make_unique<ash::ArcNotificationSurfaceManagerImpl>(),
      std::make_unique<ash::ArcInputMethodSurfaceManager>(),
      std::make_unique<ash::ArcToastSurfaceManager>());
  ash::Shell::Get()->TrackInputMethodBounds(
      ash::ArcInputMethodBoundsTracker::Get());
}
