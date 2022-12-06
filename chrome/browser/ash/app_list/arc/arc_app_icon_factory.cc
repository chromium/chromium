// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/arc/arc_app_icon_factory.h"

#include "content/public/browser/browser_context.h"

namespace arc {

ArcAppIconFactory::ArcAppIconFactory() = default;
ArcAppIconFactory::~ArcAppIconFactory() = default;

std::unique_ptr<ArcAppIcon> ArcAppIconFactory::CreateArcAppIcon(
    content::BrowserContext* context,
    const std::string& app_id,
    int size_in_dip,
    ArcAppIcon::Observer* observer,
    ArcAppIcon::IconType icon_type) {
  return std::make_unique<ArcAppIcon>(context, app_id, size_in_dip, observer,
                                      icon_type);
}

}  // namespace arc
