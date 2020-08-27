// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_WEB_APPLICATIONS_TELEMETRY_EXTENSION_WEB_APP_INFO_H_
#define CHROME_BROWSER_CHROMEOS_WEB_APPLICATIONS_TELEMETRY_EXTENSION_WEB_APP_INFO_H_

#include <memory>

#if defined(OFFICIAL_BUILD)
#error Telemetry Extension should only be included in unofficial builds.
#endif

struct WebApplicationInfo;

// Return a WebApplicationInfo used to install the app.
std::unique_ptr<WebApplicationInfo> CreateWebAppInfoForTelemetryExtension();

#endif  // CHROME_BROWSER_CHROMEOS_WEB_APPLICATIONS_TELEMETRY_EXTENSION_WEB_APP_INFO_H_
