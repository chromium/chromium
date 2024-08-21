// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_ARC_VIDEO_ACCELERATOR_GPU_ARC_VIDEO_DECODER_H_
#define ASH_COMPONENTS_ARC_VIDEO_ACCELERATOR_GPU_ARC_VIDEO_DECODER_H_

#include <map>
#include <memory>
#include <optional>
#include <queue>

#include "ash/components/arc/mojom/video_decoder.mojom.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "media/base/decoder_buffer.h"
#include "media/base/video_decoder.h"
#include "media/gpu/chromeos/vda_video_frame_pool.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace arc {

class GpuArcVideoFramePool;
class ProtectedBufferManager;

// The GpuArcVideoDecoder listens to mojo IPC requests and forwards these to an
// instance of media::VideoDecoder. Decoded frames are passed back to the mojo
// client.
class GpuArcVideoDecoder : public mojom::VideoDecoder {
 public:
  GpuArcVideoDecoder(
      scoped_refptr<ProtectedBufferManager> protected_buffer_manager);
  ~GpuArcVideoDecoder() override;

  GpuArcVideoDecoder(const GpuArcVideoDecoder&) = delete;
  GpuArcVideoDecoder& operator=(const GpuArcVideoDecoder&) = delete;
  GpuArcVideoDecoder(GpuArcVideoDecoder&&) = delete;
  GpuArcVideoDecoder& operator=(GpuArcVideoDecoder&&) = delete;

  // Implementation of mojom::VideoDecoder.
  void Initialize(
      arc::mojom::VideoDecoderConfigPtr config,
      mojo::PendingRemote<mojom::VideoDecoderClient> client,
      mojo::PendingAssociatedReceiver<mojom::VideoFramePool> video_frame_pool,
      InitializeCallback callback) override;
  void Decode(arc::mojom::DecoderBufferPtr buffer,
              DecodeCallback callback) override;
  void Reset(ResetCallback callback) override;
  void ReleaseVideoFrame(int32_t video_frame_id) override;

 private:
  using Request = base::OnceClosure;

  // Called by the decoder when initialization is done.
  void OnInitializeDone(media::DecoderStatus status);
  // Called by the decoder when the specified buffer has been decoded.
  void OnDecodeDone(DecodeCallback callback, media::DecoderStatus status);
  // Called by the decoder when a decoded frame is ready.
  void OnFrameReady(scoped_refptr<media::VideoFrame> frame);
  // Called by the decoder when a reset has been completed.
  void OnResetDone();
  // Called by the video frame pool to notify us that the pool won't be tracking
  // the current set of the video frames so we should release references to
  // them. This can indicate that new frames will be soon added to the pool
  // using the same IDs.
  void ReleaseClientVideoFrames();
  // Called when an error occurred.
  void OnError(media::DecoderStatus status);

  // Handle all requests that are currently in the |requests_| queue.
  void HandleRequests();
  // Handle the specified request. If the decoder is currently resetting the
  // request will be queued and handled once OnResetDone() is called.
  void HandleRequest(Request request);
  // Handle a decode request of the specified |buffer|.
  void HandleDecodeRequest(scoped_refptr<media::DecoderBuffer> buffer,
                           DecodeCallback callback);
  // Handle a reset request with specified |callback|. All ongoing flush
  // operations will be reported as canceled.
  void HandleResetRequest(ResetCallback callback);

  // Create a decoder buffer from the specified |fd|.
  scoped_refptr<media::DecoderBuffer> CreateDecoderBuffer(base::ScopedFD fd,
                                                          uint32_t offset,
                                                          uint32_t bytes_used);

  // The number of currently active instances. Always accessed on the same
  // thread, so we don't need to use a lock.
  static size_t num_instances_;

  // Whether the video decoder encountered an error and is aborting.
  bool error_state_ = false;

  // The remote mojo client.
  mojo::Remote<mojom::VideoDecoderClient> client_;
  // The local video decoder.
  std::unique_ptr<media::VideoDecoder> decoder_;
  // The video frame pool service.
  std::unique_ptr<GpuArcVideoFramePool> video_frame_pool_;

  // Initialization callback, used while the decoder is being initialized.
  InitializeCallback init_callback_;
  // The callback associated with the ongoing reset request if any.
  ResetCallback reset_callback_;
  // Requests currently waiting until resetting the decoder has completed.
  std::queue<Request> requests_;

  // The video frames currently in use by the client and their associated
  // video frame ids. We need to hold references to video frames to prevent
  // them from being returned to the video frame pool for reuse while they are
  // still in use by our client. The use count is tracked as the same frame
  // might be sent multiple times to the client when using the VP9
  // 'show_existing_frame' feature.
  std::map<int32_t, std::pair<scoped_refptr<media::VideoFrame>, size_t>>
      client_video_frames_;

  // The protected buffer manager, used when decoding an encrypted video.
  scoped_refptr<ProtectedBufferManager> protected_buffer_manager_;
  // Whether we're decoding an encrypted video.
  std::optional<bool> secure_mode_;

  // The client task runner and its sequence checker. All methods should be run
  // on this task runner.
  scoped_refptr<base::SequencedTaskRunner> client_task_runner_;
  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtr<GpuArcVideoDecoder> weak_this_;
  base::WeakPtrFactory<GpuArcVideoDecoder> weak_this_factory_{this};
};

}  // namespace arc

#endif  // ASH_COMPONENTS_ARC_VIDEO_ACCELERATOR_GPU_ARC_VIDEO_DECODER_H_
