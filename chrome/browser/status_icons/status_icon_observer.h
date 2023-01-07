// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_STATUS_ICONS_STATUS_ICON_OBSERVER_H_
#define CHROME_BROWSER_STATUS_ICONS_STATUS_ICON_OBSERVER_H_

#include "build/build_config.h"

class StatusIconObserver {
 public:
  // Called when the user clicks on the system tray icon. Clicks that result
  // in the context menu being displayed will not be passed to this observer
  // (i.e. if there's a context menu set on this status icon, and the user
  // right clicks on the icon to display the context menu, OnStatusIconClicked()
  // will not be called).
  // Note: Chrome OS displays the context menu on left button clicks.
  // This will only be fired for this platform if no context menu is present.
  virtual void OnStatusIconClicked() = 0;

#if BUILDFLAG(IS_WIN)
  // Called when the user clicks on a balloon generated for a system tray icon.
  // TODO(dewittj): Implement on platforms other than Windows.  Currently this
  // event will never fire on non-Windows platforms.
  virtual void OnBalloonClicked() {}
#endif

 protected:
  virtual ~StatusIconObserver() {}
};

#endif  // CHROME_BROWSER_STATUS_ICONS_STATUS_ICON_OBSERVER_H_
