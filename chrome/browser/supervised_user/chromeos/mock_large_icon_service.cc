// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/chromeos/mock_large_icon_service.h"

#include <vector>

#include "base/memory/ref_counted_memory.h"
#include "base/task/cancelable_task_tracker.h"
#include "components/favicon_base/favicon_types.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/image/image_unittest_util.h"

using testing::_;

namespace {
const base::CancelableTaskTracker::TaskId kTaskId = 1;
}  // namespace

MockLargeIconService::MockLargeIconService() {
  ON_CALL(*this, GetLargeIconOrFallbackStyleFromGoogleServerSkippingLocalCache(
                     _, _, _, _))
      .WillByDefault(
          [this](auto, auto, auto,
                 favicon_base::GoogleFaviconServerCallback callback) {
            StoreIconInCache();
            std::move(callback).Run(
                favicon_base::GoogleFaviconServerRequestStatus::SUCCESS);
          });

  ON_CALL(*this, GetLargeIconImageOrFallbackStyleForPageUrl(_, _, _, _, _))
      .WillByDefault([this](auto, auto, auto,
                            favicon_base::LargeIconImageCallback callback,
                            auto) {
        std::move(callback).Run(favicon_base::LargeIconImageResult(
            gfx::Image::CreateFrom1xBitmap(favicon_), kIconUrl));
        return kTaskId;
      });

  ON_CALL(*this, GetLargeIconRawBitmapForPageUrl(_, _, _, _, _, _))
      .WillByDefault([this](auto, auto, auto, auto,
                            favicon_base::LargeIconCallback callback, auto) {
        favicon_base::FaviconRawBitmapResult result;
        result.icon_url = kIconUrl;
        std::vector<unsigned char> png_bytes;
        gfx::PNGCodec::EncodeBGRASkBitmap(
            favicon_, /*discard_transparency=*/false, &png_bytes);

        result.bitmap_data = base::RefCountedBytes::TakeVector(&png_bytes);
        std::move(callback).Run(favicon_base::LargeIconResult(result));
        return kTaskId;
      });
}

MockLargeIconService::~MockLargeIconService() = default;

void MockLargeIconService::StoreIconInCache() {
  favicon_ = gfx::test::CreateBitmap(1);
}
