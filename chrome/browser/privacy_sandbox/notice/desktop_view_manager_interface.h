// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_PRIVACY_SANDBOX_NOTICE_DESKTOP_VIEW_MANAGER_INTERFACE_H_
#define CHROME_BROWSER_PRIVACY_SANDBOX_NOTICE_DESKTOP_VIEW_MANAGER_INTERFACE_H_

#include <optional>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/privacy_sandbox/notice/notice.mojom-forward.h"

class BrowserWindowInterface;

namespace privacy_sandbox {

// This class will:
// 1. Manage the showing, hiding and closing of notices in the correct order on
// the desktop side.
// 2. Advance multi-step notices
// 3. Manage sticky behavior of notices across tabs
class DesktopViewManagerInterface {
 public:
  class Observer {
   public:
    // Fired whenever observers are required to proceed to the next step.
    virtual void MaybeNavigateToNextStep(
        std::optional<notice::mojom::PrivacySandboxNotice> next_id) = 0;

    virtual BrowserWindowInterface* GetBrowser() = 0;
  };

  virtual ~DesktopViewManagerInterface();

  // Called by navigation handler when a suitable URL has
  // been found. All suitable URLs are chrome-owned.
  virtual void HandleChromeOwnedPageNavigation(
      BrowserWindowInterface* browser_interface) = 0;

  // Triggered by the WebUI handler once an event occurs on a |notice|.
  virtual void OnEventOccurred(
      notice::mojom::PrivacySandboxNotice notice,
      notice::mojom::PrivacySandboxNoticeEvent event) = 0;

  virtual void AddObserver(Observer* observer) = 0;

  virtual void RemoveObserver(Observer* observer) = 0;
};
}  // namespace privacy_sandbox

#endif  // CHROME_BROWSER_PRIVACY_SANDBOX_NOTICE_DESKTOP_VIEW_MANAGER_INTERFACE_H_
