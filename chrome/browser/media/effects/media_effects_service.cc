// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/effects/media_effects_service.h"
#include "chrome/browser/profiles/profile.h"
#include "services/video_capture/public/mojom/video_effects_manager.mojom.h"

MediaEffectsService::MediaEffectsService(Profile* profile)
    : profile_(profile) {}

MediaEffectsService::~MediaEffectsService() = default;

void MediaEffectsService::BindVideoEffectsManager(
    const std::string& device_id,
    mojo::PendingReceiver<video_capture::mojom::VideoEffectsManager>
        effects_manager_receiver) {
  auto& effects_manager = GetOrCreateVideoEffectsManager(device_id);
  effects_manager.Bind(std::move(effects_manager_receiver));
}

VideoEffectsManagerImpl& MediaEffectsService::GetOrCreateVideoEffectsManager(
    const std::string& device_id) {
  if (auto effects_manager = video_effects_managers_.find(device_id);
      effects_manager != video_effects_managers_.end()) {
    return *effects_manager->second;
  }

  // base::Unretained is safe here because `this` owns the
  // `VideoEffectsManagerImpl` that would call this callback.
  auto [effects_manager, inserted] = video_effects_managers_.emplace(
      device_id,
      std::make_unique<VideoEffectsManagerImpl>(
          profile_->GetPrefs(),
          base::BindOnce(&MediaEffectsService::OnLastReceiverDisconnected,
                         base::Unretained(this), device_id)));
  CHECK(inserted);
  return *effects_manager->second;
}

void MediaEffectsService::OnLastReceiverDisconnected(
    const std::string& device_id) {
  video_effects_managers_.erase(device_id);
}
