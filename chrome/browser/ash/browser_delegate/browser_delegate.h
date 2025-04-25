// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_BROWSER_DELEGATE_BROWSER_DELEGATE_H_
#define CHROME_BROWSER_ASH_BROWSER_DELEGATE_BROWSER_DELEGATE_H_

#include "chrome/browser/ash/browser_delegate/browser_type.h"
#include "components/sessions/core/session_id.h"
#include "ui/gfx/geometry/rect.h"

class Browser;

namespace aura {
class Window;
}  // namespace aura

namespace content {
class WebContents;
}  // namespace content

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

  // Returns the browser window's current bounds.
  virtual gfx::Rect GetBounds() const = 0;

  // Returns the active contents. Can be nullptr, e.g. when the tab strip is
  // being initialized or destroyed.
  virtual content::WebContents* GetActiveWebContents() const = 0;

  // Returns the native window. Can be nullptr, e.g. when the browser is being
  // closed.
  virtual aura::Window* GetNativeWindow() const = 0;

  // Returns whether the browser is in the process of being closed and deleted.
  virtual bool IsClosing() const = 0;

  // Shows the browser window, or activates it if it's already visible.
  virtual void Show() = 0;

  // Minimizes the browser window.
  virtual void Minimize() = 0;

  // Closes the browser as soon as possible.
  virtual void Close() = 0;

 protected:
  ~BrowserDelegate() = default;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_BROWSER_DELEGATE_BROWSER_DELEGATE_H_
