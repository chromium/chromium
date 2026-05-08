// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BOOKMARKS_DESKTOP_FORCING_CHROME_BOOKMARK_TEST_CLIENT_H_
#define CHROME_BROWSER_BOOKMARKS_DESKTOP_FORCING_CHROME_BOOKMARK_TEST_CLIENT_H_

#include "chrome/browser/bookmarks/chrome_bookmark_client.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

// Behaves exactly the same as ChromeBookmarkClient but "forces" desktop
// bookmarks behavior by having GetBookmarkFormFactor return
// BookmarkFormFactor::kDesktop.
// TODO(crbug.com/509156770): This class only exists because `is_desktop()` in
// device_info.h returns false for desktop Android tests. Using this class is
// strongly discouraged! Remove this class once fixed.
class DesktopForcingChromeBookmarkTestClient : public ChromeBookmarkClient {
 public:
  using ChromeBookmarkClient::ChromeBookmarkClient;

  DesktopForcingChromeBookmarkTestClient(
      const DesktopForcingChromeBookmarkTestClient&) = delete;
  DesktopForcingChromeBookmarkTestClient& operator=(
      const DesktopForcingChromeBookmarkTestClient&) = delete;

  // ChromeBookmarkClient:
  bookmarks::BookmarkFormFactor GetBookmarkFormFactor() override;

  static BrowserContextKeyedServiceFactory::TestingFactory GetTestingFactory();
};

#endif  // CHROME_BROWSER_BOOKMARKS_DESKTOP_FORCING_CHROME_BOOKMARK_TEST_CLIENT_H_
