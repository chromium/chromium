// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_CAPTURED_SITES_TEST_UTILS_H_
#define CHROME_BROWSER_AUTOFILL_CAPTURED_SITES_TEST_UTILS_H_

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "chrome/browser/ui/browser.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/browser_test_utils.h"
#include "services/network/public/cpp/network_switches.h"

namespace content {
class RenderFrameHost;
class WebContents;
}  // namespace content

namespace captured_sites_test_utils {

// The amount of time to wait for an action to complete, or for a page element
// to appear. The Captured Site Automation Framework uses this timeout to break
// out of wait loops in the event that
// 1. A page load error occurred and the page does not have a page element
//    the test expects. Test should stop waiting.
// 2. A page contains persistent animation (such as a flash sale count down
//    timer) that causes the framework's paint-based PageActivityObserver to
//    wait indefinitely. Test should stop waiting if a sufficiently large time
//    has expired for the page to load or for the page to respond to the last
//    user action.
const base::TimeDelta default_action_timeout = base::TimeDelta::FromSeconds(30);
// The amount of time to wait for a page to trigger a paint in response to a
// an ation. The Captured Site Automation Framework uses this timeout to
// break out of a wait loop after a hover action.
const base::TimeDelta visual_update_timeout = base::TimeDelta::FromSeconds(20);

std::string FilePathToUTF8(const base::FilePath::StringType& str);

// PageActivityObserver
//
// PageActivityObserver is a universal wait-for-page-ready object that ensures
// the current web page finishes responding to user input. The Captured Site
// Automation Framework, specifically the TestRecipeReplayer class, uses
// PageActivityObserver to time delays between two test actions. Without the
// delay, a test may break itself by performing a page action before the page
// is ready to receive the action.
//
// For example, the Amazon.com checkout page runs background scripts after
// loading. While running the background scripts, the checkout page displays
// a spinner. If a user clicks on a link while the spinner is present,
// Amazon.com will dispatch the user to an error page.
//
// Page readiness is hard to determine because on real-world sites, page
// readiness does not correspond to page load events. In the above Amazon.com
// example, the checkout page starts the background scripts after the page
// finishes loading.
//
// The PageActivityObserver defines page ready as the absence of Chrome paint
// events. On real-world sites, if a page is busy loading, the Chrome tab
// should be busy and Chrome should continuously make layout changes and
// repaint the page. If a site is busy doing background work, most pages
// typically display some form of persistent animation such as a progress bar
// or a spinner to tell the user that the page is not ready. Therefore, it
// is reasonable to assume that a page is ready if Chrome finished painting.
class PageActivityObserver : public content::WebContentsObserver {
 public:
  explicit PageActivityObserver(content::WebContents* web_contents);
  explicit PageActivityObserver(content::RenderFrameHost* frame);
  ~PageActivityObserver() override = default;

  // Wait until Chrome finishes loading a page and updating the page's visuals.
  // If Chrome finishes loading a page but continues to paint every half
  // second, exit after |continuous_paint_timeout| expires since Chrome
  // finished loading the page.
  void WaitTillPageIsIdle(
      base::TimeDelta continuous_paint_timeout = default_action_timeout);
  // Wait until Chrome makes at least 1 visual update, or until timeout
  // expires.
  bool WaitForVisualUpdate(base::TimeDelta timeout = visual_update_timeout);

 private:
  // PageActivityObserver determines if Chrome stopped painting by checking if
  // Chrome hasn't painted for a specific amount of time.
  // kPaintEventCheckInterval defines this amount of time.
  static constexpr base::TimeDelta kPaintEventCheckInterval =
      base::TimeDelta::FromMilliseconds(500);

  // content::WebContentsObserver:
  void DidCommitAndDrawCompositorFrame() override;

  bool paint_occurred_during_last_loop_ = false;

  DISALLOW_COPY_AND_ASSIGN(PageActivityObserver);
};

// IFrameWaiter
//
// IFrameWaiter is an waiter object that waits for an iframe befitting a
// criteria to appear. The criteria can be the iframe's 'name' attribute,
// the iframe's origin, or the iframe's full url.
class IFrameWaiter : public content::WebContentsObserver {
 public:
  explicit IFrameWaiter(content::WebContents* webcontents);
  ~IFrameWaiter() override;
  content::RenderFrameHost* WaitForFrameMatchingName(
      const std::string& name,
      const base::TimeDelta timeout = default_action_timeout);
  content::RenderFrameHost* WaitForFrameMatchingOrigin(
      const GURL origin,
      const base::TimeDelta timeout = default_action_timeout);
  content::RenderFrameHost* WaitForFrameMatchingUrl(
      const GURL url,
      const base::TimeDelta timeout = default_action_timeout);

 private:
  enum QueryType { NAME, ORIGIN, URL };

  static bool FrameHasOrigin(const GURL& origin,
                             content::RenderFrameHost* frame);

  // content::WebContentsObserver
  void RenderFrameCreated(content::RenderFrameHost* render_frame_host) override;
  void DidFinishLoad(content::RenderFrameHost* render_frame_host,
                     const GURL& validated_url) override;
  void FrameNameChanged(content::RenderFrameHost* render_frame_host,
                        const std::string& name) override;

  QueryType query_type_;
  base::RunLoop run_loop_;
  content::RenderFrameHost* target_frame_;
  std::string frame_name_;
  GURL origin_;
  GURL url_;

  DISALLOW_COPY_AND_ASSIGN(IFrameWaiter);
};

// TestRecipeReplayChromeFeatureActionExecutor
//
// TestRecipeReplayChromeFeatureActionExecutor is a helper interface. A
// TestRecipeReplayChromeFeatureActionExecutor class implements functions
// that automate Chrome feature behavior. TestRecipeReplayer calls
// TestRecipeReplayChromeFeatureActionExecutor functions to execute actions
// that involves a Chrome feature - such as Chrome Autofill or Chrome
// Password Manager. Executing a Chrome feature action typically require
// using private or protected hooks defined inside that feature's
// InProcessBrowserTest class. By implementing this interface an
// InProcessBrowserTest exposes its feature to captured site automation.
class TestRecipeReplayChromeFeatureActionExecutor {
 public:
  // Chrome Autofill feature methods.
  // Triggers Chrome Autofill in the specified input element on the specified
  // document.
  virtual bool AutofillForm(content::RenderFrameHost* frame,
                            const std::string& focus_element_css_selector,
                            const int attempts = 1);
  virtual bool AddAutofillProfileInfo(const std::string& field_type,
                                      const std::string& field_value);
  virtual bool SetupAutofillProfile();
  // Chrome Password Manager feature methods.
  virtual bool AddCredential(const std::string& origin,
                             const std::string& username,
                             const std::string& password);
  virtual bool SavePassword();
  virtual bool UpdatePassword();
  virtual bool HasChromeShownSavePasswordPrompt();
  virtual bool HasChromeStoredCredential(const std::string& origin,
                                         const std::string& username,
                                         const std::string& password);

 protected:
  TestRecipeReplayChromeFeatureActionExecutor();
  ~TestRecipeReplayChromeFeatureActionExecutor();

  DISALLOW_COPY_AND_ASSIGN(TestRecipeReplayChromeFeatureActionExecutor);
};

// TestRecipeReplayer
//
// The TestRecipeReplayer object drives Captured Site Automation by
// 1. Providing a set of functions that help an InProcessBrowserTest to
//    configure, start and stop a Web Page Replay (WPR) server. A WPR server
//    is a local server that intercepts and responds to Chrome requests with
//    pre-recorded traffic. Using a captured site archive file, WPR can
//    mimick the site server and provide the test with deterministic site
//    behaviors.
// 2. Providing a function that deserializes and replays a Test Recipe. A Test
//    Recipe is a JSON formatted file containing instructions on how to run a
//    Chrome test against a live or captured site. These instructions include
//    the starting URL for the test, and a list of user actions (clicking,
//    typing) that drives the test. One may sample some example Test Recipes
//    under the src/chrome/test/data/autofill/captured_sites directory.
class TestRecipeReplayer {
 public:
  static const int kHostHttpPort = 8080;
  static const int kHostHttpsPort = 8081;

  TestRecipeReplayer(
      Browser* browser,
      TestRecipeReplayChromeFeatureActionExecutor* feature_action_executor);
  ~TestRecipeReplayer();
  void Setup();
  void Cleanup();

  // Replay a test by:
  // 1. Starting a WPR server using the specified capture file.
  // 2. Replaying the specified Test Recipe file.
  bool ReplayTest(const base::FilePath capture_file_path,
                  const base::FilePath recipe_file_path);

  static void SetUpCommandLine(base::CommandLine* command_line);
  static bool PlaceFocusOnElement(content::RenderFrameHost* frame,
                                  const std::string& element_xpath);
  static bool GetCenterCoordinateOfTargetElement(
      content::RenderFrameHost* frame,
      const std::string& target_element_xpath,
      int& x,
      int& y);
  static bool SimulateLeftMouseClickAt(
      content::RenderFrameHost* render_frame_host,
      const gfx::Point& point);
  static bool SimulateMouseHoverAt(content::RenderFrameHost* render_frame_host,
                                   const gfx::Point& point);

 private:
  Browser* browser();
  TestRecipeReplayChromeFeatureActionExecutor* feature_action_executor();
  content::WebContents* GetWebContents();
  void CleanupSiteData();
  bool StartWebPageReplayServer(const base::FilePath& capture_file_path);
  bool StopWebPageReplayServer();
  bool InstallWebPageReplayServerRootCert();
  bool RemoveWebPageReplayServerRootCert();
  bool RunWebPageReplayCmdAndWaitForExit(
      const std::string& cmd,
      const std::vector<std::string>& args,
      const base::TimeDelta& timeout = base::TimeDelta::FromSeconds(5));
  bool RunWebPageReplayCmd(const std::string& cmd,
                           const std::vector<std::string>& args,
                           base::Process* process);
  bool ReplayRecordedActions(const base::FilePath& recipe_file_path);
  bool InitializeBrowserToExecuteRecipe(
      std::unique_ptr<base::DictionaryValue>& recipe);
  bool ExecuteAutofillAction(const base::DictionaryValue& action);
  bool ExecuteClickAction(const base::DictionaryValue& action);
  bool ExecuteHoverAction(const base::DictionaryValue& action);
  bool ExecutePressEnterAction(const base::DictionaryValue& action);
  bool ExecuteRunCommandAction(const base::DictionaryValue& action);
  bool ExecuteSavePasswordAction(const base::DictionaryValue& action);
  bool ExecuteSelectDropdownAction(const base::DictionaryValue& action);
  bool ExecuteTypeAction(const base::DictionaryValue& action);
  bool ExecuteTypePasswordAction(const base::DictionaryValue& action);
  bool ExecuteUpdatePasswordAction(const base::DictionaryValue& action);
  bool ExecuteValidateFieldValueAction(const base::DictionaryValue& action);
  bool ExecuteValidateNoSavePasswordPromptAction(
      const base::DictionaryValue& action);
  bool ExecuteWaitForStateAction(const base::DictionaryValue& action);
  bool GetTargetHTMLElementXpathFromAction(const base::DictionaryValue& action,
                                           std::string* xpath);
  bool GetTargetFrameFromAction(const base::DictionaryValue& action,
                                content::RenderFrameHost** frame);
  bool WaitForElementToBeReady(content::RenderFrameHost* frame,
                               const std::string& xpath);
  bool WaitForStateChange(
      content::RenderFrameHost* frame,
      const std::vector<std::string>& state_assertions,
      const base::TimeDelta& timeout = default_action_timeout);
  bool AllAssertionsPassed(const content::ToRenderFrameHost& frame,
                           const std::vector<std::string>& assertions);
  bool ExecuteJavaScriptOnElementByXpath(
      const content::ToRenderFrameHost& frame,
      const std::string& element_xpath,
      const std::string& execute_function_body,
      const base::TimeDelta& time_to_wait_for_element = default_action_timeout);
  bool ExpectElementPropertyEquals(
      const content::ToRenderFrameHost& frame,
      const std::string& element_xpath,
      const std::string& get_property_function_body,
      const std::string& expected_value,
      const bool ignoreCase = false);
  void NavigateAwayAndDismissBeforeUnloadDialog();
  bool HasChromeStoredCredential(const base::DictionaryValue& action,
                                 bool* stored_cred);
  bool SetupSavedAutofillProfile(
      const base::Value& saved_autofill_profile_container);
  bool SetupSavedPasswords(const base::Value& saved_password_list_container);

  Browser* browser_;
  TestRecipeReplayChromeFeatureActionExecutor* feature_action_executor_;

  // The Web Page Replay server that serves the captured sites.
  base::Process web_page_replay_server_;

  DISALLOW_COPY_AND_ASSIGN(TestRecipeReplayer);
};

}  // namespace captured_sites_test_utils

#endif  // CHROME_BROWSER_AUTOFILL_CAPTURED_SITES_TEST_UTILS_H_
