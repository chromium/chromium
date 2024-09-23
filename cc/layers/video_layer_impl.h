// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYERS_VIDEO_LAYER_IMPL_H_
#define CC_LAYERS_VIDEO_LAYER_IMPL_H_

#include <memory>
#include <optional>
#include <vector>

#include "base/time/time.h"
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
      const media::VideoTransformation& video_transform);
  VideoLayerImpl(const VideoLayerImpl&) = delete;
  ~VideoLayerImpl() override;

  VideoLayerImpl& operator=(const VideoLayerImpl&) = delete;

  // LayerImpl implementation.
  mojom::LayerType GetLayerType() const override;
  std::unique_ptr<LayerImpl> CreateLayerImpl(
      LayerTreeImpl* tree_impl) const override;
  bool WillDraw(DrawMode draw_mode,
                viz::ClientResourceProvider* resource_provider) override;
  void AppendQuads(viz::CompositorRenderPass* render_pass,
                   AppendQuadsData* append_quads_data) override;
  void DidDraw(viz::ClientResourceProvider* resource_provider) override;
  SimpleEnclosedRegion VisibleOpaqueRegion() const override;
  void DidBecomeActive() override;
  void ReleaseResources() override;
  gfx::ContentColorUsage GetContentColorUsage() const override;
  DamageReasonSet GetDamageReasons() const override;

  void SetNeedsRedraw();
  std::optional<base::TimeDelta> GetPreferredRenderInterval();

  media::VideoTransformation video_transform_for_testing() const {
    return video_transform_;
  }

 private:
  VideoLayerImpl(
      LayerTreeImpl* tree_impl,
      int id,
      scoped_refptr<VideoFrameProviderClientImpl> provider_client_impl,
      const media::VideoTransformation& video_transform);

  scoped_refptr<VideoFrameProviderClientImpl> provider_client_impl_;

  scoped_refptr<media::VideoFrame> frame_;

  media::VideoTransformation video_transform_;

  std::unique_ptr<media::VideoResourceUpdater> updater_;
};

}  // namespace cc

#endif  // CC_LAYERS_VIDEO_LAYER_IMPL_H_
