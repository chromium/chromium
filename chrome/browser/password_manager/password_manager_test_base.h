// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_MANAGER_TEST_BASE_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_MANAGER_TEST_BASE_H_

#include <memory>

#include "base/macros.h"
#include "base/run_loop.h"
#include "chrome/browser/ssl/cert_verifier_browser_test.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/password_manager/core/browser/password_store_consumer.h"
#include "content/public/browser/web_contents_observer.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace autofill {
struct PasswordForm;
}

class ManagePasswordsUIController;

class NavigationObserver : public content::WebContentsObserver {
 public:
  explicit NavigationObserver(content::WebContents* web_contents);
  ~NavigationObserver() override;

  // Normally Wait() will not return until a main frame navigation occurs.
  // If a path is set, Wait() will return after this path has been seen,
  // regardless of the frame that navigated. Useful for multi-frame pages.
  void SetPathToWaitFor(const std::string& path) { wait_for_path_ = path; }

  // Normally Wait() will not return until a main frame navigation occurs.
  // If quit_on_entry_committed is true Wait() will return on EntryCommited.
  void set_quit_on_entry_committed(bool quit_on_entry_committed) {
    quit_on_entry_committed_ = quit_on_entry_committed;
  }

  // Wait for navigation to succeed.
  void Wait();

  // Returns the RenderFrameHost that navigated.
  content::RenderFrameHost* render_frame_host() { return render_frame_host_; }

  // content::WebContentsObserver:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidFinishLoad(content::RenderFrameHost* render_frame_host,
                     const GURL& validated_url) override;

 private:
  std::string wait_for_path_;
  content::RenderFrameHost* render_frame_host_;
  bool quit_on_entry_committed_;
  base::RunLoop run_loop_;

  DISALLOW_COPY_AND_ASSIGN(NavigationObserver);
};

// Checks the save password prompt for a specified WebContents and allows
// accepting saving passwords through it.
class BubbleObserver {
 public:
  // The constructor doesn't start tracking |web_contents|. To check the status
  // of the prompt one can even construct a temporary BubbleObserver.
  explicit BubbleObserver(content::WebContents* web_contents);

  // Checks if the save prompt is being currently available due to either manual
  // fallback or successful login.
  bool IsSavePromptAvailable() const;

  // Checks if the update prompt is being currently available due to either
  // manual fallback or successful login.
  bool IsUpdatePromptAvailable() const;

  // Checks if the save prompt was shown automatically.
  // |web_contents| must be the custom one returned by
  // PasswordManagerBrowserTestBase.
  bool IsSavePromptShownAutomatically() const;

  // Checks if the update prompt was shown automatically.
  // |web_contents| must be the custom one returned by
  // PasswordManagerBrowserTestBase.
  bool IsUpdatePromptShownAutomatically() const;

  // Hide the currently open prompt.
  void Hide() const;

  // Expecting that the prompt is available, saves the password. At the end,
  // checks that the prompt is no longer available afterwards.
  void AcceptSavePrompt() const;

  // Expecting that the prompt is shown, update |form| with the password from
  // observed form. Checks that the prompt is no longer visible afterwards.
  void AcceptUpdatePrompt(const autofill::PasswordForm& form) const;

  // Returns once the account chooser pops up or it's already shown.
  // |web_contents| must be the custom one returned by
  // PasswordManagerBrowserTestBase.
  void WaitForAccountChooser() const;

  // Returns once the UI controller is in inactive state.
  // |web_contents| must be the custom one returned by
  // PasswordManagerBrowserTestBase.
  void WaitForInactiveState() const;

  // Returns once the UI controller is in the management state due to matching
  // credentials autofilled.
  // |web_contents| must be the custom one returned by
  // PasswordManagerBrowserTestBase.
  void WaitForManagementState() const;

  // Returns once the save prompt pops up or it's already shown.
  // |web_contents| must be the custom one returned by
  // PasswordManagerBrowserTestBase.
  void WaitForAutomaticSavePrompt() const;

  // Returns once the update prompt pops up or it's already shown.
  // |web_contents| must be the custom one returned by
  // PasswordManagerBrowserTestBase.
  void WaitForAutomaticUpdatePrompt() const;

  // Returns true if the browser shows the fallback for saving password within
  // the allotted timeout.
  // |web_contents| must be the custom one returned by
  // PasswordManagerBrowserTestBase.
  bool WaitForFallbackForSaving(
      const base::TimeDelta timeout = base::TimeDelta::Max()) const;

 private:
  ManagePasswordsUIController* const passwords_ui_controller_;

  DISALLOW_COPY_AND_ASSIGN(BubbleObserver);
};

class PasswordManagerBrowserTestBase : public CertVerifierBrowserTest {
 public:
  PasswordManagerBrowserTestBase();
  ~PasswordManagerBrowserTestBase() override;

  // InProcessBrowserTest:
  void SetUpOnMainThread() override;
  void TearDownOnMainThread() override;

  // Bring up a new Chrome tab set up with password manager test hooks.
  // @param[in] browser the browser running the password manager test, upon
  // which this function will perform the setup steps.
  // @param[out] a new tab on the browser set up with password manager test
  // hooks.
  static void SetUpOnMainThreadAndGetNewTab(
      Browser* browser,
      content::WebContents** web_contents);
  // Make sure that the password store associated with the given browser
  // processed all the previous calls, calls executed on another thread.
  static void WaitForPasswordStore(Browser* browser);

 protected:
  // Wrapper around ui_test_utils::NavigateToURL that waits until
  // DidFinishLoad() fires. Normally this function returns after
  // DidStopLoading(), which caused flakiness as the NavigationObserver
  // would sometimes see the DidFinishLoad event from a previous navigation and
  // return immediately.
  void NavigateToFile(const std::string& path);

  // Waits until the "value" attribute of the HTML element with |element_id| is
  // equal to |expected_value|. If the current value is not as expected, this
  // waits until the "change" event is fired for the element. This also
  // guarantees that once the real value matches the expected, the JavaScript
  // event loop is spun to allow all other possible events to take place.
  // WARNING:
  // - the function waits only for the first "onchange" event.
  // - "onchange" event is triggered by autofill. However, if user's typing is
  // simulated then the event is triggered only when control looses focus.
  void WaitForElementValue(const std::string& element_id,
                           const std::string& expected_value);
  // Same as above except the element |element_id| is in iframe |iframe_id|
  void WaitForElementValue(const std::string& iframe_id,
                           const std::string& element_id,
                           const std::string& expected_value);

  // Same as above except the element has index |element_index| in elements() of
  // the form |form_id|.
  void WaitForElementValue(const std::string& form_id,
                           size_t element_index,
                           const std::string& expected_value);

  // Same as above except the element is selected with |element_selector| JS
  // expression.
  void WaitForJsElementValue(const std::string& element_selector,
                             const std::string& expected_value);

  // Make sure that the password store processed all the previous calls which
  // are executed on another thread.
  void WaitForPasswordStore();
  // Checks that the current "value" attribute of the HTML element with
  // |element_id| is equal to |expected_value|.
  void CheckElementValue(const std::string& element_id,
                         const std::string& expected_value);
  // Same as above except the element |element_id| is in iframe |iframe_id|
  void CheckElementValue(const std::string& iframe_id,
                         const std::string& element_id,
                         const std::string& expected_value);

  // Synchronoulsy adds the given host to the list of valid HSTS hosts.
  void AddHSTSHost(const std::string& host);

  // Checks that |password_store| stores only one credential with |username| and
  // |password|.
  void CheckThatCredentialsStored(const std::string& username,
                                  const std::string& password);

  // Accessors
  // Return the first created tab with a custom ManagePasswordsUIController.
  content::WebContents* WebContents() const;
  content::RenderFrameHost* RenderFrameHost() const;
  net::EmbeddedTestServer& https_test_server() { return https_test_server_; }

 private:
  net::EmbeddedTestServer https_test_server_;
  // A tab with some hooks injected.
  content::WebContents* web_contents_;
  DISALLOW_COPY_AND_ASSIGN(PasswordManagerBrowserTestBase);
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_MANAGER_TEST_BASE_H_
