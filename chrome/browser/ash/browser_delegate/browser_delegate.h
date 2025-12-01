// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_BROWSER_DELEGATE_BROWSER_DELEGATE_H_
#define CHROME_BROWSER_ASH_BROWSER_DELEGATE_BROWSER_DELEGATE_H_

#include "chrome/browser/ash/browser_delegate/browser_type.h"
#include "components/account_id/account_id.h"
#include "components/sessions/core/session_id.h"
#include "components/webapps/common/web_app_id.h"
#include "ui/gfx/geometry/rect.h"

class Browser;
class GURL;

namespace aura {
class Window;
}  // namespace aura

namespace content {
class WebContents;
}  // namespace content

namespace tab_groups {
struct TabGroupInfo;
}  // namespace tab_groups

namespace ui {
class BaseWindow;
}  // namespace ui

namespace ash {

// Abstraction of the `Browser` class from chrome/browser/ui/browser.h for use
// by ChromeOS feature code. See README.md.
class BrowserDelegate {
 public:
  // Returns the underlying raw browser instance.
  // NOTE: This function is here only temporarily to facilitate transitioning
  // code from Browser to BrowserDelegate incrementally. See also
  // BrowserController::GetDelegate.
  virtual Browser& GetBrowser() const = 0;

  // Returns the browser's type.
  virtual BrowserType GetType() const = 0;

  // Returns the browser's unique ID for the current session.
  virtual SessionID GetSessionID() const = 0;

  // Returns the account id associated with the browser. In production, this id
  // should always be valid (see AccountId::is_valid).
  virtual const AccountId& GetAccountId() const = 0;

  // Returns whether the browser is off the record, i.e. incognito or in a guest
  // session.
  virtual bool IsOffTheRecord() const = 0;

  // Returns the browser window's current bounds.
  virtual gfx::Rect GetBounds() const = 0;

  // Returns the active contents. Can be nullptr, e.g. when the tab strip is
  // being initialized or destroyed.
  virtual content::WebContents* GetActiveWebContents() const = 0;

  // Returns the number of web contents.
  virtual size_t GetWebContentsCount() const = 0;

  // Returns the contents for the given index, or nullptr if out of bounds. Can
  // be nullptr even if index is in bounds, just like GetActiveWebContents().
  virtual content::WebContents* GetWebContentsAt(size_t index) const = 0;

  // Returns the inspected web contents if this is a kDevTools type browser.
  // Returns nullptr otherwise.
  // Can also be nullptr while the browser is initialized/shutdown.
  virtual content::WebContents* GetInspectedWebContents() const = 0;

  // Returns the window. Can be nullptr, e.g. when the browser is being
  // closed.
  virtual ui::BaseWindow* GetWindow() const = 0;

  // Returns the native window. Can be nullptr, e.g. when the browser is being
  // closed.
  virtual aura::Window* GetNativeWindow() const = 0;

  // Returns the non-empty browser application id, if applicable.
  virtual std::optional<webapps::AppId> GetAppId() const = 0;

  // Returns whether the browser is a web app window/pop-up.
  virtual bool IsWebApp() const = 0;

  // Returns true during the initial phase of the browser being closed, when
  // `beforeunload` handlers are running (async). It may be aborted.
  virtual bool IsAttemptingToClose() const = 0;

  // Returns whether the browser is in the process of being closed and deleted.
  // In this phase, closing committed, browser is hidden and deletion is
  // scheduled. It cannot be aborted.
  virtual bool IsClosing() const = 0;

  // Returns whether the browser window is active.
  virtual bool IsActive() const = 0;

  // Returns whether the browser window is minimized.
  virtual bool IsMinimized() const = 0;

  // Returns whether the browser window is visible.
  virtual bool IsVisible() const = 0;

  // Shows the browser window, or activates it if it's already visible.
  virtual void Show() = 0;

  // Shows the window, but does not activate it. Does nothing if the window is
  // already visible.
  virtual void ShowInactive() = 0;

  // Activates the browser window.
  virtual void Activate() = 0;

  // Minimizes the browser window.
  virtual void Minimize() = 0;

  // Closes the browser as soon as possible.
  virtual void Close() = 0;

  // Loads the given URL in a new tab.
  // If the `url` is empty the new tab-page is loaded.
  // If an `index` is given, the tab is placed at the corresponding position in
  // the tab strip. Otherwise it is added to the end.
  enum class TabDisposition { kForeground, kBackground };
  virtual void AddTab(const GURL& url,
                      std::optional<size_t> index,
                      TabDisposition disposition) = 0;

  // Closes the contents at the given index, triggering its destruction.
  // If UserGesture::kYes is given, the contents will first be marked as closed
  // by user gesture.
  enum class UserGesture { kYes, kNo };
  virtual void CloseWebContentsAt(size_t index, UserGesture user_gesture) = 0;

  // Navigates the browser to the given URL.
  // The browser must be of `kApp` or `kAppPopup` type.
  // In the case of a tabbed web app (e.g. ChromeOS Terminal), performs tab
  // pinning as requested and ensures that home tab URL navigation happens in
  // the home tab.
  enum class TabPinning { kYes, kNo };
  virtual content::WebContents* NavigateWebApp(const GURL& url,
                                               TabPinning pin_tab) = 0;

  // Creates the specified tab group.
  virtual void CreateTabGroup(const tab_groups::TabGroupInfo& tab_group) = 0;

  // Pins the given tab.
  virtual void PinTab(size_t tab_index) = 0;

  // Moves the given tab to the given `target_browser`, where it's placed at the
  // end of the tab strip.
  virtual void MoveTab(size_t tab_index, BrowserDelegate& target_browser) = 0;

  // Initiates user install of a WebApp for the current page.
  virtual bool CreateWebAppFromActiveWebContents() = 0;

 protected:
  ~BrowserDelegate() = default;

 private:
  // BrowserDelegateImpl assumes it's the only implementation.
  BrowserDelegate() = default;
  friend class BrowserDelegateImpl;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_BROWSER_DELEGATE_BROWSER_DELEGATE_H_
