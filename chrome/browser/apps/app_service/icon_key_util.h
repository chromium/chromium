// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_ICON_KEY_UTIL_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_ICON_KEY_UTIL_H_

// Utility classes for providing an App Service IconKey.

#include <string>

#include "base/macros.h"
#include "chrome/services/app_service/public/mojom/types.mojom.h"

namespace apps_util {

// Creates IconKeys such that passing the same arguments twice to MakeIconKey
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
class IncrementingIconKeyFactory {
 public:
  IncrementingIconKeyFactory();

  apps::mojom::IconKeyPtr MakeIconKey(uint32_t icon_effects);

 private:
  uint64_t last_timeline_;

  DISALLOW_COPY_AND_ASSIGN(IncrementingIconKeyFactory);
};

}  // namespace apps_util

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_ICON_KEY_UTIL_H_
