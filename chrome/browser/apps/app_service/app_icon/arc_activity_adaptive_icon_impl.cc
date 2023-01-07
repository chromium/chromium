// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/app_icon/arc_activity_adaptive_icon_impl.h"

#include <utility>

#include "base/functional/bind.h"
#include "chrome/browser/apps/app_service/app_icon/app_icon_factory.h"
#include "components/arc/common/intent_helper/adaptive_icon_delegate.h"

namespace apps {

ArcActivityAdaptiveIconImpl::ArcActivityAdaptiveIconImpl() = default;

ArcActivityAdaptiveIconImpl::~ArcActivityAdaptiveIconImpl() = default;

void ArcActivityAdaptiveIconImpl::GenerateAdaptiveIcons(
    const std::vector<arc::mojom::ActivityIconPtr>& icons,
    AdaptiveIconDelegateCallback callback) {
  apps::ArcActivityIconsToImageSkias(
      icons, base::BindOnce(
                 [](AdaptiveIconDelegateCallback callback,
                    const std::vector<gfx::ImageSkia>& images) {
                   std::move(callback).Run(images);
                 },
                 std::move(callback)));
}

}  // namespace apps
