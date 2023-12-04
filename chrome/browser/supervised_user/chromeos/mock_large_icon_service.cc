// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/chromeos/mock_large_icon_service.h"

#include "base/task/cancelable_task_tracker.h"
#include "components/favicon_base/favicon_types.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/image/image_unittest_util.h"

using testing::_;

namespace {
const base::CancelableTaskTracker::TaskId kTaskId = 1;
}  // namespace

MockLargeIconService::MockLargeIconService() {
  ON_CALL(*this, GetLargeIconOrFallbackStyleFromGoogleServerSkippingLocalCache(
                     _, _, _, _, _))
      .WillByDefault(
          [this](auto, auto, auto, auto,
                 favicon_base::GoogleFaviconServerCallback callback) {
            StoreIconInCache();
            std::move(callback).Run(
                favicon_base::GoogleFaviconServerRequestStatus::SUCCESS);
          });

  ON_CALL(*this, GetLargeIconImageOrFallbackStyleForPageUrl(_, _, _, _, _))
      .WillByDefault([this](auto, auto, auto,
                            favicon_base::LargeIconImageCallback callback,
                            auto) {
        std::move(callback).Run(
            favicon_base::LargeIconImageResult(gfx::Image(favicon_), kIconUrl));
        return kTaskId;
      });
}

MockLargeIconService::~MockLargeIconService() = default;

void MockLargeIconService::StoreIconInCache() {
  favicon_ = gfx::test::CreateImageSkia(1, 2);
}
