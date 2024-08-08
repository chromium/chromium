// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYERS_VIDEO_FRAME_PROVIDER_CLIENT_IMPL_H_
#define CC_LAYERS_VIDEO_FRAME_PROVIDER_CLIENT_IMPL_H_

#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "cc/cc_export.h"
#include "cc/layers/video_frame_provider.h"
#include "cc/scheduler/video_frame_controller.h"
#include "ui/gfx/geometry/transform.h"

namespace media { class VideoFrame; }

namespace cc {
class VideoLayerImpl;

// VideoFrameProviderClientImpl liaisons with the VideoFrameProvider and the
// VideoLayer. It receives updates from the provider and updates the layer as a
// result. It also allows the layer to access the video frame that the provider
// has.
class CC_EXPORT VideoFrameProviderClientImpl
    : public VideoFrameProvider::Client,
      public VideoFrameController,
      public base::RefCounted<VideoFrameProviderClientImpl> {
 public:
  // Must be created on the impl thread while the main thread is blocked.
  static scoped_refptr<VideoFrameProviderClientImpl> Create(
      VideoFrameProvider* provider,
      VideoFrameControllerClient* client);

  VideoFrameProviderClientImpl(const VideoFrameProviderClientImpl&) = delete;
  VideoFrameProviderClientImpl& operator=(const VideoFrameProviderClientImpl&) =
      delete;

  VideoLayerImpl* ActiveVideoLayer() const;
  void SetActiveVideoLayer(VideoLayerImpl* video_layer);

  bool Stopped() const;
  // Must be called on the impl thread while the main thread is blocked.
  void Stop();

  scoped_refptr<media::VideoFrame> AcquireLockAndCurrentFrame()
      EXCLUSIVE_LOCK_FUNCTION(provider_lock_);
  void PutCurrentFrame() EXCLUSIVE_LOCKS_REQUIRED(provider_lock_);
  void ReleaseLock() UNLOCK_FUNCTION(provider_lock_);
  void AssertLocked() const ASSERT_EXCLUSIVE_LOCK(provider_lock_) {
    provider_lock_.AssertAcquired();
  }
  bool HasCurrentFrame();
  std::optional<base::TimeDelta> GetPreferredRenderInterval();

  // VideoFrameController implementation.
  void OnBeginFrame(const viz::BeginFrameArgs& args) override;
  void DidDrawFrame() override;

  // VideoFrameProvider::Client implementation.
  // Called on the main thread.
  void StopUsingProvider() override;
  // Called on the impl thread.
  void StartRendering() override;
  void StopRendering() override;
  void DidReceiveFrame() override;
  bool IsDrivingFrameUpdates() const override;

  const VideoFrameProvider* get_provider_for_testing() const {
    return provider_;
  }

 private:
  friend class base::RefCounted<VideoFrameProviderClientImpl>;

  VideoFrameProviderClientImpl(VideoFrameProvider* provider,
                               VideoFrameControllerClient* client);
  ~VideoFrameProviderClientImpl() override;

  raw_ptr<VideoFrameProvider> provider_;
  raw_ptr<VideoFrameControllerClient> client_;
  raw_ptr<VideoLayerImpl> active_video_layer_;
  bool stopped_;
  bool rendering_;
  bool needs_put_current_frame_;

  // Since the provider lives on another thread, it can be destroyed while the
  // frame controller are accessing its frame. Before being destroyed the
  // provider calls StopUsingProvider. provider_lock_ blocks StopUsingProvider
  // from returning until the frame controller is done using the frame.
  base::Lock provider_lock_;
  base::ThreadChecker thread_checker_;
};

}  // namespace cc

#endif  // CC_LAYERS_VIDEO_FRAME_PROVIDER_CLIENT_IMPL_H_
