// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/capture_mode/fake_camera_device.h"

#include <cstring>
#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/shared_memory_mapping.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/notreached.h"
#include "cc/paint/paint_flags.h"
#include "cc/paint/skia_paint_canvas.h"
#include "components/viz/common/resources/shared_image_format.h"
#include "gpu/command_buffer/client/client_shared_image.h"
#include "gpu/command_buffer/client/gpu_memory_buffer_manager.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/ipc/common/surface_handle.h"
#include "media/base/video_frame.h"
#include "media/base/video_types.h"
#include "media/capture/mojom/video_capture_buffer.mojom.h"
#include "media/capture/video_capture_types.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/video_capture/public/mojom/video_frame_handler.mojom.h"
#include "third_party/skia/include/core/SkRect.h"
#include "ui/aura/env.h"
#include "ui/compositor/compositor.h"

namespace ash {

namespace {

// The next ID to be used for a newly created buffer.
int g_next_buffer_id = 0;

scoped_refptr<gpu::ClientSharedImage> CreateSharedImage(
    const gfx::Size& frame_size) {
  gpu::SharedImageUsageSet shared_image_usage =
      gpu::SHARED_IMAGE_USAGE_DISPLAY_READ |
      gpu::SHARED_IMAGE_USAGE_CONCURRENT_READ_WRITE |
      gpu::SHARED_IMAGE_USAGE_SCANOUT;
  return aura::Env::GetInstance()
      ->context_factory()
      ->SharedMainThreadRasterContextProvider()
      ->SharedImageInterface()
      ->CreateSharedImage(
          {viz::SinglePlaneFormat::kBGRA_8888, frame_size, gfx::ColorSpace(),
           shared_image_usage, "FakeCameraDevice"},
          gpu::kNullSurfaceHandle, gfx::BufferUsage::SCANOUT_CPU_READ_WRITE);
}

SkRect GetCircleRect(const gfx::Point& center, int radius) {
  return SkRect::MakeLTRB(center.x() - radius, center.y() - radius,
                          center.x() + radius, center.y() + radius);
}

// Draws the content of the produced video frame whose size is the given
// `frame_size` on the given `canvas`.
void DrawFrameOnCanvas(cc::SkiaPaintCanvas&& canvas,
                       const gfx::Size& frame_size) {
  cc::PaintFlags flags;
  flags.setStyle(cc::PaintFlags::kFill_Style);
  const auto canvas_bounds =
      SkRect::MakeIWH(frame_size.width(), frame_size.height());
  flags.setColor(SK_ColorBLUE);
  canvas.drawRect(canvas_bounds, flags);
  flags.setColor(SK_ColorGREEN);
  const auto canvas_center = gfx::Rect(frame_size).CenterPoint();
  const int radius = 20;
  canvas.drawOval(GetCircleRect(canvas_center, radius), flags);
  flags.setColor(SK_ColorRED);
  canvas.drawOval(GetCircleRect(gfx::Point(), radius), flags);
}

// -----------------------------------------------------------------------------
// BufferStrategy:

// Defines an interface for operations that can be done on different buffer
// types.
class BufferStrategy {
 public:
  virtual ~BufferStrategy() = default;

  // Gets the handle of the concrete buffer implementation that can be sent over
  // via mojo.
  virtual media::mojom::VideoBufferHandlePtr GetHandle() const = 0;

  // Draws the contents of a video frame whose size is `frame_size` on the
  // buffer backing this object.
  virtual void DrawFrameOnBuffer(const gfx::Size& frame_size) = 0;
};

// -----------------------------------------------------------------------------
// GpuMemoryBufferStrategy:

// Defines a concrete implementation of `BufferStrategy` which creates a
// `GpuMemoryBuffer` and implements all the operations on it.
class GpuMemoryBufferStrategy : public BufferStrategy {
 public:
  explicit GpuMemoryBufferStrategy(const gfx::Size& frame_size)
      : client_si_(CreateSharedImage(frame_size)) {
    CHECK(client_si_);
  }

  // BufferStrategy:
  media::mojom::VideoBufferHandlePtr GetHandle() const override {
    return media::mojom::VideoBufferHandle::NewGpuMemoryBufferHandle(
        client_si_->CloneGpuMemoryBufferHandle());
  }
  void DrawFrameOnBuffer(const gfx::Size& frame_size) override {
    auto scoped_mapping = client_si_->Map();
    CHECK(scoped_mapping);
    const gfx::Size buffer_size = scoped_mapping->Size();
    uint8_t* data = static_cast<uint8_t*>(scoped_mapping->Memory(0));

    // Clear all the buffer to 0.
    memset(data, 0, scoped_mapping->Stride(0) * buffer_size.height());

    SkBitmap bitmap;
    // Create an `SkImageInfo` with color type `kBGRA_8888_SkColorType` which
    // matches the buffer format `BGRA_8888` of the `gmb_`. This `info` will be
    // used to read the pixels from `bitmap` into the buffer.
    const auto info =
        SkImageInfo::Make(frame_size.width(), frame_size.height(),
                          kBGRA_8888_SkColorType, kPremul_SkAlphaType);
    bitmap.setInfo(info);
    bitmap.setPixels(data);
    DrawFrameOnCanvas(cc::SkiaPaintCanvas(bitmap), frame_size);
  }

 private:
  scoped_refptr<gpu::ClientSharedImage> client_si_;
};

// -----------------------------------------------------------------------------
// SharedMemoryBufferStrategy:

// Defines a concrete implementation of `BufferStrategy` which creates an
// `UnsafeSharedMemoryRegion` and implements all the operations on it.
class SharedMemoryBufferStrategy : public BufferStrategy {
 public:
  explicit SharedMemoryBufferStrategy(const gfx::Size& frame_size)
      : region_(base::UnsafeSharedMemoryRegion::Create(
            media::VideoFrame::AllocationSize(media::PIXEL_FORMAT_ARGB,
                                              frame_size))) {
    DCHECK(region_.IsValid());
  }

  // BufferStrategy:
  media::mojom::VideoBufferHandlePtr GetHandle() const override {
    return media::mojom::VideoBufferHandle::NewUnsafeShmemRegion(
        region_.Duplicate());
  }
  void DrawFrameOnBuffer(const gfx::Size& frame_size) override {
    if (!mapping_.IsValid())
      mapping_ = region_.Map();
    DCHECK(mapping_.IsValid());
    uint8_t* buffer_ptr = mapping_.GetMemoryAsSpan<uint8_t>().data();
    const int buffer_size = mapping_.size();
    memset(buffer_ptr, 0, buffer_size);
    SkBitmap bitmap;
    bitmap.setInfo(
        SkImageInfo::MakeN32Premul(frame_size.width(), frame_size.height()));
    bitmap.setPixels(buffer_ptr);
    DrawFrameOnCanvas(cc::SkiaPaintCanvas(bitmap), frame_size);
  }

 private:
  base::UnsafeSharedMemoryRegion region_;
  base::WritableSharedMemoryMapping mapping_;
};

}  // namespace

// -----------------------------------------------------------------------------
// FakeCameraDevice::Buffer:

class FakeCameraDevice::Buffer {
 public:
  Buffer(const Buffer&) = delete;
  Buffer& operator=(const Buffer&) = delete;
  ~Buffer() = default;

  // Creates an instance of this object with the given `buffer_id`. The actual
  // underlying buffer will depend on the given `buffer_type` and will be big
  // enough to hold the content of a video frame of size `frame_size`.
  static std::unique_ptr<FakeCameraDevice::Buffer> Create(
      int buffer_id,
      media::VideoCaptureBufferType buffer_type,
      const gfx::Size& frame_size) {
    switch (buffer_type) {
      case media::VideoCaptureBufferType::kSharedMemory:
        return base::WrapUnique(new Buffer(
            buffer_id, buffer_type, frame_size,
            std::make_unique<SharedMemoryBufferStrategy>(frame_size)));
      case media::VideoCaptureBufferType::kGpuMemoryBuffer:
        return base::WrapUnique(
            new Buffer(buffer_id, buffer_type, frame_size,
                       std::make_unique<GpuMemoryBufferStrategy>(frame_size)));
      default:
        NOTREACHED();
    }
  }

  int buffer_id() const { return buffer_id_; }
  media::VideoCaptureBufferType buffer_type() const { return buffer_type_; }
  const gfx::Size& frame_size() const { return frame_size_; }
  void set_is_in_use(bool value) { is_in_use_ = value; }

  // Returns true if this buffer is not already in use, and can be reused for
  // the given `buffer_type` and the given `frame_size`.
  bool CanBeReused(media::VideoCaptureBufferType buffer_type,
                   const gfx::Size& frame_size) const {
    return !is_in_use_ && buffer_type_ == buffer_type &&
           frame_size_ == frame_size;
  }

  // Returns a handle for the underlying buffer that can be sent via mojo.
  media::mojom::VideoBufferHandlePtr GetHandle() const {
    return buffer_strategy_->GetHandle();
  }

  // Draw the content of the video frame on the underlying buffer.
  void DrawFrame() {
    // buffer_strategy_->CopyBitmapToBuffer(
    //     FakeCameraDevice::GetProducedFrameAsBitmap(frame_size_));
    buffer_strategy_->DrawFrameOnBuffer(frame_size_);
  }

 private:
  Buffer(int buffer_id,
         media::VideoCaptureBufferType buffer_type,
         const gfx::Size& frame_size,
         std::unique_ptr<BufferStrategy> strategy)
      : buffer_id_(buffer_id),
        buffer_type_(buffer_type),
        frame_size_(frame_size),
        buffer_strategy_(std::move(strategy)) {}

  // The ID of this buffer.
  const int buffer_id_;

  // The type of this buffer. Only `kSharedMemory`, and `kGpuMemoryBuffer` are
  // supported.
  const media::VideoCaptureBufferType buffer_type_;

  // The size of the video frame this buffer can hold.
  const gfx::Size frame_size_;

  // The strategy object that holds the underlying buffer.
  const std::unique_ptr<BufferStrategy> buffer_strategy_;

  // True if this buffer is still in use by the subscriber.
  bool is_in_use_ = false;
};

// -----------------------------------------------------------------------------
// FakeCameraDevice::Subscription:

// A fake implementation of the `PushVideoStreamSubscription` mojo API which
// is created for a remote implementation of `VideoFrameHandler` that subscribes
// to the camera device.
class FakeCameraDevice::Subscription
    : public video_capture::mojom::PushVideoStreamSubscription {
 public:
  Subscription(
      FakeCameraDevice* owner_device,
      mojo::PendingReceiver<video_capture::mojom::PushVideoStreamSubscription>
          subscription,
      mojo::PendingRemote<video_capture::mojom::VideoFrameHandler> subscriber)
      : owner_device_(owner_device),
        receiver_(this, std::move(subscription)),
        subscriber_(std::move(subscriber)) {
    subscriber_.set_disconnect_handler(base::BindOnce(
        &Subscription::OnSubscriberDisconnected, base::Unretained(this)));
  }

  bool is_active() const { return is_active_; }
  bool is_suspended() const { return is_suspended_; }

  void OnNewBuffer(int buffer_id,
                   media::mojom::VideoBufferHandlePtr buffer_handle) {
    DCHECK(is_active_);
    subscriber_->OnNewBuffer(buffer_id, std::move(buffer_handle));
  }

  void OnBufferRetired(int buffer_id) {
    DCHECK(is_active_);
    subscriber_->OnBufferRetired(buffer_id);
  }

  void OnFrameReadyInBuffer(
      video_capture::mojom::ReadyFrameInBufferPtr buffer) {
    DCHECK(is_active_ && !is_suspended_);
    subscriber_->OnFrameReadyInBuffer(std::move(buffer));
  }

  void OnFrameDropped() {
    DCHECK(is_active_ && !is_suspended_);
    subscriber_->OnFrameDropped(
        media::VideoCaptureFrameDropReason::kBufferPoolMaxBufferCountExceeded);
  }

  void TriggerFatalError() {
    subscriber_->OnError(
        media::VideoCaptureError::kCrosHalV3DeviceDelegateMojoConnectionError);
  }

  // video_capture::mojom::PushVideoStreamSubscription:
  void Activate() override {
    is_active_ = true;
    DCHECK(subscriber_);
    subscriber_->OnFrameAccessHandlerReady(
        owner_device_->GetNewAccessHandlerRemote());
    owner_device_->OnSubscriptionActivationChanged(this);
  }
  void Suspend(SuspendCallback callback) override { is_suspended_ = true; }
  void Resume() override { is_suspended_ = false; }
  void GetPhotoState(GetPhotoStateCallback callback) override {}
  void SetPhotoOptions(media::mojom::PhotoSettingsPtr settings,
                       SetPhotoOptionsCallback callback) override {}
  void TakePhoto(TakePhotoCallback callback) override {}
  void Close(CloseCallback callback) override {
    is_active_ = false;
    owner_device_->OnSubscriptionActivationChanged(this);
  }
  void ProcessFeedback(const media::VideoCaptureFeedback& feedback) override {}

 private:
  void OnSubscriberDisconnected() {
    owner_device_->OnSubscriberDisconnected(this);
    // `this` is deleted at this point.
  }

  // The camera device which owns this object.
  const raw_ptr<FakeCameraDevice> owner_device_;

  mojo::Receiver<video_capture::mojom::PushVideoStreamSubscription> receiver_{
      this};

  // The remote subscriber which implements the `VideoFrameHandler` mojo API.
  mojo::Remote<video_capture::mojom::VideoFrameHandler> subscriber_;

  // True when this subscription is active. Active subscriptions always produce
  // `OnNewBuffer()` and `OnBufferRetired()` calls regardless of the state of
  // `is_suspended_`.
  bool is_active_ = false;

  // True if this subscription is suspended. Suspended subscriptions don't
  // produce `OnFrameReadyInBuffer()` nor `OnFrameDropped()` calls.
  bool is_suspended_ = false;
};

// -----------------------------------------------------------------------------
// FakeCameraDevice:

FakeCameraDevice::FakeCameraDevice(
    const media::VideoCaptureDeviceInfo& device_info)
    : device_info_(device_info) {}

FakeCameraDevice::~FakeCameraDevice() = default;

// static
SkBitmap FakeCameraDevice::GetProducedFrameAsBitmap(
    const gfx::Size& frame_size) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(frame_size.width(), frame_size.height());
  DrawFrameOnCanvas(cc::SkiaPaintCanvas(bitmap), frame_size);
  return bitmap;
}

void FakeCameraDevice::Bind(
    mojo::PendingReceiver<video_capture::mojom::VideoSource> pending_receiver) {
  video_source_receiver_set_.Add(this, std::move(pending_receiver));
}

void FakeCameraDevice::TriggerFatalError() {
  for (auto& pair : subscriptions_map_) {
    auto* subscription = pair.first;
    subscription->TriggerFatalError();
  }
}

void FakeCameraDevice::CreatePushSubscription(
    mojo::PendingRemote<video_capture::mojom::VideoFrameHandler> subscriber,
    const media::VideoCaptureParams& requested_settings,
    bool force_reopen_with_new_settings,
    mojo::PendingReceiver<video_capture::mojom::PushVideoStreamSubscription>
        subscription,
    CreatePushSubscriptionCallback callback) {
  auto new_subscription = std::make_unique<FakeCameraDevice::Subscription>(
      this, std::move(subscription), std::move(subscriber));
  auto* new_subscription_ptr = new_subscription.get();
  subscriptions_map_.emplace(new_subscription_ptr, std::move(new_subscription));

  DCHECK(requested_settings.IsValid());
  const bool has_current_settings = current_settings_.has_value();
  if (force_reopen_with_new_settings || !has_current_settings) {
    RetireAllBuffers();
    current_settings_.emplace(requested_settings);
    OnSubscriptionActivationChanged(/*subscription=*/nullptr);
  }

  DCHECK(callback);
  std::move(callback).Run(
      video_capture::mojom::CreatePushSubscriptionResultCode::NewSuccessCode(
          video_capture::mojom::CreatePushSubscriptionSuccessCode::
              kCreatedWithRequestedSettings),
      requested_settings);
}

void FakeCameraDevice::RegisterVideoEffectsProcessor(
    mojo::PendingRemote<video_effects::mojom::VideoEffectsProcessor> remote) {}

void FakeCameraDevice::OnFinishedConsumingBuffer(int32_t buffer_id) {
  auto iter = buffer_pool_.find(buffer_id);
  if (iter != buffer_pool_.end())
    iter->second->set_is_in_use(false);
}

mojo::PendingRemote<video_capture::mojom::VideoFrameAccessHandler>
FakeCameraDevice::GetNewAccessHandlerRemote() {
  mojo::PendingReceiver<video_capture::mojom::VideoFrameAccessHandler> receiver;
  auto remote = receiver.InitWithNewPipeAndPassRemote();
  access_handler_receiver_set_.Add(this, std::move(receiver));
  return remote;
}

void FakeCameraDevice::OnSubscriberDisconnected(Subscription* subscription) {
  DCHECK(subscriptions_map_.contains(subscription));

  subscriptions_map_.erase(subscription);

  if (subscriptions_map_.empty()) {
    frame_timer_.Stop();
    current_settings_.reset();
    RetireAllBuffers();
  }
}

void FakeCameraDevice::OnSubscriptionActivationChanged(
    Subscription* subscription) {
  DCHECK(current_settings_.has_value());

  // Newly activated subscriptions should be told about all existing buffers.
  if (subscription && subscription->is_active()) {
    for (auto& pair : buffer_pool_) {
      Buffer* buffer = pair.second.get();
      subscription->OnNewBuffer(buffer->buffer_id(), buffer->GetHandle());
    }
  }

  const auto frame_duration =
      base::Hertz(current_settings_->requested_format.frame_rate);
  if (IsAnySubscriptionActive()) {
    if (!frame_timer_.IsRunning() ||
        frame_timer_.GetCurrentDelay() != frame_duration) {
      frame_timer_.Start(FROM_HERE, frame_duration, this,
                         &FakeCameraDevice::OnNextFrame);
    }
  } else {
    frame_timer_.Stop();
  }
}

bool FakeCameraDevice::IsAnySubscriptionActive() const {
  for (const auto& pair : subscriptions_map_) {
    if (pair.first->is_active())
      return true;
  }
  return false;
}

void FakeCameraDevice::OnNextFrame() {
  DCHECK(IsAnySubscriptionActive());
  DCHECK(current_settings_.has_value());

  if (start_time_.is_null())
    start_time_ = base::Time::Now();

  auto* buffer = AllocateOrReuseBuffer();
  if (!buffer)
    return;

  buffer->DrawFrame();

  for (auto& pair : subscriptions_map_) {
    auto* subscription = pair.first;
    if (!subscription->is_active() || subscription->is_suspended())
      return;

    media::mojom::VideoFrameInfoPtr info = media::mojom::VideoFrameInfo::New();
    info->timestamp = base::Time::Now() - start_time_;
    info->pixel_format = media::PIXEL_FORMAT_ARGB;
    info->coded_size = current_settings_->requested_format.frame_size;
    info->visible_rect = gfx::Rect(info->coded_size);
    info->is_premapped = false;

    subscription->OnFrameReadyInBuffer(
        video_capture::mojom::ReadyFrameInBuffer::New(buffer->buffer_id(),
                                                      /*frame_feedback_id=*/0,
                                                      std::move(info)));
  }
}

FakeCameraDevice::Buffer* FakeCameraDevice::AllocateOrReuseBuffer() {
  DCHECK(current_settings_.has_value());

  const auto buffer_type = current_settings_->buffer_type;
  const auto frame_size = current_settings_->requested_format.frame_size;

  // Can we reuse an existing buffer?
  for (auto& pair : buffer_pool_) {
    Buffer* buffer = pair.second.get();
    if (buffer->CanBeReused(buffer_type, frame_size)) {
      buffer->set_is_in_use(true);
      return buffer;
    }
  }

  if (buffer_pool_.size() >= FakeCameraDevice::kMaxBufferCount) {
    for (auto& pair : subscriptions_map_) {
      auto* subscription = pair.first;
      if (subscription->is_active() && !subscription->is_suspended())
        subscription->OnFrameDropped();
    }
    return nullptr;
  }

  const int buffer_id = g_next_buffer_id++;
  auto unique_buffer = Buffer::Create(buffer_id, buffer_type, frame_size);
  Buffer* buffer_ptr = unique_buffer.get();
  buffer_ptr->set_is_in_use(true);
  buffer_pool_.emplace(buffer_id, std::move(unique_buffer));

  for (auto& pair : subscriptions_map_) {
    auto* subscription = pair.first;
    if (subscription->is_active())
      subscription->OnNewBuffer(buffer_id, buffer_ptr->GetHandle());
  }

  return buffer_ptr;
}

void FakeCameraDevice::RetireAllBuffers() {
  for (auto& pair : buffer_pool_) {
    const int buffer_id = pair.first;
    for (auto& subscription_pair : subscriptions_map_) {
      auto* subscription = subscription_pair.first;
      if (subscription->is_active())
        subscription->OnBufferRetired(buffer_id);
    }
  }
  buffer_pool_.clear();
}

}  // namespace ash
