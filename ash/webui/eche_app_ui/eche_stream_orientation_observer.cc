// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/eche_app_ui/eche_stream_orientation_observer.h"

#include "ash/constants/ash_features.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/system/eche/eche_tray.h"

namespace ash {

namespace {

EcheTray* GetEcheTray() {
  return Shell::GetPrimaryRootWindowController()
      ->GetStatusAreaWidget()
      ->eche_tray();
}

}  // namespace

namespace eche_app {

EcheStreamOrientationObserver::EcheStreamOrientationObserver() = default;

EcheStreamOrientationObserver::~EcheStreamOrientationObserver() = default;

void EcheStreamOrientationObserver::OnStreamOrientationChanged(
    mojom::StreamOrientation orientation) {
  if (features::IsEcheSWAEnabled()) {
    GetEcheTray()->OnStreamOrientationChanged(orientation);
  }
}

void EcheStreamOrientationObserver::Bind(
    mojo::PendingReceiver<mojom::StreamOrientationObserver> receiver) {
  stream_orientation_receiver_.reset();
  stream_orientation_receiver_.Bind(std::move(receiver));
}

}  // namespace eche_app
}  // namespace ash
