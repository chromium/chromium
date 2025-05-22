// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_PRIVACY_SANDBOX_NOTICE_DESKTOP_ENTRYPOINT_HANDLERS_H_
#define CHROME_BROWSER_PRIVACY_SANDBOX_NOTICE_DESKTOP_ENTRYPOINT_HANDLERS_H_

#include "content/public/browser/navigation_handle.h"

class Profile;
class BrowserWindowInterface;

namespace privacy_sandbox {

// This class defines shared methods used aross all entrypoints.
class EntryPointHandler {
 public:
  explicit EntryPointHandler(
      base::RepeatingCallback<void(BrowserWindowInterface*)>
          entry_point_callback);
  virtual ~EntryPointHandler();

  // Alerts callback location that entrypoint checks have passed and that a view
  // may show.
  void HandleEntryPoint(BrowserWindowInterface* browser_interface);

 protected:
  // Called when we want to inform the callback location a valid entrypoint has
  // been encountered.
  base::RepeatingCallback<void(BrowserWindowInterface*)> entry_point_callback_;
};

// This class handles view manager entrypoints triggered by new navigation
// events.
class NavigationHandler : public EntryPointHandler {
 public:
  explicit NavigationHandler(
      base::RepeatingCallback<void(BrowserWindowInterface*)>
          entry_point_callback);

  // Performs checks required to determine whether a view can be shown on a
  // navigation.
  void HandleNewNavigation(content::NavigationHandle* navigation_handle,
                           Profile* profile);
};

}  // namespace privacy_sandbox

#endif  // CHROME_BROWSER_PRIVACY_SANDBOX_NOTICE_DESKTOP_ENTRYPOINT_HANDLERS_H_
