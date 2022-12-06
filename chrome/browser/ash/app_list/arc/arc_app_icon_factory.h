// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_ARC_ARC_APP_ICON_FACTORY_H_
#define CHROME_BROWSER_ASH_APP_LIST_ARC_ARC_APP_ICON_FACTORY_H_

#include <memory>

#include "chrome/browser/ash/app_list/arc/arc_app_icon.h"

namespace content {
class BrowserContext;
}

namespace arc {

class ArcAppIconFactory {
 public:
  ArcAppIconFactory();
  virtual ~ArcAppIconFactory();

  ArcAppIconFactory(const ArcAppIconFactory&) = delete;
  ArcAppIconFactory& operator=(const ArcAppIconFactory&) = delete;

  virtual std::unique_ptr<ArcAppIcon> CreateArcAppIcon(
      content::BrowserContext* context,
      const std::string& app_id,
      int resource_size_in_dip,
      ArcAppIcon::Observer* observer,
      ArcAppIcon::IconType icon_type);
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_APP_LIST_ARC_ARC_APP_ICON_FACTORY_H_
