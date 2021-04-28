// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_WEB_APPLICATIONS_DEMO_MODE_WEB_APP_INFO_H_
#define CHROME_BROWSER_ASH_WEB_APPLICATIONS_DEMO_MODE_WEB_APP_INFO_H_

#include <memory>

struct WebApplicationInfo;

std::unique_ptr<WebApplicationInfo> CreateWebAppInfoForDemoModeApp();

#endif  // CHROME_BROWSER_ASH_WEB_APPLICATIONS_DEMO_MODE_WEB_APP_INFO_H_
