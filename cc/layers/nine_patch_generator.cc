// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/nine_patch_generator.h"

#include "base/trace_event/traced_value.h"
#include "cc/base/math_util.h"
#include "cc/trees/layer_tree_impl.h"
#include "components/viz/client/client_resource_provider.h"
#include "components/viz/common/quads/compositor_render_pass.h"
#include "components/viz/common/quads/texture_draw_quad.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/rect_f.h"

namespace cc {

namespace {

// Maximum number of patches that can be produced for one NinePatchLayer.
const int kMaxOcclusionPatches = 12;
const int kMaxPatches = 9;

gfx::Rect BoundsToRect(int x1, int y1, int x2, int y2) {
  return gfx::Rect(x1, y1, x2 - x1, y2 - y1);
}

gfx::RectF NormalizedRect(const gfx::Rect& rect,
                          float total_width,
                          float total_height) {
  return gfx::RectF(rect.x() / total_width, rect.y() / total_height,
                    rect.width() / total_width, rect.height() / total_height);
}

}  // namespace

NinePatchGenerator::Patch::Patch(const gfx::Rect& image_rect,
                                 const gfx::Size& total_image_bounds,
                                 const gfx::Rect& output_rect)
    : image_rect(image_rect),
      normalized_image_rect(NormalizedRect(image_rect,
                                           total_image_bounds.width(),
                                           total_image_bounds.height())),
      output_rect(output_rect) {}

NinePatchGenerator::NinePatchGenerator()
    : fill_center_(false), nearest_neighbor_(false) {}

bool NinePatchGenerator::SetLayout(const gfx::Size& image_bounds,
                                   const gfx::Size& output_bounds,
                                   const gfx::Rect& aperture,
                                   const gfx::Rect& border,
                                   const gfx::Rect& output_occlusion,
                                   bool fill_center,
                                   bool nearest_neighbor) {
  if (image_bounds_ == image_bounds && output_bounds_ == output_bounds &&
      image_aperture_ == aperture && border_ == border &&
      fill_center_ == fill_center && output_occlusion_ == output_occlusion &&
      nearest_neighbor_ == nearest_neighbor)
    return false;

  image_bounds_ = image_bounds;
  output_bounds_ = output_bounds;
  image_aperture_ = aperture;
  border_ = border;
  fill_center_ = fill_center;
  output_occlusion_ = output_occlusion;
  nearest_neighbor_ = nearest_neighbor;

  return true;
}

void NinePatchGenerator::CheckGeometryLimitations() {
  // |border| is in layer space.  It cannot exceed the bounds of the layer.
  DCHECK_GE(output_bounds_.width(), border_.width());
  DCHECK_GE(output_bounds_.height(), border_.height());

  // Sanity Check on |border|
  DCHECK_LE(border_.x(), border_.width());
  DCHECK_LE(border_.y(), border_.height());
  DCHECK_GE(border_.x(), 0);
  DCHECK_GE(border_.y(), 0);

  // |aperture| is in image space.  It cannot exceed the bounds of the bitmap.
  DCHECK(!image_aperture_.size().IsEmpty());
  DCHECK(gfx::Rect(image_bounds_).Contains(image_aperture_))
      << "image_bounds_ " << gfx::Rect(image_bounds_).ToString()
      << " image_aperture_ " << image_aperture_.ToString();

  // Sanity check on |output_occlusion_|. It should always be within the
  // border.
  gfx::Rect border_rect(border_.x(), border_.y(),
                        output_bounds_.width() - border_.width(),
                        output_bounds_.height() - border_.height());
  DCHECK(output_occlusion_.IsEmpty() || output_occlusion_.Contains(border_rect))
      << "border_rect " << border_rect.ToString() << " output_occlusion_ "
      << output_occlusion_.ToString();
}

std::vector<NinePatchGenerator::Patch>
NinePatchGenerator::ComputeQuadsWithoutOcclusion() const {
  float image_width = image_bounds_.width();
  float image_height = image_bounds_.height();
  float output_width = output_bounds_.width();
  float output_height = output_bounds_.height();
  gfx::RectF output_aperture(border_.x(), border_.y(),
                             output_width - border_.width(),
                             output_height - border_.height());

  if (fill_center_) {
    bool match_horizontally =
        output_aperture.x() == image_aperture_.x() &&
        output_aperture.width() == image_aperture_.width() &&
        output_width == image_width;
    bool match_vertically =
        output_aperture.y() == image_aperture_.y() &&
        output_aperture.height() == image_aperture_.height() &&
        output_height == image_height;

    if (match_horizontally) {
      if (match_vertically) {
        // Only need 1 patch.
        return {Patch(gfx::Rect(image_bounds_), image_bounds_,
                      gfx::Rect(image_bounds_))};
      }

      // Only need 3 vertical patches.
      return {Patch(BoundsToRect(0, 0, image_width, image_aperture_.y()),
                    image_bounds_,
                    BoundsToRect(0, 0, image_width, output_aperture.y())),
              Patch(BoundsToRect(0, image_aperture_.y(), image_width,
                                 image_aperture_.bottom()),
                    image_bounds_,
                    BoundsToRect(0, output_aperture.y(), image_width,
                                 output_aperture.bottom())),
              Patch(BoundsToRect(0, image_aperture_.bottom(), image_width,
                                 image_height),
                    image_bounds_,
                    BoundsToRect(0, output_aperture.bottom(), image_width,
                                 output_height))};
    }

    if (match_vertically) {
      // Only need 3 horizontal patches.
      return {Patch(BoundsToRect(0, 0, image_aperture_.x(), image_height),
                    image_bounds_,
                    BoundsToRect(0, 0, output_aperture.x(), image_height)),
              Patch(BoundsToRect(image_aperture_.x(), 0,
                                 image_aperture_.right(), image_height),
                    image_bounds_,
                    BoundsToRect(output_aperture.x(), 0,
                                 output_aperture.right(), image_height)),
              Patch(BoundsToRect(image_aperture_.right(), 0, image_width,
                                 image_height),
                    image_bounds_,
                    BoundsToRect(output_aperture.right(), 0, output_width,
                                 image_height))};
    }
  }

  std::vector<Patch> patches;
  patches.reserve(kMaxPatches);

  // Top-left.
  patches.push_back(
      Patch(BoundsToRect(0, 0, image_aperture_.x(), image_aperture_.y()),
            image_bounds_,
            BoundsToRect(0, 0, output_aperture.x(), output_aperture.y())));

  // Top-right.
  patches.push_back(Patch(BoundsToRect(image_aperture_.right(), 0, image_width,
                                       image_aperture_.y()),
                          image_bounds_,
                          BoundsToRect(output_aperture.right(), 0, output_width,
                                       output_aperture.y())));

  // Bottom-left.
  patches.push_back(Patch(BoundsToRect(0, image_aperture_.bottom(),
                                       image_aperture_.x(), image_height),
                          image_bounds_,
                          BoundsToRect(0, output_aperture.bottom(),
                                       output_aperture.x(), output_height)));

  // Bottom-right.
  patches.push_back(
      Patch(BoundsToRect(image_aperture_.right(), image_aperture_.bottom(),
                         image_width, image_height),
            image_bounds_,
            BoundsToRect(output_aperture.right(), output_aperture.bottom(),
                         output_width, output_height)));

  // Top.
  patches.push_back(
      Patch(BoundsToRect(image_aperture_.x(), 0, image_aperture_.right(),
                         image_aperture_.y()),
            image_bounds_,
            BoundsToRect(output_aperture.x(), 0, output_aperture.right(),
                         output_aperture.y())));

  // Left.
  patches.push_back(
      Patch(BoundsToRect(0, image_aperture_.y(), image_aperture_.x(),
                         image_aperture_.bottom()),
            image_bounds_,
            BoundsToRect(0, output_aperture.y(), output_aperture.x(),
                         output_aperture.bottom())));

  // Right.
  patches.push_back(
      Patch(BoundsToRect(image_aperture_.right(), image_aperture_.y(),
                         image_width, image_aperture_.bottom()),
            image_bounds_,
            BoundsToRect(output_aperture.right(), output_aperture.y(),
                         output_width, output_aperture.bottom())));

  // Bottom.
  patches.push_back(
      Patch(BoundsToRect(image_aperture_.x(), image_aperture_.bottom(),
                         image_aperture_.right(), image_height),
            image_bounds_,
            BoundsToRect(output_aperture.x(), output_aperture.bottom(),
                         output_aperture.right(), output_height)));

  // Center.
  if (fill_center_) {
    patches.push_back(
        Patch(BoundsToRect(image_aperture_.x(), image_aperture_.y(),
                           image_aperture_.right(), image_aperture_.bottom()),
              image_bounds_,
              BoundsToRect(output_aperture.x(), output_aperture.y(),
                           output_aperture.right(), output_aperture.bottom())));
  }

  return patches;
}

std::vector<NinePatchGenerator::Patch>
NinePatchGenerator::ComputeQuadsWithOcclusion() const {
  float image_width = image_bounds_.width();
  float image_height = image_bounds_.height();

  float output_width = output_bounds_.width();
  float output_height = output_bounds_.height();

  float layer_border_right = border_.width() - border_.x();
  float layer_border_bottom = border_.height() - border_.y();

  float image_aperture_right = image_width - image_aperture_.right();
  float image_aperture_bottom = image_height - image_aperture_.bottom();

  float output_occlusion_right = output_width - output_occlusion_.right();
  float output_occlusion_bottom = output_height - output_occlusion_.bottom();

  gfx::RectF image_occlusion(BoundsToRect(
      border_.x() == 0
          ? 0
          : (output_occlusion_.x() * image_aperture_.x() / border_.x()),
      border_.y() == 0
          ? 0
          : (output_occlusion_.y() * image_aperture_.y() / border_.y()),
      image_width - (layer_border_right == 0
                         ? 0
                         : output_occlusion_right * image_aperture_right /
                               layer_border_right),
      image_height - (layer_border_bottom == 0
                          ? 0
                          : output_occlusion_bottom * image_aperture_bottom /
                                layer_border_bottom)));
  gfx::RectF output_aperture(border_.x(), border_.y(),
                             output_width - border_.width(),
                             output_height - border_.height());

  std::vector<Patch> patches;
  patches.reserve(kMaxOcclusionPatches);

  // Top-left-left.
  patches.push_back(
      Patch(BoundsToRect(0, 0, image_occlusion.x(), image_aperture_.y()),
            image_bounds_,
            BoundsToRect(0, 0, output_occlusion_.x(), output_aperture.y())));

  // Top-left-right.
  patches.push_back(
      Patch(BoundsToRect(image_occlusion.x(), 0, image_aperture_.x(),
                         image_occlusion.y()),
            image_bounds_,
            BoundsToRect(output_occlusion_.x(), 0, output_aperture.x(),
                         output_occlusion_.y())));

  // Top-center.
  patches.push_back(
      Patch(BoundsToRect(image_aperture_.x(), 0, image_aperture_.right(),
                         image_occlusion.y()),
            image_bounds_,
            BoundsToRect(output_aperture.x(), 0, output_aperture.right(),
                         output_occlusion_.y())));

  // Top-right-left.
  patches.push_back(
      Patch(BoundsToRect(image_aperture_.right(), 0, image_occlusion.right(),
                         image_occlusion.y()),
            image_bounds_,
            BoundsToRect(output_aperture.right(), 0, output_occlusion_.right(),
                         output_occlusion_.y())));

  // Top-right-right.
  patches.push_back(Patch(BoundsToRect(image_occlusion.right(), 0, image_width,
                                       image_aperture_.y()),
                          image_bounds_,
                          BoundsToRect(output_occlusion_.right(), 0,
                                       output_width, output_aperture.y())));

  // Left-center.
  patches.push_back(
      Patch(BoundsToRect(0, image_aperture_.y(), image_occlusion.x(),
                         image_aperture_.bottom()),
            image_bounds_,
            BoundsToRect(0, output_aperture.y(), output_occlusion_.x(),
                         output_aperture.bottom())));

  // Right-center.
  patches.push_back(
      Patch(BoundsToRect(image_occlusion.right(), image_aperture_.y(),
                         image_width, image_aperture_.bottom()),
            image_bounds_,
            BoundsToRect(output_occlusion_.right(), output_aperture.y(),
                         output_width, output_aperture.bottom())));

  // Bottom-left-left.
  patches.push_back(Patch(BoundsToRect(0, image_aperture_.bottom(),
                                       image_occlusion.x(), image_height),
                          image_bounds_,
                          BoundsToRect(0, output_aperture.bottom(),
                                       output_occlusion_.x(), output_height)));

  // Bottom-left-right.
  patches.push_back(
      Patch(BoundsToRect(image_occlusion.x(), image_occlusion.bottom(),
                         image_aperture_.x(), image_height),
            image_bounds_,
            BoundsToRect(output_occlusion_.x(), output_occlusion_.bottom(),
                         output_aperture.x(), output_height)));

  // Bottom-center.
  patches.push_back(
      Patch(BoundsToRect(image_aperture_.x(), image_occlusion.bottom(),
                         image_aperture_.right(), image_height),
            image_bounds_,
            BoundsToRect(output_aperture.x(), output_occlusion_.bottom(),
                         output_aperture.right(), output_height)));

  // Bottom-right-left.
  patches.push_back(
      Patch(BoundsToRect(image_aperture_.right(), image_occlusion.bottom(),
                         image_occlusion.right(), image_height),
            image_bounds_,
            BoundsToRect(output_aperture.right(), output_occlusion_.bottom(),
                         output_occlusion_.right(), output_height)));

  // Bottom-right-right.
  patches.push_back(
      Patch(BoundsToRect(image_occlusion.right(), image_aperture_.bottom(),
                         image_width, image_height),
            image_bounds_,
            BoundsToRect(output_occlusion_.right(), output_aperture.bottom(),
                         output_width, output_height)));

  return patches;
}

std::vector<NinePatchGenerator::Patch> NinePatchGenerator::GeneratePatches()
    const {
  DCHECK(!output_bounds_.IsEmpty());

  std::vector<Patch> patches;

  if (output_occlusion_.IsEmpty() || fill_center_)
    patches = ComputeQuadsWithoutOcclusion();
  else
    patches = ComputeQuadsWithOcclusion();

  return patches;
}

void NinePatchGenerator::AppendQuadsForCc(
    LayerImpl* layer_impl,
    UIResourceId ui_resource_id,
    viz::CompositorRenderPass* render_pass,
    viz::SharedQuadState* shared_quad_state,
    const std::vector<Patch>& patches,
    const gfx::Vector2d& offset) {
  if (!ui_resource_id) {
    return;
  }

  viz::ResourceId resource =
      layer_impl->layer_tree_impl()->ResourceIdForUIResource(ui_resource_id);
  if (!resource) {
    return;
  }

  const bool opaque =
      layer_impl->layer_tree_impl()->IsUIResourceOpaque(ui_resource_id);
  AppendQuads(
      resource, opaque,
      [layer_impl](const gfx::Rect& rect) {
        return layer_impl->draw_properties()
            .occlusion_in_content_space.GetUnoccludedContentRect(rect);
      },
      layer_impl->layer_tree_impl()->resource_provider(), render_pass,
      shared_quad_state, patches, offset);
}

void NinePatchGenerator::AppendQuads(
    viz::ResourceId resource,
    bool opaque,
    base::FunctionRef<gfx::Rect(const gfx::Rect&)> clip_visible_rect,
    viz::ClientResourceProvider* client_resource_provider,
    viz::CompositorRenderPass* render_pass,
    viz::SharedQuadState* shared_quad_state,
    const std::vector<Patch>& patches,
    const gfx::Vector2d& offset) {
  if (!resource) {
    return;
  }
#if DCHECK_IS_ON()
  client_resource_provider->ValidateResource(resource);
#endif

  constexpr bool flipped = false;
  constexpr bool premultiplied_alpha = true;

  for (const auto& patch : patches) {
    gfx::Rect output_rect = patch.output_rect + offset;
    gfx::Rect visible_rect = clip_visible_rect(output_rect);
    bool needs_blending = !opaque;
    if (!visible_rect.IsEmpty()) {
      gfx::RectF image_rect = patch.normalized_image_rect;
      auto* quad = render_pass->CreateAndAppendDrawQuad<viz::TextureDrawQuad>();
      quad->SetNew(shared_quad_state, output_rect, visible_rect, needs_blending,
                   resource, premultiplied_alpha, image_rect.origin(),
                   image_rect.bottom_right(), SkColors::kTransparent, flipped,
                   nearest_neighbor_,
                   /*secure_output=*/false, gfx::ProtectedVideoType::kClear);
    }
  }
}

void NinePatchGenerator::AsValueInto(
    base::trace_event::TracedValue* state) const {
  MathUtil::AddToTracedValue("ImageAperture", image_aperture_, state);
  MathUtil::AddToTracedValue("ImageBounds", image_bounds_, state);
  MathUtil::AddToTracedValue("Border", border_, state);
  state->SetBoolean("FillCenter", fill_center_);
  MathUtil::AddToTracedValue("OutputOcclusion", output_occlusion_, state);
}

}  // namespace cc
