// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_BROWSER_DELEGATE_BROWSER_CONTROLLER_H_
#define CHROME_BROWSER_ASH_BROWSER_DELEGATE_BROWSER_CONTROLLER_H_

#include <string_view>

#include "base/containers/span.h"
#include "base/functional/function_ref.h"
#include "base/observer_list_types.h"
#include "chrome/browser/ash/browser_delegate/browser_type.h"
#include "components/webapps/common/web_app_id.h"
#include "url/gurl.h"

class AccountId;
class BrowserWindowInterface;

namespace aura {
class Window;
}  // namespace aura

namespace content {
class WebContents;
}  // namespace content

namespace ash {

class BrowserDelegate;

// BrowserController is a singleton created by
// ChromeBrowserMainPartsAsh::PostProfileInit. See also README.md.
class BrowserController {
 public:
  // See AddObserver below.
  class Observer : public base::CheckedObserver {
   public:
    // Called when a browser is created.
    // `browser` is never nullptr.
    // Note: When invoking BrowserController::ForEachBrowser in
    // OnBrowserCreated, the new browser will show up for
    // kAscendingCreationTime but not yet for kAscendingActivationTime.
    // TODO(crbug.com/369688254): Revisit this behavior.
    virtual void OnBrowserCreated(BrowserDelegate* browser) {}

    // Called when a browser is activated.
    // `browser` is never nullptr.
    virtual void OnBrowserActivated(BrowserDelegate* browser) {}

    // Called when a browser is closed.
    // `browser` is never nullptr.
    virtual void OnBrowserClosed(BrowserDelegate* browser) {}

    // Called when the last browser is irrevocably being closed.
    // TODO(crbug.com/369689187): Figure out if/how we want to allow inspection
    // of the browser (the instance still exists but we shouldn't allow
    // arbitrary operations).
    virtual void OnLastBrowserClosed() {}
  };

  // See CreateWebApp below.
  struct CreateParams {
    bool allow_resize;
    bool allow_maximize;
    bool allow_fullscreen;
    // TODO(crbug.com/369689187): Figure out if the restore_id field makes
    // sense, and if so, add a description.
    int32_t restore_id;
  };

  // See ForEachBrowser below.
  enum class BrowserOrder {
    kAscendingCreationTime,
    kAscendingActivationTime,
  };
  using enum BrowserOrder;

  // See ForEachBrowser below.
  enum class IterationDirective {
    kContinueIteration,
    kBreakIteration,
  };
  using enum IterationDirective;

  static BrowserController* GetInstance();

  // Returns the corresponding delegate, possibly creating it first.
  // Returns nullptr for a nullptr input.
  // NOTE: This function is here only temporarily to facilitate transitioning
  // code from BrowserWindowInterface to BrowserDelegate incrementally. See also
  // BrowserDelegate::GetBrowser.
  virtual BrowserDelegate* GetDelegate(BrowserWindowInterface* bwi) = 0;

  // Returns (the delegate for) the most recently used browser that still
  // exists. Returns nullptr if there's none.
  virtual BrowserDelegate* GetLastUsedBrowser() = 0;

  // Returns (the delegate for) the most recently used browser that is
  // currently visible. Returns nullptr if there's none.
  virtual BrowserDelegate* GetLastUsedVisibleBrowser() = 0;

  // Returns (the delegate for) the most recently used browser that is
  // currently visible and on-the-record. Returns nullptr if there's none.
  virtual BrowserDelegate* GetLastUsedVisibleOnTheRecordBrowser() = 0;

  // Iterates over (the delegates for) the currently existing browsers in the
  // given order, invoking the callback for each. The callback can terminate the
  // iteration early by returning kBreakIteration.
  virtual void ForEachBrowser(
      BrowserOrder order,
      base::FunctionRef<IterationDirective(BrowserDelegate&)> callback) = 0;

  // Returns (the delegate for) the browser associated with the given native
  // window, if any. This can be nullptr when the browser is shutting down.
  virtual BrowserDelegate* GetBrowserForWindow(aura::Window* window) = 0;

  // Returns (the delegate for) the browser associated with the given tab, if
  // any. This can be nullptr when the tab is in the process of being moved from
  // one browser to another.
  virtual BrowserDelegate* GetBrowserForTab(content::WebContents* contents) = 0;

  // Returns (the delegate for) the most recently activated web app browser
  // that matches the given parameters. Returns nullptr if there's none.
  // Url matching is done ignoring any references, and only if `url` is not
  // empty.
  // The `browser_type` must be kApp or kAppPopup.
  virtual BrowserDelegate* FindWebApp(const AccountId& account_id,
                                      webapps::AppId app_id,
                                      BrowserType browser_type,
                                      const GURL& url = GURL()) = 0;

  // Makes a POST request in a new tab in the last active tabbed browser. If no
  // such browser exists, a new one is created. Returns nullptr if the creation
  // is not possible for the given arguments.
  // This is needed by the Media app.
  virtual BrowserDelegate* NewTabWithPostData(
      const AccountId& account_id,
      const GURL& url,
      base::span<const uint8_t> post_data,
      std::string_view extra_headers) = 0;

  // Creates a web app browser for the given parameters.
  // The `browser_type` must be kApp or kAppPopup. In the case of kApp, a pinned
  // home tab is added if that feature is supported and a URL is registered for
  // the app.
  // Returns nullptr if the creation is not possible for the given arguments.
  virtual BrowserDelegate* CreateWebApp(const AccountId& account_id,
                                        webapps::AppId app_id,
                                        BrowserType browser_type,
                                        const CreateParams& params) = 0;

  // Creates a "custom tab" browser with the given contents.
  // TODO(crbug.com/369689187): This is a special kind of popup only used by
  // ARC. It's based on the Browser::TYPE_CUSTOM_TAB type that only exists on
  // ChromeOS. Consider getting rid of this special type.
  virtual BrowserDelegate* CreateCustomTab(
      const AccountId& account_id,
      std::unique_ptr<content::WebContents> contents) = 0;

  // Facilitates observation of browser events.
  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  // Encapsulates the creation of AutofillClient instances.
  virtual void CreateAutofillClientForWebContents(
      content::WebContents* web_contents) = 0;

 protected:
  BrowserController();
  BrowserController(const BrowserController&) = delete;
  BrowserController& operator=(const BrowserController&) = delete;
  virtual ~BrowserController();
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_BROWSER_DELEGATE_BROWSER_CONTROLLER_H_
