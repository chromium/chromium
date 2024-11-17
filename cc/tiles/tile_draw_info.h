// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TILES_TILE_DRAW_INFO_H_
#define CC_TILES_TILE_DRAW_INFO_H_

#include "base/notreached.h"
#include "base/trace_event/traced_value.h"
#include "cc/resources/resource_pool.h"
#include "components/viz/common/resources/platform_color.h"
#include "third_party/skia/include/core/SkColor.h"

namespace cc {

// This class holds all the state relevant to drawing a tile.
class CC_EXPORT TileDrawInfo {
 public:
  enum Mode { RESOURCE_MODE, SOLID_COLOR_MODE, OOM_MODE };

  TileDrawInfo();
  ~TileDrawInfo();

  Mode mode() const { return mode_; }

  bool IsReadyToDraw() const {
    switch (mode_) {
      case RESOURCE_MODE:
        return is_resource_ready_to_draw_;
      case SOLID_COLOR_MODE:
      case OOM_MODE:
        return true;
    }
    NOTREACHED();
  }
  bool NeedsRaster() const {
    switch (mode_) {
      case RESOURCE_MODE:
        return !resource_;
      case SOLID_COLOR_MODE:
        return false;
      case OOM_MODE:
        return true;
    }
    NOTREACHED();
  }

  viz::ResourceId resource_id_for_export() const {
    DCHECK(mode_ == RESOURCE_MODE);
    DCHECK(resource_);
    return resource_.resource_id_for_export();
  }

  const gfx::Size& resource_size() const {
    DCHECK(mode_ == RESOURCE_MODE);
    DCHECK(resource_);
    return resource_.size();
  }

  const viz::SharedImageFormat& resource_shared_image_format() const {
    DCHECK(mode_ == RESOURCE_MODE);
    DCHECK(resource_);
    return resource_.format();
  }

  SkColor4f solid_color() const {
    DCHECK(mode_ == SOLID_COLOR_MODE);
    return solid_color_;
  }

  bool is_premultiplied() const { return is_premultiplied_; }

  bool requires_resource() const {
    return mode_ == RESOURCE_MODE || mode_ == OOM_MODE;
  }

  inline bool has_resource() const { return !!resource_; }
  bool is_resource_ready_to_draw() const { return is_resource_ready_to_draw_; }

  const ResourcePool::InUsePoolResource& GetResource() const;

  bool is_checker_imaged() const {
    DCHECK(!resource_is_checker_imaged_ || resource_);
    return resource_is_checker_imaged_;
  }

  void SetSolidColorForTesting(SkColor4f color) { set_solid_color(color); }

  void AsValueInto(base::trace_event::TracedValue* state) const;

 private:
  friend class Tile;
  friend class TileManager;

  void SetResource(ResourcePool::InUsePoolResource resource,
                   bool resource_is_checker_imaged,
                   bool is_premultiplied);
  ResourcePool::InUsePoolResource TakeResource();

  void set_resource_ready_for_draw() {
    is_resource_ready_to_draw_ = true;
  }

  void set_solid_color(const SkColor4f& color) {
    DCHECK(!resource_);
    mode_ = SOLID_COLOR_MODE;
    solid_color_ = color;
  }

  void set_oom() { mode_ = OOM_MODE; }

  Mode mode_ = RESOURCE_MODE;
  SkColor4f solid_color_ = SkColors::kWhite;
  ResourcePool::InUsePoolResource resource_;
  bool is_premultiplied_ = false;
  bool is_resource_ready_to_draw_ = false;

  // Set to true if |resource_| was rasterized with checker-imaged content. The
  // flag can only be true iff we have a valid |resource_|.
  bool resource_is_checker_imaged_ = false;
};

}  // namespace cc

#endif  // CC_TILES_TILE_DRAW_INFO_H_
