// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREES_DAMAGE_TRACKER_H_
#define CC_TREES_DAMAGE_TRACKER_H_

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "cc/cc_export.h"
#include "cc/layers/layer_collections.h"
#include "cc/layers/view_transition_content_layer_impl.h"
#include "cc/paint/element_id.h"
#include "cc/trees/damage_reason.h"
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
  void AddDamageNextUpdate(const gfx::Rect& dmg) {
    current_damage_.Union(dmg, {DamageReason::kUntracked});
  }

  bool GetDamageRectIfValid(gfx::Rect* rect);
  DamageReasonSet GetDamageReasons();

  bool has_damage_from_contributing_content() const {
    return has_damage_from_contributing_content_;
  }

 private:
  using ViewTransitionElementResourceIdToRenderSurfaceMap =
      base::flat_map<viz::ViewTransitionElementResourceId, RenderSurfaceImpl*>;

  DamageTracker();

  class DamageAccumulator {
   public:
    template <typename Type>
    void Union(const Type& rect, DamageReasonSet reasons) {
      if (rect.IsEmpty())
        return;

      // Can skip updating reasons only if the other rect is empty so this Union
      // is no-op. In particular, cannot skip updating reasons if this is
      // invalid, input rect is invalid, or input rect is a subrect of this.
      reasons_.PutAll(reasons);

      if (!is_valid_rect_) {
        return;
      }

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

    void UnionReasons(DamageReasonSet reasons) { reasons_.PutAll(reasons); }

    int x() const { return x_; }
    int y() const { return y_; }
    int right() const { return right_; }
    int bottom() const { return bottom_; }
    bool IsEmpty() const { return x_ == right_ || y_ == bottom_; }

    bool GetAsRect(gfx::Rect* rect);

    DamageReasonSet reasons() const { return reasons_; }

   private:
    bool is_valid_rect_ = true;
    int x_ = 0;
    int y_ = 0;
    int right_ = 0;
    int bottom_ = 0;

    DamageReasonSet reasons_;
  };

  DamageAccumulator TrackDamageFromLeftoverRects();

  static void InitializeUpdateDamageTracking(
      LayerTreeImpl* layer_tree_impl,
      ViewTransitionElementResourceIdToRenderSurfaceMap&
          id_to_render_surface_map);

  // These helper functions are used only during UpdateDamageTracking().
  void PrepareForUpdate();
  // view_transition_content_surface corresponds to the render surface which
  // produces content drawn by a ViewTransitionContentLayer. Must be provided if
  // layer has a valid view transition resource id.
  void AccumulateDamageFromLayer(
      LayerImpl* layer,
      ViewTransitionElementResourceIdToRenderSurfaceMap&
          id_to_render_surface_map);
  void AccumulateDamageFromRenderSurface(RenderSurfaceImpl* render_surface);
  void ComputeSurfaceDamage(RenderSurfaceImpl* render_surface);
  void ExpandDamageInsideRectWithFilters(const gfx::Rect& pre_filter_rect,
                                         const FilterOperations& filters);

  gfx::Rect GetViewTransitionContentSurfaceDamageInSharedElementLayerSpace(
      LayerImpl* layer,
      ViewTransitionElementResourceIdToRenderSurfaceMap&
          id_to_render_surface_map);

  struct LayerRectMapData {
    LayerRectMapData() = default;
    explicit LayerRectMapData(int layer_id) : layer_id_(layer_id) {}
    void Update(const gfx::Rect& rect, unsigned int mailbox_id) {
      mailbox_id_ = mailbox_id;
      rect_ = rect;
    }

    bool operator<(const LayerRectMapData& other) const {
      return layer_id_ < other.layer_id_;
    }

    int layer_id_ = 0;
    unsigned int mailbox_id_ = 0;
    gfx::Rect rect_;
  };

  struct SurfaceRectMapData {
    SurfaceRectMapData() = default;
    explicit SurfaceRectMapData(ElementId surface_id)
        : surface_id_(surface_id) {}
    void Update(const gfx::Rect& rect, unsigned int mailbox_id) {
      mailbox_id_ = mailbox_id;
      rect_ = rect;
    }

    bool operator<(const SurfaceRectMapData& other) const {
      return surface_id_ < other.surface_id_;
    }

    ElementId surface_id_;
    unsigned int mailbox_id_ = 0;
    gfx::Rect rect_;
  };
  typedef std::vector<LayerRectMapData> SortedRectMapForLayers;
  typedef std::vector<SurfaceRectMapData> SortedRectMapForSurfaces;

  LayerRectMapData& RectDataForLayer(int layer_id, bool* layer_is_new);
  SurfaceRectMapData& RectDataForSurface(ElementId surface_id,
                                         bool* layer_is_new);

  SortedRectMapForLayers rect_history_for_layers_;
  SortedRectMapForSurfaces rect_history_for_surfaces_;

  unsigned int mailbox_id_ = 0;
  DamageAccumulator current_damage_;
  // Damage from contributing render surface and layer
  bool has_damage_from_contributing_content_ = false;

  // Damage accumulated since the last call to PrepareForUpdate().
  DamageAccumulator damage_for_this_update_;

  struct SurfaceWithRect {
    SurfaceWithRect(RenderSurfaceImpl* rs, const gfx::Rect& rect)
        : render_surface(rs), rect_in_target_space(rect) {}
    raw_ptr<RenderSurfaceImpl> render_surface;
    const gfx::Rect rect_in_target_space;
  };

  std::vector<SurfaceWithRect> contributing_surfaces_;

  // Track the view transition content render surfaces.
  // The corresponding content surface of a view transition layer might be
  // omitted. Surface appearing and disappearing should cause full damage on the
  // view transition layer. Tracking previous/current content surfaces to
  // determine surface appearing and disappearing.
  std::vector<viz::ViewTransitionElementResourceId>
      previous_view_transition_content_surfaces_by_id_;
  std::vector<viz::ViewTransitionElementResourceId>
      current_view_transition_content_surfaces_by_id_;
};

}  // namespace cc

#endif  // CC_TREES_DAMAGE_TRACKER_H_
