// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_SLIM_TEST_FRAME_SINK_IMPL_H_
#define CC_SLIM_TEST_FRAME_SINK_IMPL_H_

#include <memory>

#include "cc/slim/frame_sink_impl.h"

namespace cc::slim {

class TestFrameSinkImpl : public FrameSinkImpl {
 public:
  static std::unique_ptr<TestFrameSinkImpl> Create();

  ~TestFrameSinkImpl() override;

  base::WeakPtr<TestFrameSinkImpl> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }
  void SetBindToClientResult(bool result) {
    DCHECK(!bind_to_client_called_);
    bind_to_client_result_ = result;
  }
  bool GetDidSubmitAndReset();
  bool GetDidNotProduceFrameAndReset();
  viz::CompositorFrame TakeLastFrame();
  const std::optional<::viz::HitTestRegionList>& GetLastHitTestRegionList()
      const;
  bool bind_to_client_called() const { return bind_to_client_called_; }
  bool needs_begin_frames() const { return needs_begin_frames_; }

  // FrameSinkImpl overrides.
  bool BindToClient(FrameSinkImplClient* client) override;
  void SetNeedsBeginFrame(bool needs_begin_frame) override;

  using FrameSinkImpl::UploadedResourceMap;
  using FrameSinkImpl::UploadedUIResource;
  const UploadedResourceMap& uploaded_resources() const {
    return uploaded_resources_;
  }
  const viz::LocalSurfaceId& GetCurrentLocalSurfaceId() const {
    return local_surface_id_;
  }

 private:
  class TestMojoCompositorFrameSink;
  TestFrameSinkImpl(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      mojo::PendingAssociatedRemote<viz::mojom::CompositorFrameSink>
          compositor_frame_sink_associated_remote,
      mojo::PendingReceiver<viz::mojom::CompositorFrameSinkClient>
          client_receiver,
      scoped_refptr<viz::RasterContextProvider> context_provider,
      mojo::PendingAssociatedReceiver<viz::mojom::CompositorFrameSink>
          sink_receiver);

  std::unique_ptr<TestMojoCompositorFrameSink> mojo_sink_;

  bool bind_to_client_called_ = false;
  bool bind_to_client_result_ = true;
  bool needs_begin_frames_ = false;

  base::WeakPtrFactory<TestFrameSinkImpl> weak_factory_{this};
};

}  // namespace cc::slim

#endif  // CC_SLIM_TEST_FRAME_SINK_IMPL_H_
