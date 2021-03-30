// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_WEB_APPLICATIONS_FILE_MANAGER_WEB_APP_INFO_H_
#define CHROME_BROWSER_ASH_WEB_APPLICATIONS_FILE_MANAGER_WEB_APP_INFO_H_

#include <memory>

#if defined(OFFICIAL_BUILD)
#error File Manager should only be included in unofficial builds.
#endif

struct WebApplicationInfo;

// Return a WebApplicationInfo used to install the app.
std::unique_ptr<WebApplicationInfo> CreateWebAppInfoForFileManager();

#endif  // CHROME_BROWSER_ASH_WEB_APPLICATIONS_FILE_MANAGER_WEB_APP_INFO_H_
