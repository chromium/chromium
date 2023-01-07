// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_TEST_SUPPORT_TEST_SYSTEM_WEB_APP_URL_DATA_SOURCE_H_
#define CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_TEST_SUPPORT_TEST_SYSTEM_WEB_APP_URL_DATA_SOURCE_H_

#include <string>

namespace content {
class BrowserContext;
}

namespace ash {

// Creates and registers a URLDataSource that serves a blank page with
// a manifest.
void AddTestURLDataSource(const std::string& source_name,
                          content::BrowserContext* browser_context);
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_TEST_SUPPORT_TEST_SYSTEM_WEB_APP_URL_DATA_SOURCE_H_
