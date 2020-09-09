// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/web_applications/system_web_app_install_utils.h"

#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/resource/resource_bundle.h"

namespace web_app {

void CreateIconInfoForSystemWebApp(
    const GURL& app_url,
    const std::initializer_list<IconResourceInfo>& icon_infos,
    WebApplicationInfo& web_app) {
  for (const auto& info : icon_infos) {
    web_app.icon_infos.emplace_back(app_url.Resolve(info.icon_name), info.size);
    auto image =
        ui::ResourceBundle::GetSharedInstance().GetImageNamed(info.resource_id);
    web_app.icon_bitmaps_any[info.size] = image.AsBitmap();
  }
}
}  // namespace web_app
