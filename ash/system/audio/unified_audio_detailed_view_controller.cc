// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/audio/unified_audio_detailed_view_controller.h"

#include "ash/strings/grit/ash_strings.h"
#include "ash/system/audio/audio_detailed_view.h"
#include "ash/system/tray/detailed_view_delegate.h"
#include "ui/base/l10n/l10n_util.h"

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

std::unique_ptr<views::View> UnifiedAudioDetailedViewController::CreateView() {
  DCHECK(!view_);
  auto view =
      std::make_unique<AudioDetailedView>(detailed_view_delegate_.get());
  view_ = view.get();
  view_->Update();
  return view;
}

std::u16string UnifiedAudioDetailedViewController::GetAccessibleName() const {
  return l10n_util::GetStringUTF16(
      IDS_ASH_QUICK_SETTINGS_BUBBLE_AUDIO_SETTINGS_ACCESSIBLE_DESCRIPTION);
}

void UnifiedAudioDetailedViewController::OnAudioNodesChanged() {
  UpdateView();
}

void UnifiedAudioDetailedViewController::OnActiveOutputNodeChanged() {
  UpdateView();
}

void UnifiedAudioDetailedViewController::OnActiveInputNodeChanged() {
  UpdateView();
}

void UnifiedAudioDetailedViewController::OnNoiseCancellationStateChanged() {
  UpdateView();
}

void UnifiedAudioDetailedViewController::OnStyleTransferStateChanged() {
  UpdateView();
}

void UnifiedAudioDetailedViewController::UpdateView() {
  if (view_) {
    view_->Update();
  }
}

}  // namespace ash
