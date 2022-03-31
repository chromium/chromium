// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/eche_app_ui/eche_tray_stream_status_observer.h"

#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/system/eche/eche_tray.h"
#include "ash/webui/eche_app_ui/eche_stream_status_change_handler.h"
#include "ui/gfx/image/image.h"

namespace ash {

EcheTray* GetEcheTray() {
  return Shell::GetPrimaryRootWindowController()
      ->GetStatusAreaWidget()
      ->eche_tray();
}

namespace eche_app {

void LaunchBubble(const GURL& url,
                  const gfx::Image& icon,
                  const std::u16string& visible_name,
                  EcheTray::GracefulCloseCallback graceful_close_callback) {
  auto* eche_tray = ash::GetEcheTray();
  DCHECK(eche_tray);
  eche_tray->LoadBubble(url, icon, visible_name);
  eche_tray->SetGracefulCloseCallback(std::move(graceful_close_callback));
}

void CloseBubble() {
  auto* eche_tray = ash::GetEcheTray();
  if (eche_tray)
    eche_tray->PurgeAndClose();
  return;
}

EcheTrayStreamStatusObserver::EcheTrayStreamStatusObserver(
    EcheStreamStatusChangeHandler* stream_status_change_handler) {
  observed_session_.Observe(stream_status_change_handler);
}

EcheTrayStreamStatusObserver::~EcheTrayStreamStatusObserver() = default;

void EcheTrayStreamStatusObserver::OnStartStreaming() {
  OnStreamStatusChanged(mojom::StreamStatus::kStreamStatusStarted);
}

void EcheTrayStreamStatusObserver::OnStreamStatusChanged(
    mojom::StreamStatus status) {
  GetEcheTray()->OnStreamStatusChanged(status);
}

}  // namespace eche_app
}  // namespace ash
