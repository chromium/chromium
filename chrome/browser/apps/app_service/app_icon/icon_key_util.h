// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_APP_ICON_ICON_KEY_UTIL_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_APP_ICON_ICON_KEY_UTIL_H_

// Utility classes for providing an App Service IconKey.

#include "components/services/app_service/public/cpp/icon_types.h"
#include "components/services/app_service/public/mojom/types.mojom.h"

namespace apps_util {

// Creates IconKeys such that passing the same arguments twice to CreateIconKey
// will result in different IconKeys (different not just in the pointer sense,
// but their IconKey.timeline values will also differ).
//
// Callers (which are presumably App Service app publishers) can therefore
// publish such IconKeys whenever an app's icon changes. Even though the app_id
// does not change, App Service app subscribers will notice (and reload) the
// new icon from the new (changed) IconKey.
//
// The IconKey.resource_id is always zero, as resource-backed icons do not
// change without a browser re-start.
//
// TODO(crbug.com/1253250): Remove MakeIconKey.
class IncrementingIconKeyFactory {
 public:
  IncrementingIconKeyFactory();
  IncrementingIconKeyFactory(const IncrementingIconKeyFactory&) = delete;
  IncrementingIconKeyFactory& operator=(const IncrementingIconKeyFactory&) =
      delete;

  apps::mojom::IconKeyPtr MakeIconKey(uint32_t icon_effects);

  std::unique_ptr<apps::IconKey> CreateIconKey(uint32_t icon_effects);

 private:
  uint64_t last_timeline_ = 0;
};

}  // namespace apps_util

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_APP_ICON_ICON_KEY_UTIL_H_
