// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_PASSWORDS_NAVIGATION_OBSERVER_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_PASSWORDS_NAVIGATION_OBSERVER_H_

#include <string>
#include <vector>

#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "components/password_manager/core/browser/password_manager_interface.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/browser_test_utils.h"

class PasswordsNavigationObserver
    : public content::WebContentsObserver,
      public password_manager::PasswordManagerInterface::Observer {
 public:
  explicit PasswordsNavigationObserver(content::WebContents* web_contents);

  PasswordsNavigationObserver(const PasswordsNavigationObserver&) = delete;
  PasswordsNavigationObserver& operator=(const PasswordsNavigationObserver&) =
      delete;

  ~PasswordsNavigationObserver() override;

  // Normally Wait() will not return until a main frame navigation occurs.
  // If a path is set, Wait() will return after this path has been seen,
  // regardless of the frame that navigated. Useful for multi-frame pages.
  void SetPathToWaitFor(const std::string& path) { wait_for_path_ = path; }

  // Normally Wait() will not return until a main frame navigation occurs.
  // If quit_on_entry_committed is true Wait() will return on EntryCommitted.
  void set_quit_on_entry_committed(bool quit_on_entry_committed) {
    quit_on_entry_committed_ = quit_on_entry_committed;
  }

  // If set to true, Wait() will only return after password forms have been
  // parsed on the page.
  void set_wait_for_password_forms_parsed(bool wait) {
    wait_for_password_forms_parsed_ = wait;
  }

  // Wait for navigation. Returns true on success, false otherwise (e.g. on
  // timeout).
  [[nodiscard]] bool Wait();

  // content::WebContentsObserver:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidFinishLoad(content::RenderFrameHost* render_frame_host,
                     const GURL& validated_url) override;

  // password_manager::PasswordManagerInterface::Observer:
  void OnLoginSuccessful(
      const password_manager::PasswordForm& pending_form) override {}
  void OnPasswordFormsParsed(
      password_manager::PasswordManagerDriver* driver,
      const std::vector<autofill::FormData>& forms_data) override;

 private:
  std::string wait_for_path_;
  bool quit_on_entry_committed_ = false;
  bool wait_for_password_forms_parsed_ = false;
  bool password_forms_parsed_ = false;
  bool wait_for_forms_after_load_ = false;
  content::WaiterHelper waiter_helper_;

  base::ScopedObservation<password_manager::PasswordManagerInterface,
                          password_manager::PasswordManagerInterface::Observer>
      password_manager_observation_{this};
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_PASSWORDS_NAVIGATION_OBSERVER_H_
