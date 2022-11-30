// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_APP_ICON_ARC_ACTIVITY_ADAPTIVE_ICON_IMPL_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_APP_ICON_ARC_ACTIVITY_ADAPTIVE_ICON_IMPL_H_

#include <vector>

#include "ash/components/arc/mojom/intent_helper.mojom.h"
#include "components/arc/common/intent_helper/adaptive_icon_delegate.h"

namespace arc {
class AdaptiveIconDelegate;
}

namespace apps {

// An icon loader specific to an activity icon from ARC, to generate the
// adaptive icon with the raw foreground and background icon images.
class ArcActivityAdaptiveIconImpl : public arc::AdaptiveIconDelegate {
 public:
  ArcActivityAdaptiveIconImpl();
  ~ArcActivityAdaptiveIconImpl() override;

  ArcActivityAdaptiveIconImpl(const ArcActivityAdaptiveIconImpl&) = delete;
  ArcActivityAdaptiveIconImpl& operator=(const ArcActivityAdaptiveIconImpl&) =
      delete;

  void GenerateAdaptiveIcons(
      const std::vector<arc::mojom::ActivityIconPtr>& icons,
      AdaptiveIconDelegateCallback callback) override;
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_APP_ICON_ARC_ACTIVITY_ADAPTIVE_ICON_IMPL_H_
