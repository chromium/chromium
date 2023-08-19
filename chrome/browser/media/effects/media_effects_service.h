// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_EFFECTS_MEDIA_EFFECTS_SERVICE_H_
#define CHROME_BROWSER_MEDIA_EFFECTS_MEDIA_EFFECTS_SERVICE_H_

#include "chrome/browser/media/effects/video_effects_manager_impl.h"
#include "components/keyed_service/core/keyed_service.h"
#include "services/video_capture/public/mojom/video_effects_manager.mojom.h"

class Profile;

class MediaEffectsService : public KeyedService {
 public:
  explicit MediaEffectsService(Profile* profile);

  MediaEffectsService(const MediaEffectsService&) = delete;
  MediaEffectsService& operator=(const MediaEffectsService&) = delete;

  MediaEffectsService(MediaEffectsService&&) = delete;
  MediaEffectsService& operator=(MediaEffectsService&&) = delete;

  ~MediaEffectsService() override;

  // Connects a `VideoEffectsManagerImpl` to the provided
  // `effects_manager_receiver`. If the keyd profile already has a manager for
  // the passed `device_id`, then it will be used. Otherwise, a new manager will
  // be created.
  //
  // The device id must be the raw string from
  // `media::mojom::VideoCaptureDeviceDescriptor::device_id`.
  //
  // Note that this API only allows interacting with the manager via mojo in
  // order to support communication with the VideoCaptureService in a different
  // process. The usages in Browser UI could potentially directly interact with
  // a manager instance in order to avoid the mojo overhead, interactions
  // are expected to be very low frequency and this approach is worth that
  // tradeoff given the benefits:
  //   * A consistent interaction mechanism for both in-process and
  //     out-of-process clients
  //   * Automatic cleanup when all remotes are disconnected
  void BindVideoEffectsManager(
      const std::string& device_id,
      mojo::PendingReceiver<video_capture::mojom::VideoEffectsManager>
          effects_manager_receiver);

 private:
  VideoEffectsManagerImpl& GetOrCreateVideoEffectsManager(
      const std::string& device_id);

  void OnLastReceiverDisconnected(const std::string& device_id);

  raw_ptr<Profile> profile_;

  // Device ID strings mapped to effects manager instances.
  base::flat_map<std::string, std::unique_ptr<VideoEffectsManagerImpl>>
      video_effects_managers_;
};

#endif  // CHROME_BROWSER_MEDIA_EFFECTS_MEDIA_EFFECTS_SERVICE_H_
