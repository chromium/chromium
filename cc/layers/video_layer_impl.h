// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYERS_VIDEO_LAYER_IMPL_H_
#define CC_LAYERS_VIDEO_LAYER_IMPL_H_

#include <vector>

#include "cc/cc_export.h"
#include "cc/layers/layer_impl.h"
#include "components/viz/common/resources/release_callback.h"
#include "media/base/video_transformation.h"

namespace media {
class VideoFrame;
class VideoResourceUpdater;
}

namespace cc {
class VideoFrameProvider;
class VideoFrameProviderClientImpl;

class CC_EXPORT VideoLayerImpl : public LayerImpl {
 public:
  // Must be called on the impl thread while the main thread is blocked. This is
  // so that |provider| stays alive while this is being created.
  static std::unique_ptr<VideoLayerImpl> Create(
      LayerTreeImpl* tree_impl,
      int id,
      VideoFrameProvider* provider,
      media::VideoRotation video_rotation);
  VideoLayerImpl(const VideoLayerImpl&) = delete;
  ~VideoLayerImpl() override;

  VideoLayerImpl& operator=(const VideoLayerImpl&) = delete;

  // LayerImpl implementation.
  std::unique_ptr<LayerImpl> CreateLayerImpl(LayerTreeImpl* tree_impl) override;
  bool WillDraw(DrawMode draw_mode,
                viz::ClientResourceProvider* resource_provider) override;
  void AppendQuads(viz::RenderPass* render_pass,
                   AppendQuadsData* append_quads_data) override;
  void DidDraw(viz::ClientResourceProvider* resource_provider) override;
  SimpleEnclosedRegion VisibleOpaqueRegion() const override;
  void DidBecomeActive() override;
  void ReleaseResources() override;

  void SetNeedsRedraw();
  media::VideoRotation video_rotation() const { return video_rotation_; }

 private:
  VideoLayerImpl(
      LayerTreeImpl* tree_impl,
      int id,
      scoped_refptr<VideoFrameProviderClientImpl> provider_client_impl,
      media::VideoRotation video_rotation);

  const char* LayerTypeAsString() const override;

  scoped_refptr<VideoFrameProviderClientImpl> provider_client_impl_;

  scoped_refptr<media::VideoFrame> frame_;

  media::VideoRotation video_rotation_;

  std::unique_ptr<media::VideoResourceUpdater> updater_;
};

}  // namespace cc

#endif  // CC_LAYERS_VIDEO_LAYER_IMPL_H_
