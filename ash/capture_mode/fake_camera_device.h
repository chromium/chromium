// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CAPTURE_MODE_FAKE_CAMERA_DEVICE_H_
#define ASH_CAPTURE_MODE_FAKE_CAMERA_DEVICE_H_

#include <memory>
#include <optional>

#include "ash/ash_export.h"
#include "base/containers/flat_map.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "media/capture/video/video_capture_device_info.h"
#include "media/capture/video_capture_types.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "services/video_capture/public/mojom/video_frame_handler.mojom.h"
#include "services/video_capture/public/mojom/video_source.mojom.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace ash {

// A fake implementation of the `VideoSource` mojo API that represents a fake
// camera in ash_unittests.
class ASH_EXPORT FakeCameraDevice
    : public video_capture::mojom::VideoSource,
      public video_capture::mojom::VideoFrameAccessHandler {
 public:
  explicit FakeCameraDevice(const media::VideoCaptureDeviceInfo& device_info);
  FakeCameraDevice(const FakeCameraDevice&) = delete;
  FakeCameraDevice& operator=(const FakeCameraDevice&) = delete;
  ~FakeCameraDevice() override;

  // The max size of the `buffer_pool_` which contains the buffers backing the
  // video frames produced by this device.
  static constexpr size_t kMaxBufferCount = 4;

  // Returns the same bitmap used to paint the produced video frames from this
  // camera device for purposes of comparing them with the received frames in
  // tests.
  static SkBitmap GetProducedFrameAsBitmap(const gfx::Size& frame_size);

  const media::VideoCaptureDeviceInfo& device_info() const {
    return device_info_;
  }

  // Binds the given `pending_receiver` to this instance.
  void Bind(mojo::PendingReceiver<video_capture::mojom::VideoSource>
                pending_receiver);

  // Simulates a fatal error on this camera device, by sending an error of type
  // `kCrosHalV3DeviceDelegateMojoConnectionError` to the `VideoFrameHandler`.
  // See https://crbug.com/1316230 for more details.
  void TriggerFatalError();

  // video_capture::mojom::VideoSource:
  void CreatePushSubscription(
      mojo::PendingRemote<video_capture::mojom::VideoFrameHandler> subscriber,
      const media::VideoCaptureParams& requested_settings,
      bool force_reopen_with_new_settings,
      mojo::PendingReceiver<video_capture::mojom::PushVideoStreamSubscription>
          subscription,
      CreatePushSubscriptionCallback callback) override;

  void RegisterVideoEffectsProcessor(
      mojo::PendingRemote<video_effects::mojom::VideoEffectsProcessor> remote)
      override;

  // video_capture::mojom::VideoFrameAccessHandler:
  void OnFinishedConsumingBuffer(int32_t buffer_id) override;

 private:
  class Buffer;
  class Subscription;

  // Creates a new pending remote whose receiver is bound to this instance. The
  // pending remote will be sent to a remote `VideoFrameHandler` so it can
  // notify us when done consuming a buffer.
  mojo::PendingRemote<video_capture::mojom::VideoFrameAccessHandler>
  GetNewAccessHandlerRemote();

  // Handles the case when a subscriber `VideoFrameHandler` gets disconnected
  // from this instance.
  void OnSubscriberDisconnected(Subscription* subscription);

  // Called when the subscription activation changes or the settings used to
  // open this device changes. If this if for a specific `subscription`, it will
  // provided (i.e not null).
  void OnSubscriptionActivationChanged(Subscription* subscription);

  // Returns true if any of the current subscriptions to this device are active,
  // and therefore should be provided with video frames.
  bool IsAnySubscriptionActive() const;

  // Called repeatedly on every tick of `frame_timer_` which depends on the
  // frame desired to open this device.
  void OnNextFrame();

  // Allocates a new buffer (and therefore notifies active subscribers with
  // `OnNewBuffer()`) or reuses an existing one. Either way, the buffer is
  // returned. If all buffers in the `buffer_pool_` are used, and there's no
  // room for another buffer, active-non-suspended subscribers are notfied with
  // `OnFrameDropped()`, and nullptr is returned.
  Buffer* AllocateOrReuseBuffer();

  // Clears all the buffers in `buffer_pool_` and notifies active subscribers
  // with `OnBufferRetired()`.
  void RetireAllBuffers();

  // The device info this camera device has.
  const media::VideoCaptureDeviceInfo device_info_;

  mojo::ReceiverSet<video_capture::mojom::VideoSource>
      video_source_receiver_set_;

  mojo::ReceiverSet<video_capture::mojom::VideoFrameAccessHandler>
      access_handler_receiver_set_;

  // Maps each owned subscription instance by its instance ptr.
  base::flat_map<Subscription*, std::unique_ptr<Subscription>>
      subscriptions_map_;

  // The current settings used to open this device. It's a nullopt until a
  // subscription is created to this device.
  std::optional<media::VideoCaptureParams> current_settings_;

  // Maps each buffer by its buffer ID.
  base::flat_map</*buffer_id=*/int, std::unique_ptr<Buffer>> buffer_pool_;

  // The time at which this device started producing video frames.
  base::Time start_time_;

  // The timer that invokes `OnNextFrame()` repeatedly depending on the frame
  // rate requested to open this device.
  base::RepeatingTimer frame_timer_;
};

}  // namespace ash

#endif  // ASH_CAPTURE_MODE_FAKE_CAMERA_DEVICE_H_
