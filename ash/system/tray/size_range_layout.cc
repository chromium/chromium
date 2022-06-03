// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <limits>

#include "ash/system/tray/size_range_layout.h"

#include "base/check.h"
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
    : layout_manager_(new views::FillLayout()),
      min_size_(gfx::Size(kAbsoluteMinSize, kAbsoluteMinSize)),
      max_size_(gfx::Size(kAbsoluteMaxSize, kAbsoluteMaxSize)) {
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

void SizeRangeLayout::SetLayoutManager(
    std::unique_ptr<LayoutManager> layout_manager) {
  DCHECK(layout_manager_);
  layout_manager_ = std::move(layout_manager);
  layout_manager_->Installed(host_);
}

void SizeRangeLayout::Installed(views::View* host) {
  DCHECK(!host_);
  host_ = host;
  layout_manager_->Installed(host);
}

void SizeRangeLayout::Layout(views::View* host) {
  layout_manager_->Layout(host);
}

gfx::Size SizeRangeLayout::GetPreferredSize(const views::View* host) const {
  gfx::Size preferred_size = layout_manager_->GetPreferredSize(host);
  ClampSizeToRange(&preferred_size);
  return preferred_size;
}

int SizeRangeLayout::GetPreferredHeightForWidth(const views::View* host,
                                                int width) const {
  const int height = layout_manager_->GetPreferredHeightForWidth(host, width);
  gfx::Size size(0, height);
  ClampSizeToRange(&size);
  return size.height();
}

void SizeRangeLayout::ViewAdded(views::View* host, views::View* view) {
  layout_manager_->ViewAdded(host, view);
}

void SizeRangeLayout::ViewRemoved(views::View* host, views::View* view) {
  layout_manager_->ViewRemoved(host, view);
}

void SizeRangeLayout::ClampSizeToRange(gfx::Size* size) const {
  size->SetToMax(min_size_);
  size->SetToMin(max_size_);
}

}  // namespace ash
