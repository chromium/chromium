// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_EFFECTS_VIDEO_EFFECTS_MANAGER_IMPL_H_
#define CHROME_BROWSER_MEDIA_EFFECTS_VIDEO_EFFECTS_MANAGER_IMPL_H_

#include "base/allocator/partition_allocator/pointers/raw_ptr.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "services/video_capture/public/mojom/video_effects_manager.mojom.h"

class PrefService;

class VideoEffectsManagerImpl
    : public video_capture::mojom::VideoEffectsManager {
 public:
  VideoEffectsManagerImpl(PrefService* pref_service,
                          base::OnceClosure last_receiver_disconnected_handler);

  VideoEffectsManagerImpl(const VideoEffectsManagerImpl&) = delete;
  VideoEffectsManagerImpl& operator=(const VideoEffectsManagerImpl&) = delete;

  ~VideoEffectsManagerImpl() override;

  void Bind(mojo::PendingReceiver<video_capture::mojom::VideoEffectsManager>
                receiver);

  // video_capture::mojom::VideoEffectsManager overrides
  void GetConfiguration(GetConfigurationCallback callback) override;
  void SetConfiguration(
      video_capture::mojom::VideoEffectsConfigurationPtr configuration,
      SetConfigurationCallback callback) override;
  void AddObserver(mojo::PendingRemote<
                   video_capture::mojom::VideoEffectsConfigurationObserver>
                       observer) override;

 private:
  void OnReceiverDisconnected();

  raw_ptr<PrefService> pref_service_;
  base::OnceClosure last_receiver_disconnected_handler_;
  mojo::ReceiverSet<video_capture::mojom::VideoEffectsManager> receivers_;

  video_capture::mojom::VideoEffectsConfigurationPtr configuration_;
  mojo::RemoteSet<video_capture::mojom::VideoEffectsConfigurationObserver>
      observers_;
};

#endif  // CHROME_BROWSER_MEDIA_EFFECTS_VIDEO_EFFECTS_MANAGER_IMPL_H_
