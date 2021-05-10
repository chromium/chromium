// Copyright 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYERS_VIDEO_LAYER_H_
#define CC_LAYERS_VIDEO_LAYER_H_

#include "base/callback.h"
#include "cc/cc_export.h"
#include "cc/layers/layer.h"
#include "media/base/video_transformation.h"

namespace media { class VideoFrame; }

namespace cc {

class VideoFrameProvider;
class VideoLayerImpl;

// A Layer that contains a Video element.
class CC_EXPORT VideoLayer : public Layer {
 public:
  static scoped_refptr<VideoLayer> Create(VideoFrameProvider* provider,
                                          media::VideoRotation video_rotation);

  VideoLayer(const VideoLayer&) = delete;
  VideoLayer& operator=(const VideoLayer&) = delete;

  std::unique_ptr<LayerImpl> CreateLayerImpl(LayerTreeImpl* tree_impl) override;

  bool Update() override;

  // Clears |provider_| to ensure it is not used after destruction.
  void StopUsingProvider();

 private:
  VideoLayer(VideoFrameProvider* provider, media::VideoRotation video_rotation);
  ~VideoLayer() override;

  // This pointer is only for passing to VideoLayerImpl's constructor. It should
  // never be dereferenced by this class.
  VideoFrameProvider* provider_;

  media::VideoRotation video_rotation_;
};

}  // namespace cc

#endif  // CC_LAYERS_VIDEO_LAYER_H_
