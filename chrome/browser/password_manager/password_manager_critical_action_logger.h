// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_MANAGER_CRITICAL_ACTION_LOGGER_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_MANAGER_CRITICAL_ACTION_LOGGER_H_

#include "base/memory/raw_ptr.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "content/public/browser/web_contents_observer.h"

namespace content {
class WebContents;
}

class Profile;

namespace password_manager {

class PasswordManagerDriver;

// A helper class that handles Google Password Manager (GPM) critical action
// logging. It encapsulates navigation observation and logs critical actions via
// CriticalActionService.
class PasswordManagerCriticalActionLogger
    : public content::WebContentsObserver {
 public:
  PasswordManagerCriticalActionLogger(content::WebContents* web_contents,
                                      Profile* profile);

  PasswordManagerCriticalActionLogger(
      const PasswordManagerCriticalActionLogger&) = delete;
  PasswordManagerCriticalActionLogger& operator=(
      const PasswordManagerCriticalActionLogger&) = delete;

  ~PasswordManagerCriticalActionLogger() override;

  // Logs critical actions if the feature is enabled.
  void MaybeLogCriticalAction(
      PasswordManagerDriver* driver,
      const GURL& url,
      PasswordManagerClient::PasswordFillTrigger trigger_type);

 private:
  // content::WebContentsObserver:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void RenderFrameDeleted(content::RenderFrameHost* render_frame_host) override;

  int64_t GetNavigationIdForDriver(PasswordManagerDriver* driver) const;

  const raw_ptr<Profile> profile_;
};

}  // namespace password_manager

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_MANAGER_CRITICAL_ACTION_LOGGER_H_
