// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_THUMBNAIL_CC_THUMBNAIL_H_
#define CHROME_BROWSER_THUMBNAIL_CC_THUMBNAIL_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "cc/resources/ui_resource_bitmap.h"
#include "cc/resources/ui_resource_client.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "ui/gfx/geometry/size_f.h"

namespace base {
class Time;
}

namespace gfx {
class Size;
}

namespace ui {
class UIResourceProvider;
}

namespace thumbnail {

typedef int TabId;

class Thumbnail;

class ThumbnailDelegate {
 public:
  virtual void InvalidateCachedThumbnail(Thumbnail* thumbnail) = 0;
  virtual ~ThumbnailDelegate() {}
};

class Thumbnail : public cc::UIResourceClient {
 public:
  static std::unique_ptr<Thumbnail> Create(
      TabId tab_id,
      const base::Time& time_stamp,
      float scale,
      base::WeakPtr<ui::UIResourceProvider> ui_resource_provider,
      ThumbnailDelegate* thumbnail_delegate);

  Thumbnail(const Thumbnail&) = delete;
  Thumbnail& operator=(const Thumbnail&) = delete;

  ~Thumbnail() override;

  TabId tab_id() const { return tab_id_; }
  base::Time time_stamp() const { return time_stamp_; }
  float scale() const { return scale_; }
  cc::UIResourceId ui_resource_id() const { return ui_resource_id_; }
  const gfx::SizeF& scaled_content_size() const { return scaled_content_size_; }
  const gfx::SizeF& scaled_data_size() const { return scaled_data_size_; }
  size_t size_in_bytes() const { return size_in_bytes_; }

  void SetBitmap(const SkBitmap& bitmap);
  void SetCompressedBitmap(sk_sp<SkPixelRef> compressed_bitmap,
                           const gfx::Size& content_size);
  void CreateUIResource();

  // cc::UIResourceClient implementation.
  cc::UIResourceBitmap GetBitmap(cc::UIResourceId uid,
                                 bool resource_lost) override;

 private:
  Thumbnail(TabId tab_id,
            const base::Time& time_stamp,
            float scale,
            base::WeakPtr<ui::UIResourceProvider> ui_resource_provider,
            ThumbnailDelegate* thumbnail_delegate);

  void ClearUIResourceId();
  void DoInvalidate();

  TabId tab_id_;
  base::Time time_stamp_;
  float scale_;

  gfx::SizeF scaled_content_size_;
  gfx::SizeF scaled_data_size_;

  size_t size_in_bytes_ = 1U;
  cc::UIResourceBitmap bitmap_;
  cc::UIResourceId ui_resource_id_;

  bool retrieved_;

  base::WeakPtr<ui::UIResourceProvider> ui_resource_provider_;
  raw_ptr<ThumbnailDelegate> thumbnail_delegate_;

  base::WeakPtrFactory<Thumbnail> weak_factory_{this};
};

}  // namespace thumbnail

#endif  // CHROME_BROWSER_THUMBNAIL_CC_THUMBNAIL_H_
