// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/audio/unified_audio_detailed_view_controller.h"

#include "ash/system/audio/audio_detailed_view.h"
#include "ash/system/tray/detailed_view_delegate.h"

using chromeos::CrasAudioHandler;

namespace ash {

UnifiedAudioDetailedViewController::UnifiedAudioDetailedViewController(
    UnifiedSystemTrayController* tray_controller)
    : detailed_view_delegate_(
          std::make_unique<DetailedViewDelegate>(tray_controller)) {
  CrasAudioHandler::Get()->AddAudioObserver(this);
}

UnifiedAudioDetailedViewController::~UnifiedAudioDetailedViewController() {
  CrasAudioHandler::Get()->RemoveAudioObserver(this);
}

views::View* UnifiedAudioDetailedViewController::CreateView() {
  DCHECK(!view_);
  view_ = new tray::AudioDetailedView(detailed_view_delegate_.get());
  view_->Update();
  return view_;
}

void UnifiedAudioDetailedViewController::OnAudioNodesChanged() {
  view_->Update();
}

void UnifiedAudioDetailedViewController::OnActiveOutputNodeChanged() {
  view_->Update();
}

void UnifiedAudioDetailedViewController::OnActiveInputNodeChanged() {
  view_->Update();
}

}  // namespace ash
