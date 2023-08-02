// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/effects/video_effects_manager_impl.h"

VideoEffectsManagerImpl::VideoEffectsManagerImpl(
    PrefService* pref_service,
    base::OnceClosure last_receiver_disconnected_handler)
    : pref_service_(pref_service),
      last_receiver_disconnected_handler_(
          std::move(last_receiver_disconnected_handler)) {
  configuration_ = video_capture::mojom::VideoEffectsConfiguration::New();
  receivers_.set_disconnect_handler(
      base::BindRepeating(&VideoEffectsManagerImpl::OnReceiverDisconnected,
                          base::Unretained(this)));
}

VideoEffectsManagerImpl::~VideoEffectsManagerImpl() = default;

void VideoEffectsManagerImpl::Bind(
    mojo::PendingReceiver<video_capture::mojom::VideoEffectsManager> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void VideoEffectsManagerImpl::GetConfiguration(
    GetConfigurationCallback callback) {
  std::move(callback).Run(configuration_->Clone());
}

void VideoEffectsManagerImpl::SetConfiguration(
    video_capture::mojom::VideoEffectsConfigurationPtr configuration,
    SetConfigurationCallback callback) {
  configuration_ = configuration->Clone();
  for (const auto& observer : observers_) {
    observer->OnConfigurationChanged(configuration_->Clone());
  }
  std::move(callback).Run(video_capture::mojom::SetConfigurationResult::kOk);
}

void VideoEffectsManagerImpl::AddObserver(
    mojo::PendingRemote<video_capture::mojom::VideoEffectsConfigurationObserver>
        observer) {
  observers_.Add(std::move(observer));
}

void VideoEffectsManagerImpl::OnReceiverDisconnected() {
  if (!receivers_.empty()) {
    return;
  }

  std::move(last_receiver_disconnected_handler_).Run();
}
