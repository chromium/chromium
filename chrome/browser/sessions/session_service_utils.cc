// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sessions/session_service_utils.h"

#include "build/build_config.h"

sessions::SessionWindow::WindowType WindowTypeForBrowserType(
    BrowserWindowInterface::Type type) {
  switch (type) {
    case BrowserWindowInterface::TYPE_NORMAL:
      return sessions::SessionWindow::TYPE_NORMAL;
    case BrowserWindowInterface::TYPE_POPUP:
      return sessions::SessionWindow::TYPE_POPUP;
    case BrowserWindowInterface::TYPE_PICTURE_IN_PICTURE:
      // For now, picture in picture windows can be treated like popups.
      return sessions::SessionWindow::TYPE_POPUP;
    case BrowserWindowInterface::TYPE_APP:
      return sessions::SessionWindow::TYPE_APP;
    case BrowserWindowInterface::TYPE_DEVTOOLS:
      return sessions::SessionWindow::TYPE_DEVTOOLS;
    case BrowserWindowInterface::TYPE_APP_POPUP:
      return sessions::SessionWindow::TYPE_APP_POPUP;
  }
  NOTREACHED();
}

BrowserWindowInterface::Type BrowserTypeForWindowType(
    sessions::SessionWindow::WindowType type) {
  switch (type) {
    case sessions::SessionWindow::TYPE_NORMAL:
      return BrowserWindowInterface::TYPE_NORMAL;
    case sessions::SessionWindow::TYPE_POPUP:
      return BrowserWindowInterface::TYPE_POPUP;
    case sessions::SessionWindow::TYPE_APP:
      return BrowserWindowInterface::TYPE_APP;
    case sessions::SessionWindow::TYPE_DEVTOOLS:
      return BrowserWindowInterface::TYPE_DEVTOOLS;
    case sessions::SessionWindow::TYPE_APP_POPUP:
      return BrowserWindowInterface::TYPE_APP_POPUP;
  }
  NOTREACHED();
}
