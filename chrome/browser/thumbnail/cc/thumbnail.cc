// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/thumbnail/cc/thumbnail.h"

#include "base/bind.h"
#include "base/check.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "ui/android/resources/ui_resource_provider.h"
#include "ui/gfx/geometry/size_conversions.h"

namespace {

SkBitmap CreateSmallHolderBitmap() {
  SkBitmap small_bitmap;
  small_bitmap.allocPixels(
      SkImageInfo::Make(1, 1, kRGBA_8888_SkColorType, kOpaque_SkAlphaType));
  SkCanvas canvas(small_bitmap);
  canvas.drawColor(SK_ColorWHITE);
  small_bitmap.setImmutable();
  return small_bitmap;
}

}  // anonymous namespace

std::unique_ptr<Thumbnail> Thumbnail::Create(
    TabId tab_id,
    const base::Time& time_stamp,
    float scale,
    ui::UIResourceProvider* ui_resource_provider,
    ThumbnailDelegate* thumbnail_delegate) {
  return base::WrapUnique(new Thumbnail(
      tab_id, time_stamp, scale, ui_resource_provider, thumbnail_delegate));
}

Thumbnail::Thumbnail(TabId tab_id,
                     const base::Time& time_stamp,
                     float scale,
                     ui::UIResourceProvider* ui_resource_provider,
                     ThumbnailDelegate* thumbnail_delegate)
    : tab_id_(tab_id),
      time_stamp_(time_stamp),
      scale_(scale),
      bitmap_(gfx::Size(1, 1), true),
      ui_resource_id_(0),
      retrieved_(false),
      ui_resource_provider_(ui_resource_provider),
      thumbnail_delegate_(thumbnail_delegate) {}

Thumbnail::~Thumbnail() {
  ClearUIResourceId();
}

void Thumbnail::SetBitmap(const SkBitmap& bitmap) {
  DCHECK(!bitmap.empty());
  retrieved_ = false;
  ClearUIResourceId();
  scaled_content_size_ =
      gfx::ScaleSize(gfx::SizeF(bitmap.width(), bitmap.height()), 1.f / scale_);
  scaled_data_size_ = scaled_content_size_;
  bitmap_ = cc::UIResourceBitmap(bitmap);
}

void Thumbnail::SetCompressedBitmap(sk_sp<SkPixelRef> compressed_bitmap,
                                    const gfx::Size& content_size) {
  DCHECK(compressed_bitmap);
  DCHECK(!content_size.IsEmpty());
  retrieved_ = false;
  ClearUIResourceId();
  gfx::Size data_size(compressed_bitmap->width(), compressed_bitmap->height());
  scaled_content_size_ = gfx::ScaleSize(gfx::SizeF(content_size), 1.f / scale_);
  scaled_data_size_ = gfx::ScaleSize(gfx::SizeF(data_size), 1.f / scale_);
  bitmap_ = cc::UIResourceBitmap(std::move(compressed_bitmap), data_size);
}

void Thumbnail::CreateUIResource() {
  DCHECK(ui_resource_provider_);
  if (!ui_resource_id_)
    ui_resource_id_ = ui_resource_provider_->CreateUIResource(this);
}

cc::UIResourceBitmap Thumbnail::GetBitmap(cc::UIResourceId uid,
                                          bool resource_lost) {
  if (retrieved_) {
    // InvalidateCachedThumbnail() causes |this| to be deleted, so
    // don't delete the resource while LayerTeeHost calls into |this|
    // to avoid reentry there.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(&Thumbnail::DoInvalidate, weak_factory_.GetWeakPtr()));
    return bitmap_;
  }

  retrieved_ = true;

  cc::UIResourceBitmap old_bitmap(bitmap_);
  // Return a place holder for all other calls to GetBitmap.
  bitmap_ = cc::UIResourceBitmap(CreateSmallHolderBitmap());
  return old_bitmap;
}

void Thumbnail::DoInvalidate() {
  if (thumbnail_delegate_)
    thumbnail_delegate_->InvalidateCachedThumbnail(this);
}

void Thumbnail::ClearUIResourceId() {
  if (ui_resource_id_ && ui_resource_provider_)
    ui_resource_provider_->DeleteUIResource(ui_resource_id_);
  ui_resource_id_ = 0;
}
