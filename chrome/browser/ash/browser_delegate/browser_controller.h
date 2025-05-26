// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_BROWSER_DELEGATE_BROWSER_CONTROLLER_H_
#define CHROME_BROWSER_ASH_BROWSER_DELEGATE_BROWSER_CONTROLLER_H_

#include <string_view>

#include "base/containers/span.h"
#include "chrome/browser/ash/browser_delegate/browser_type.h"
#include "components/webapps/common/web_app_id.h"
#include "url/gurl.h"

class Browser;

namespace content {
class WebContents;
}  // namespace content

namespace user_manager {
class User;
}  // namespace user_manager

namespace ash {

class BrowserDelegate;

// BrowserController is a singleton created by
// ChromeBrowserMainExtraPartsAsh::PostProfileInit. See also README.md.
class BrowserController {
 public:
  static BrowserController* GetInstance();

  // Returns the corresponding delegate, possibly creating it first.
  // Returns nullptr for a nullptr input.
  // NOTE: This function is here only temporarily to facilitate transitioning
  // code from Browser to BrowserDelegate incrementally. See also
  // BrowserDelegate::GetBrowser.
  virtual BrowserDelegate* GetDelegate(Browser* browser) = 0;

  // Returns (the delegate for) the most recently used browser that is
  // currently visible. Returns nullptr if there's none.
  virtual BrowserDelegate* GetLastUsedVisibleBrowser() = 0;

  // Returns (the delegate for) the most recently used browser that is
  // currently visible and on-the-record. Returns nullptr if there's none.
  virtual BrowserDelegate* GetLastUsedVisibleOnTheRecordBrowser() = 0;

  // Returns (the delegate for) the most recently activated web app browser
  // that matches the given parameters. Returns nullptr if there's none.
  // Url matching is done ignoring any references, and only if `url` is not
  // empty.
  // The `browser_type` must be kApp or kAppPopup.
  virtual BrowserDelegate* FindWebApp(const user_manager::User& user,
                                      webapps::AppId app_id,
                                      BrowserType browser_type,
                                      const GURL& url = GURL()) = 0;

  // Makes a POST request in a new tab in the last active tabbed browser. If no
  // such browser exists, a new one is created. Returns nullptr if the creation
  // is not possible for the given arguments.
  // This is needed by the Media app.
  virtual BrowserDelegate* NewTabWithPostData(
      const user_manager::User& user,
      const GURL& url,
      base::span<const uint8_t> post_data,
      std::string_view extra_headers) = 0;

  // Creates a web app browser for the given parameters.
  // The `browser_type` must be kApp or kAppPopup. In the case of kApp, a pinned
  // home tab is added if that feature is supported and a URL is registered for
  // the app.
  // Returns nullptr if the creation is not possible for the given arguments.
  struct CreateParams {
    bool allow_resize;
    bool allow_maximize;
    bool allow_fullscreen;
    // TODO(crbug.com/369689187): Figure out if the restore_id field makes
    // sense, and if so, add a description.
    int32_t restore_id;
  };
  virtual BrowserDelegate* CreateWebApp(const user_manager::User& user,
                                        webapps::AppId app_id,
                                        BrowserType browser_type,
                                        const CreateParams& params) = 0;

  // Creates a "custom tab" browser with the given contents.
  // TODO(crbug.com/369689187): This is a special kind of popup only used by
  // ARC. It's based on the Browser::TYPE_CUSTOM_TAB type that only exists on
  // ChromeOS. Consider getting rid of this special type.
  virtual BrowserDelegate* CreateCustomTab(
      const user_manager::User& user,
      std::unique_ptr<content::WebContents> contents) = 0;

 protected:
  BrowserController();
  BrowserController(const BrowserController&) = delete;
  BrowserController& operator=(const BrowserController&) = delete;
  virtual ~BrowserController();
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_BROWSER_DELEGATE_BROWSER_CONTROLLER_H_
