// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREES_DAMAGE_TRACKER_H_
#define CC_TREES_DAMAGE_TRACKER_H_

#include <memory>
#include <vector>

#include "cc/cc_export.h"
#include "cc/layers/layer_collections.h"
#include "ui/gfx/geometry/rect.h"

namespace gfx {
class Rect;
}

namespace cc {

class FilterOperations;
class LayerImpl;
class LayerTreeImpl;
class RenderSurfaceImpl;

// Computes the region where pixels have actually changed on a
// RenderSurfaceImpl. This region is used to scissor what is actually drawn to
// the screen to save GPU computation and bandwidth.
class CC_EXPORT DamageTracker {
 public:
  static std::unique_ptr<DamageTracker> Create();
  DamageTracker(const DamageTracker&) = delete;
  ~DamageTracker();

  DamageTracker& operator=(const DamageTracker&) = delete;

  static void UpdateDamageTracking(LayerTreeImpl* layer_tree_impl);

  void DidDrawDamagedArea() {
    current_damage_ = DamageAccumulator();
    has_damage_from_contributing_content_ = false;
  }
  void AddDamageNextUpdate(const gfx::Rect& dmg) { current_damage_.Union(dmg); }

  bool GetDamageRectIfValid(gfx::Rect* rect);

  bool has_damage_from_contributing_content() const {
    return has_damage_from_contributing_content_;
  }

 private:
  DamageTracker();

  class DamageAccumulator {
   public:
    template <typename Type>
    void Union(const Type& rect) {
      if (!is_valid_rect_)
        return;
      if (rect.IsEmpty())
        return;
      if (IsEmpty()) {
        x_ = rect.x();
        y_ = rect.y();
        right_ = rect.right();
        bottom_ = rect.bottom();
        return;
      }

      x_ = std::min(x_, rect.x());
      y_ = std::min(y_, rect.y());
      right_ = std::max(right_, rect.right());
      bottom_ = std::max(bottom_, rect.bottom());
    }

    int x() const { return x_; }
    int y() const { return y_; }
    int right() const { return right_; }
    int bottom() const { return bottom_; }
    bool IsEmpty() const { return x_ == right_ || y_ == bottom_; }

    bool GetAsRect(gfx::Rect* rect);

   private:
    bool is_valid_rect_ = true;
    int x_ = 0;
    int y_ = 0;
    int right_ = 0;
    int bottom_ = 0;
  };

  DamageAccumulator TrackDamageFromLeftoverRects();

  // These helper functions are used only during UpdateDamageTracking().
  void PrepareForUpdate();
  void AccumulateDamageFromLayer(LayerImpl* layer);
  void AccumulateDamageFromRenderSurface(RenderSurfaceImpl* render_surface);
  void ComputeSurfaceDamage(RenderSurfaceImpl* render_surface);
  void ExpandDamageInsideRectWithFilters(const gfx::Rect& pre_filter_rect,
                                         const FilterOperations& filters);

  struct LayerRectMapData {
    LayerRectMapData() : layer_id_(0), mailboxId_(0) {}
    explicit LayerRectMapData(int layer_id)
        : layer_id_(layer_id), mailboxId_(0) {}
    void Update(const gfx::Rect& rect, unsigned int mailboxId) {
      mailboxId_ = mailboxId;
      rect_ = rect;
    }

    bool operator<(const LayerRectMapData& other) const {
      return layer_id_ < other.layer_id_;
    }

    int layer_id_;
    unsigned int mailboxId_;
    gfx::Rect rect_;
  };

  struct SurfaceRectMapData {
    SurfaceRectMapData() : surface_id_(0), mailboxId_(0) {}
    explicit SurfaceRectMapData(uint64_t surface_id)
        : surface_id_(surface_id), mailboxId_(0) {}
    void Update(const gfx::Rect& rect, unsigned int mailboxId) {
      mailboxId_ = mailboxId;
      rect_ = rect;
    }

    bool operator<(const SurfaceRectMapData& other) const {
      return surface_id_ < other.surface_id_;
    }

    uint64_t surface_id_;
    unsigned int mailboxId_;
    gfx::Rect rect_;
  };
  typedef std::vector<LayerRectMapData> SortedRectMapForLayers;
  typedef std::vector<SurfaceRectMapData> SortedRectMapForSurfaces;

  LayerRectMapData& RectDataForLayer(int layer_id, bool* layer_is_new);
  SurfaceRectMapData& RectDataForSurface(uint64_t surface_id,
                                         bool* layer_is_new);

  SortedRectMapForLayers rect_history_for_layers_;
  SortedRectMapForSurfaces rect_history_for_surfaces_;

  unsigned int mailboxId_ = 0;
  DamageAccumulator current_damage_;
  // Damage from contributing render surface and layer
  bool has_damage_from_contributing_content_ = false;

  // Damage accumulated since the last call to PrepareForUpdate().
  DamageAccumulator damage_for_this_update_;
};

}  // namespace cc

#endif  // CC_TREES_DAMAGE_TRACKER_H_
