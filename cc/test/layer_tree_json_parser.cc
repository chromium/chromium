// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "cc/test/layer_tree_json_parser.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/test/values_test_util.h"
#include "base/values.h"
#include "cc/layers/layer.h"
#include "cc/layers/nine_patch_layer.h"
#include "cc/layers/picture_layer.h"
#include "cc/layers/solid_color_layer.h"
#include "cc/layers/texture_layer.h"
#include "cc/trees/layer_tree_settings.h"

namespace cc {

namespace {

scoped_refptr<Layer> ParseTreeFromValue(const base::Value& val,
                                        ContentLayerClient* content_client) {
  if (!val.is_dict())
    return nullptr;
  const base::Value::Dict& dict = val.GetDict();

  const std::string* layer_type = dict.FindString("LayerType");
  if (!layer_type)
    return nullptr;

  const base::Value::List* bounds_list = dict.FindList("Bounds");
  if (!bounds_list)
    return nullptr;
  if (bounds_list->size() < 2)
    return nullptr;

  std::optional<int> width = (*bounds_list)[0].GetIfInt();
  std::optional<int> height = (*bounds_list)[1].GetIfInt();
  if (!width.has_value() || !height.has_value())
    return nullptr;

  std::optional<bool> draws_content = dict.FindBool("DrawsContent");
  if (!draws_content.has_value())
    return nullptr;

  std::optional<bool> hit_testable = dict.FindBool("HitTestable");
  // If we cannot load hit_testable, we may try loading the old version, since
  // we do not record |hit_testable_without_draws_content| in the past, we use
  // |draws_content| as the value of |hit_testable|.
  if (!hit_testable.has_value()) {
    hit_testable = *draws_content;
  }

  scoped_refptr<Layer> new_layer;
  if (*layer_type == "SolidColorLayer") {
    new_layer = SolidColorLayer::Create();
  } else if (*layer_type == "NinePatchLayer") {
    const base::Value::List* aperture_list = dict.FindList("ImageAperture");
    if (!aperture_list)
      return nullptr;
    if (aperture_list->size() < 4)
      return nullptr;

    std::optional<int> aperture_x = (*aperture_list)[0].GetIfInt();
    std::optional<int> aperture_y = (*aperture_list)[1].GetIfInt();
    std::optional<int> aperture_width = (*aperture_list)[2].GetIfInt();
    std::optional<int> aperture_height = (*aperture_list)[3].GetIfInt();
    if (!(aperture_x.has_value() && aperture_y.has_value() &&
          aperture_width.has_value() && aperture_height.has_value()))
      return nullptr;

    const base::Value::List* image_bounds_list = dict.FindList("ImageBounds");
    if (!image_bounds_list)
      return nullptr;
    if (image_bounds_list->size() < 2)
      return nullptr;

    std::optional<double> image_width = (*image_bounds_list)[0].GetIfDouble();
    std::optional<double> image_height = (*image_bounds_list)[1].GetIfDouble();
    if (!(image_width.has_value() && image_height.has_value()))
      return nullptr;

    const base::Value::List* border_list = dict.FindList("Border");
    if (!border_list)
      return nullptr;
    if (border_list->size() < 4)
      return nullptr;

    std::optional<int> border_x = (*border_list)[0].GetIfInt();
    std::optional<int> border_y = (*border_list)[1].GetIfInt();
    std::optional<int> border_width = (*border_list)[2].GetIfInt();
    std::optional<int> border_height = (*border_list)[3].GetIfInt();

    if (!(border_x.has_value() && border_y.has_value() &&
          border_width.has_value() && border_height.has_value()))
      return nullptr;

    std::optional<bool> fill_center = dict.FindBool("FillCenter");
    if (!fill_center.has_value())
      return nullptr;

    scoped_refptr<NinePatchLayer> nine_patch_layer = NinePatchLayer::Create();

    SkBitmap bitmap;
    bitmap.allocN32Pixels(*image_width, *image_height);
    bitmap.setImmutable();
    nine_patch_layer->SetBitmap(bitmap);
    nine_patch_layer->SetAperture(
        gfx::Rect(*aperture_x, *aperture_y, *aperture_width, *aperture_height));
    nine_patch_layer->SetBorder(
        gfx::Rect(*border_x, *border_y, *border_width, *border_height));
    nine_patch_layer->SetFillCenter(*fill_center);

    new_layer = nine_patch_layer;
  } else if (*layer_type == "TextureLayer") {
    new_layer = TextureLayer::CreateForMailbox(nullptr);
  } else if (*layer_type == "PictureLayer") {
    new_layer = PictureLayer::Create(content_client);
  } else {  // Type "Layer" or "unknown"
    new_layer = Layer::Create();
  }
  new_layer->SetBounds(gfx::Size(*width, *height));
  new_layer->SetIsDrawable(*draws_content);
  new_layer->SetHitTestable(*hit_testable);

  std::optional<double> opacity = dict.FindDouble("Opacity");
  if (opacity.has_value())
    new_layer->SetOpacity(*opacity);

  std::optional<bool> contents_opaque = dict.FindBool("ContentsOpaque");
  if (contents_opaque.has_value())
    new_layer->SetContentsOpaque(*contents_opaque);

  const base::Value::List* touch_region_list = dict.FindList("TouchRegion");

  if (touch_region_list) {
    TouchActionRegion touch_action_region;
    for (size_t i = 0; i + 3 < touch_region_list->size(); i += 4) {
      std::optional<int> rect_x = (*touch_region_list)[i + 0].GetIfInt();
      std::optional<int> rect_y = (*touch_region_list)[i + 1].GetIfInt();
      std::optional<int> rect_width = (*touch_region_list)[i + 2].GetIfInt();
      std::optional<int> rect_height = (*touch_region_list)[i + 3].GetIfInt();

      if (!(rect_x.has_value() && rect_y.has_value() &&
            rect_width.has_value() && rect_height.has_value()))
        return nullptr;

      touch_action_region.Union(
          TouchAction::kNone,
          gfx::Rect(*rect_x, *rect_y, *rect_width, *rect_height));
    }
    new_layer->SetTouchActionRegion(std::move(touch_action_region));
  }

  const base::Value::List* transform_list = dict.FindList("Transform");
  if (!transform_list)
    return nullptr;
  if (transform_list->size() < 16)
    return nullptr;

  float transform[16];
  for (int i = 0; i < 16; ++i) {
    // GetDouble can implicitly convert from either double or int; however, it's
    // not clear if "is_double" is sufficient for this check. Given that int is
    // also a valid type that can be gotten, check it here.
    if (!((*transform_list)[i].is_double() || (*transform_list)[i].is_int())) {
      return nullptr;
    }

    transform[i] = (*transform_list)[i].GetDouble();
  }

  new_layer->SetTransform(gfx::Transform::ColMajorF(transform));

  const base::Value::List* child_list = dict.FindList("Children");
  if (!child_list)
    return nullptr;
  for (const auto& value : *child_list) {
    new_layer->AddChild(ParseTreeFromValue(value, content_client));
  }

  return new_layer;
}

}  // namespace

scoped_refptr<Layer> ParseTreeFromJson(std::string json,
                                       ContentLayerClient* content_client) {
  return ParseTreeFromValue(base::test::ParseJson(json), content_client);
}

}  // namespace cc
