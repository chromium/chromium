// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/solid_color_content_layer_client.h"

#include <stddef.h>

#include "cc/paint/paint_flags.h"
#include "cc/paint/paint_op_buffer.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/skia_conversions.h"

namespace cc {

scoped_refptr<DisplayItemList>
SolidColorContentLayerClient::PaintContentsToDisplayList() {
  auto display_list = base::MakeRefCounted<DisplayItemList>();
  display_list->StartPaint();
  display_list->push<SaveOp>();

  SkRect clip = gfx::RectToSkRect(gfx::Rect(size_));
  display_list->push<ClipRectOp>(clip, SkClipOp::kIntersect, false);
  SkColor4f color = SkColors::kTransparent;
  display_list->push<DrawColorOp>(color, SkBlendMode::kSrc);

  if (border_size_ != 0) {
    PaintFlags flags;
    flags.setStyle(PaintFlags::kFill_Style);
    flags.setColor(border_color_);
    display_list->push<DrawRectOp>(clip, flags);
  }

  PaintFlags flags;
  flags.setStyle(PaintFlags::kFill_Style);
  flags.setColor(color_);
  display_list->push<DrawRectOp>(clip.makeInset(border_size_, border_size_),
                                 flags);

  display_list->push<RestoreOp>();
  display_list->EndPaintOfUnpaired(gfx::Rect(size_));
  display_list->Finalize();
  return display_list;
}

bool SolidColorContentLayerClient::FillsBoundsCompletely() const {
  return false;
}

}  // namespace cc
