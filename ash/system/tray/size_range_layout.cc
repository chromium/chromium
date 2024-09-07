// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <limits>

#include "ash/system/tray/size_range_layout.h"

#include "base/check.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/layout/fill_layout.h"

namespace ash {

// static

const int SizeRangeLayout::kAbsoluteMinSize = 0;
const int SizeRangeLayout::kAbsoluteMaxSize = std::numeric_limits<int>::max();

// Non static

SizeRangeLayout::SizeRangeLayout()
    : SizeRangeLayout(gfx::Size(kAbsoluteMinSize, kAbsoluteMinSize),
                      gfx::Size(kAbsoluteMaxSize, kAbsoluteMaxSize)) {}

SizeRangeLayout::SizeRangeLayout(const gfx::Size& size)
    : SizeRangeLayout(size, size) {}

SizeRangeLayout::SizeRangeLayout(const gfx::Size& min_size,
                                 const gfx::Size& max_size)
    : min_size_(gfx::Size(kAbsoluteMinSize, kAbsoluteMinSize)),
      max_size_(gfx::Size(kAbsoluteMaxSize, kAbsoluteMaxSize)) {
  SetLayoutManager(std::make_unique<views::FillLayout>());
  SetMinSize(min_size);
  SetMaxSize(max_size);
}

SizeRangeLayout::~SizeRangeLayout() = default;

void SizeRangeLayout::SetSize(const gfx::Size& size) {
  SetMinSize(size);
  SetMaxSize(size);
}

void SizeRangeLayout::SetMinSize(const gfx::Size& size) {
  min_size_ = size;
  min_size_.SetToMax(gfx::Size());
  max_size_.SetToMax(min_size_);
}

void SizeRangeLayout::SetMaxSize(const gfx::Size& size) {
  max_size_ = size;
  max_size_.SetToMax(gfx::Size());
  min_size_.SetToMin(max_size_);
}

gfx::Size SizeRangeLayout::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  gfx::Size preferred_size =
      GetLayoutManager()->GetPreferredSize(this, available_size);
  ClampSizeToRange(&preferred_size);
  return preferred_size;
}

void SizeRangeLayout::ChildPreferredSizeChanged(View* child) {
  GetLayoutManager()->InvalidateLayout();
}

void SizeRangeLayout::ClampSizeToRange(gfx::Size* size) const {
  size->SetToMax(min_size_);
  size->SetToMin(max_size_);
}

BEGIN_METADATA(SizeRangeLayout)
END_METADATA

}  // namespace ash
