// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_NETWORK_NETWORK_PORTAL_SIGNIN_WINDOW_H_
#define CHROME_BROWSER_CHROMEOS_NETWORK_NETWORK_PORTAL_SIGNIN_WINDOW_H_

#include <memory>

#include "base/no_destructor.h"
#include "components/sessions/core/session_id.h"
#include "url/gurl.h"

class Browser;
class NetworkPortalSigninWindowLacrosBrowserTest;
class NetworkPortalSigninWindowAshBrowserTest;

namespace content {
class WebContents;
}

namespace chromeos {

// Class controlling the captive portal signin window used in Ash and Lacros
// when the CaptivePortalPopupWindow feature is enabled.
// See also the NetworkPortalSigninController class.
class NetworkPortalSigninWindow {
 public:
  static NetworkPortalSigninWindow* Get();

  NetworkPortalSigninWindow(const NetworkPortalSigninWindow&) = delete;
  NetworkPortalSigninWindow& operator=(const NetworkPortalSigninWindow&) =
      delete;
  ~NetworkPortalSigninWindow();

  // Shows the signin window.
  void Show(const GURL& url);

  Browser* GetBrowserForTesting();
  content::WebContents* GetWebContentsForTesting();

 protected:
  friend class base::NoDestructor<NetworkPortalSigninWindow>;
  friend class NetworkPortalSigninWindowLacrosBrowserTest;
  friend class NetworkPortalSigninWindowAshBrowserTest;
  NetworkPortalSigninWindow();

  int portal_detection_requested_for_testing() const {
    return portal_detection_requested_for_testing_;
  }

 private:
  class WindowObserver;

  SessionID window_session_id_{SessionID::InvalidValue()};
  std::unique_ptr<WindowObserver> window_observer_;
  int portal_detection_requested_for_testing_ = 0;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_NETWORK_NETWORK_PORTAL_SIGNIN_WINDOW_H_
