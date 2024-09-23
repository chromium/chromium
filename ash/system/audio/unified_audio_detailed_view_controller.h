// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_AUDIO_UNIFIED_AUDIO_DETAILED_VIEW_CONTROLLER_H_
#define ASH_SYSTEM_AUDIO_UNIFIED_AUDIO_DETAILED_VIEW_CONTROLLER_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/system/unified/detailed_view_controller.h"
#include "base/memory/raw_ptr.h"
#include "chromeos/ash/components/audio/cras_audio_handler.h"

namespace ash {

class AudioDetailedView;
class DetailedViewDelegate;
class UnifiedSystemTrayController;

// Controller of Audio detailed view in UnifiedSystemTray.
class ASH_EXPORT UnifiedAudioDetailedViewController
    : public DetailedViewController,
      public CrasAudioHandler::AudioObserver {
 public:
  explicit UnifiedAudioDetailedViewController(
      UnifiedSystemTrayController* tray_controller);

  UnifiedAudioDetailedViewController(
      const UnifiedAudioDetailedViewController&) = delete;
  UnifiedAudioDetailedViewController& operator=(
      const UnifiedAudioDetailedViewController&) = delete;

  ~UnifiedAudioDetailedViewController() override;

  // DetailedViewController:
  std::unique_ptr<views::View> CreateView() override;
  std::u16string GetAccessibleName() const override;

  // CrasAudioHandler::AudioObserver.
  void OnAudioNodesChanged() override;
  void OnActiveOutputNodeChanged() override;
  void OnActiveInputNodeChanged() override;
  void OnNoiseCancellationStateChanged() override;
  void OnStyleTransferStateChanged() override;

 private:
  // Used in observers to call `AudioDetailedView::Update` on `view_`.
  void UpdateView();

  const std::unique_ptr<DetailedViewDelegate> detailed_view_delegate_;

  raw_ptr<AudioDetailedView, DanglingUntriaged> view_ = nullptr;
};

}  // namespace ash

#endif  // ASH_SYSTEM_AUDIO_UNIFIED_AUDIO_DETAILED_VIEW_CONTROLLER_H_
