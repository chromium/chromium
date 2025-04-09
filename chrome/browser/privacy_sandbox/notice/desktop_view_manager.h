// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_PRIVACY_SANDBOX_NOTICE_DESKTOP_VIEW_MANAGER_H_
#define CHROME_BROWSER_PRIVACY_SANDBOX_NOTICE_DESKTOP_VIEW_MANAGER_H_

#include "base/observer_list.h"
#include "chrome/browser/privacy_sandbox/notice/notice.mojom-forward.h"
#include "chrome/browser/privacy_sandbox/notice/notice_service_interface.h"

namespace privacy_sandbox {

class PrivacySandboxNoticeServiceInterface;

// This class will:
// 1. Manage the showing, hiding and closing of notices in the correct order on
// the desktop side.
// 2. Advance multi-step notices
// 3. Manage sticky behavior of notices across tabs
class DesktopViewManager {
 public:
  explicit DesktopViewManager(
      PrivacySandboxNoticeServiceInterface* notice_service);
  virtual ~DesktopViewManager();

  class Observer {
   public:
    // Fired whenever observers are required to proceed to the next step.
    virtual void MaybeNavigateToNextStep(
        std::optional<notice::mojom::PrivacySandboxNotice> next_id) {}
  };

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 private:
  // TODO(chrstne): Create a member variable for notice_service when it gets
  // used.
  base::ObserverList<Observer>::Unchecked observers_;
};

}  // namespace privacy_sandbox

#endif  // CHROME_BROWSER_PRIVACY_SANDBOX_NOTICE_DESKTOP_VIEW_MANAGER_H_
