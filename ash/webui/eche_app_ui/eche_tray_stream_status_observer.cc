// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/eche_app_ui/eche_tray_stream_status_observer.h"

#include "ash/constants/ash_features.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/system/eche/eche_tray.h"
#include "ash/webui/eche_app_ui/eche_stream_status_change_handler.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"
#include "ui/gfx/image/image.h"

namespace ash {

EcheTray* GetEcheTray() {
  return Shell::GetPrimaryRootWindowController()
      ->GetStatusAreaWidget()
      ->eche_tray();
}

void CloseBubble() {
  auto* eche_tray = ash::GetEcheTray();
  if (eche_tray)
    eche_tray->StartGracefulClose();
  return;
}

// Checks FeatureStatus that eche feature is not able to use.
bool NeedClose(eche_app::FeatureStatus status) {
  return status == eche_app::FeatureStatus::kIneligible ||
         status == eche_app::FeatureStatus::kDisabled ||
         status == eche_app::FeatureStatus::kDependentFeature;
}

namespace eche_app {

void LaunchBubble(const GURL& url,
                  const gfx::Image& icon,
                  const std::u16string& visible_name,
                  const std::u16string& phone_name,
                  eche_app::mojom::ConnectionStatus last_connection_status,
                  eche_app::mojom::AppStreamLaunchEntryPoint entry_point,
                  EcheTray::GracefulCloseCallback graceful_close_callback,
                  EcheTray::GracefulGoBackCallback graceful_go_back_callback,
                  EcheTray::BubbleShownCallback bubble_shown_callback) {
  auto* eche_tray = ash::GetEcheTray();
  DCHECK(eche_tray);
  eche_tray->LoadBubble(url, icon, visible_name, phone_name,
                        last_connection_status, entry_point);
  eche_tray->SetGracefulCloseCallback(std::move(graceful_close_callback));
  eche_tray->SetGracefulGoBackCallback(std::move(graceful_go_back_callback));
  eche_tray->SetBubbleShownCallback(std::move(bubble_shown_callback));
}

EcheTrayStreamStatusObserver::EcheTrayStreamStatusObserver(
    EcheStreamStatusChangeHandler* stream_status_change_handler,
    FeatureStatusProvider* feature_status_provider)
    : feature_status_provider_(feature_status_provider) {
  observed_session_.Observe(stream_status_change_handler);
  feature_status_provider_->AddObserver(this);
}

EcheTrayStreamStatusObserver::~EcheTrayStreamStatusObserver() {
  feature_status_provider_->RemoveObserver(this);
}

void EcheTrayStreamStatusObserver::OnStartStreaming() {
  OnStreamStatusChanged(mojom::StreamStatus::kStreamStatusStarted);
}

void EcheTrayStreamStatusObserver::OnStreamStatusChanged(
    mojom::StreamStatus status) {
  GetEcheTray()->OnStreamStatusChanged(status);
}

void EcheTrayStreamStatusObserver::OnFeatureStatusChanged() {
  if (NeedClose(feature_status_provider_->GetStatus()) &&
      !base::FeatureList::IsEnabled(features::kEcheSWADebugMode)) {
    PA_LOG(INFO) << "Close Eche window when feature status: "
                 << feature_status_provider_->GetStatus();
    CloseBubble();
  }
}

}  // namespace eche_app
}  // namespace ash
