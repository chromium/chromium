// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_COMPOSITOR_LAYER_THUMBNAIL_LAYER_H_
#define CHROME_BROWSER_ANDROID_COMPOSITOR_LAYER_THUMBNAIL_LAYER_H_

#include <stddef.h>

#include "base/memory/ref_counted.h"
#include "cc/resources/ui_resource_client.h"
#include "cc/slim/ui_resource_layer.h"
#include "chrome/browser/android/compositor/layer/layer.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size_f.h"

namespace thumbnail {
class Thumbnail;
}  // namespace thumbnail

namespace cc::slim {
class Layer;
}

namespace android {

// A layer to render a thumbnail.
class ThumbnailLayer : public Layer {
 public:
  // Creates a ThumbnailLayer.
  static scoped_refptr<ThumbnailLayer> Create();

  ThumbnailLayer(const ThumbnailLayer&) = delete;
  ThumbnailLayer& operator=(const ThumbnailLayer&) = delete;

  // Sets thumbnail that will be shown. |thumbnail| should not be nullptr.
  void SetThumbnail(thumbnail::Thumbnail* thumbnail);
  // Clip the thumbnail to the given |clipping|.
  void Clip(const gfx::Rect& clipping);
  void ClearClip();
  // Add self to |parent| or replace self at |index| if there already is an
  // instance with the same ID at |index|.
  void AddSelfToParentOrReplaceAt(scoped_refptr<cc::slim::Layer> parent,
                                  size_t index);

  // Implements Layer.
  scoped_refptr<cc::slim::Layer> layer() override;

 protected:
  ThumbnailLayer();
  ~ThumbnailLayer() override;

 private:
  void UpdateSizes(const gfx::SizeF& content_size,
                   const gfx::SizeF& resource_size);

  scoped_refptr<cc::slim::UIResourceLayer> layer_;
  gfx::SizeF content_size_;
  gfx::Rect last_clipping_;
  bool clipped_ = false;
  gfx::SizeF resource_size_;
};

}  // namespace android

#endif  // CHROME_BROWSER_ANDROID_COMPOSITOR_LAYER_THUMBNAIL_LAYER_H_
