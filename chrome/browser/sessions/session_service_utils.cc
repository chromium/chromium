// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sessions/session_service_utils.h"

#include "build/chromeos_buildflags.h"

sessions::SessionWindow::WindowType WindowTypeForBrowserType(
    Browser::Type type) {
  switch (type) {
    case Browser::TYPE_NORMAL:
      return sessions::SessionWindow::TYPE_NORMAL;
    case Browser::TYPE_POPUP:
      return sessions::SessionWindow::TYPE_POPUP;
    case Browser::TYPE_PICTURE_IN_PICTURE:
      // For now, picture in picture windows can be treated like popups.
      return sessions::SessionWindow::TYPE_POPUP;
    case Browser::TYPE_APP:
      return sessions::SessionWindow::TYPE_APP;
    case Browser::TYPE_DEVTOOLS:
      return sessions::SessionWindow::TYPE_DEVTOOLS;
    case Browser::TYPE_APP_POPUP:
      return sessions::SessionWindow::TYPE_APP_POPUP;
#if BUILDFLAG(IS_CHROMEOS_ASH)
    case Browser::TYPE_CUSTOM_TAB:
      return sessions::SessionWindow::TYPE_CUSTOM_TAB;
#endif
  }
  NOTREACHED_IN_MIGRATION();
  return sessions::SessionWindow::TYPE_NORMAL;
}

Browser::Type BrowserTypeForWindowType(
    sessions::SessionWindow::WindowType type) {
  switch (type) {
    case sessions::SessionWindow::TYPE_NORMAL:
      return Browser::TYPE_NORMAL;
    case sessions::SessionWindow::TYPE_POPUP:
      return Browser::TYPE_POPUP;
    case sessions::SessionWindow::TYPE_APP:
      return Browser::TYPE_APP;
    case sessions::SessionWindow::TYPE_DEVTOOLS:
      return Browser::TYPE_DEVTOOLS;
    case sessions::SessionWindow::TYPE_APP_POPUP:
      return Browser::TYPE_APP_POPUP;
#if BUILDFLAG(IS_CHROMEOS_ASH)
    case sessions::SessionWindow::TYPE_CUSTOM_TAB:
      return Browser::TYPE_CUSTOM_TAB;
#endif
  }
  NOTREACHED_IN_MIGRATION();
  return Browser::TYPE_NORMAL;
}
