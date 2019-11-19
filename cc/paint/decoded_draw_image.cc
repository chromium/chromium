// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/decoded_draw_image.h"

namespace cc {

DecodedDrawImage::DecodedDrawImage(sk_sp<const SkImage> image,
                                   const SkSize& src_rect_offset,
                                   const SkSize& scale_adjustment,
                                   SkFilterQuality filter_quality,
                                   bool is_budgeted)
    : image_(std::move(image)),
      src_rect_offset_(src_rect_offset),
      scale_adjustment_(scale_adjustment),
      filter_quality_(filter_quality),
      is_budgeted_(is_budgeted) {}

DecodedDrawImage::DecodedDrawImage(
    base::Optional<uint32_t> transfer_cache_entry_id,
    const SkSize& src_rect_offset,
    const SkSize& scale_adjustment,
    SkFilterQuality filter_quality,
    bool needs_mips,
    bool is_budgeted)
    : transfer_cache_entry_id_(transfer_cache_entry_id),
      src_rect_offset_(src_rect_offset),
      scale_adjustment_(scale_adjustment),
      filter_quality_(filter_quality),
      transfer_cache_entry_needs_mips_(needs_mips),
      is_budgeted_(is_budgeted) {}

DecodedDrawImage::DecodedDrawImage()
    : DecodedDrawImage(nullptr,
                       SkSize::MakeEmpty(),
                       SkSize::Make(1.f, 1.f),
                       kNone_SkFilterQuality,
                       true) {}

DecodedDrawImage::DecodedDrawImage(const DecodedDrawImage&) = default;
DecodedDrawImage::DecodedDrawImage(DecodedDrawImage&&) = default;
DecodedDrawImage& DecodedDrawImage::operator=(const DecodedDrawImage&) =
    default;
DecodedDrawImage& DecodedDrawImage::operator=(DecodedDrawImage&&) = default;

DecodedDrawImage::~DecodedDrawImage() = default;

}  // namespace cc
