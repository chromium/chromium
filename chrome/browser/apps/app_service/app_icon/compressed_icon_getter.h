// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_APP_ICON_COMPRESSED_ICON_GETTER_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_APP_ICON_COMPRESSED_ICON_GETTER_H_

#include "components/services/app_service/public/cpp/icon_types.h"

namespace ui {
enum ResourceScaleFactor : int;
}

namespace apps {
// The interface for getting compressed icon from the publishers.
// This is used in both app publisher and shortcut publisher.
class CompressedIconGetter {
 public:
  // Requests a compressed icon data for an app service item (app or shortcut)
  // identified by `id`. The icon is identified by `size_in_dip` and
  // `scale_factor`. Calls `callback` with the result.
  virtual void GetCompressedIconData(const std::string& id,
                                     int32_t size_in_dip,
                                     ui::ResourceScaleFactor scale_factor,
                                     LoadIconCallback callback) = 0;
};
}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_APP_ICON_COMPRESSED_ICON_GETTER_H_
