// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_WEB_APPLICATIONS_SYSTEM_WEB_APP_INSTALL_UTILS_H_
#define CHROME_BROWSER_CHROMEOS_WEB_APPLICATIONS_SYSTEM_WEB_APP_INSTALL_UTILS_H_

#include <initializer_list>
#include <string>

#include "chrome/common/web_application_info.h"

class GURL;

namespace web_app {

struct IconResourceInfo {
  std::string icon_name;
  SquareSizePx size;
  int resource_id;
};

// Create the icon info struct for a system web app from a resource id. We don't
// actually download the icon, so the app_url and icon_name are just used as a
// key.
void CreateIconInfoForSystemWebApp(
    const GURL& app_url,
    const std::initializer_list<IconResourceInfo>& icon_infos,
    WebApplicationInfo& web_app);

}  // namespace web_app

#endif  // CHROME_BROWSER_CHROMEOS_WEB_APPLICATIONS_SYSTEM_WEB_APP_INSTALL_UTILS_H_
