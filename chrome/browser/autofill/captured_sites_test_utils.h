// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_CAPTURED_SITES_TEST_UTILS_H_
#define CHROME_BROWSER_AUTOFILL_CAPTURED_SITES_TEST_UTILS_H_

#include <fstream>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/strcat.h"
#include "base/types/strong_alias.h"
#include "base/values.h"
#include "chrome/browser/ui/browser.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/test_autofill_clock.h"
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
//    timer) that causes the RenderFrameHost count to never diminish completely.
//    Test should stop waiting if a sufficiently large time has expired for the
//    page to load or for the page to respond to the last user action.
const base::TimeDelta default_action_timeout = base::Seconds(30);
// The amount of time to wait for a page to trigger a paint in response to a
// an action. The Captured Site Automation Framework uses this timeout to
// break out of a wait loop after a hover action.
const base::TimeDelta visual_update_timeout = base::Seconds(20);
// The amount of time to do a precheck on the page before going to a click
// fallback action.
const base::TimeDelta click_fallback_timeout = base::Seconds(5);
// When we cause a scroll event, make sure we give the page a moment to react
// before continuing.
const base::TimeDelta scroll_wait_timeout = base::Seconds(2);
// Some times, tests tend to need a break that can't be read from the elements
// play status.
const base::TimeDelta cool_off_action_timeout = base::Seconds(1);
// The time to wait between checks for a page to become idle or active based on
// the loading status and then the render frame count.
const base::TimeDelta wait_for_idle_loop_length = base::Milliseconds(500);

std::string FilePathToUTF8(const base::FilePath::StringType& str);

enum ExpectedResult { kPass, kFail };
struct CapturedSiteParams {
  std::string scenario_dir;
  std::string site_name;
  std::optional<int> bug_number;
  ExpectedResult expectation = kPass;
  bool is_disabled = false;
  base::FilePath capture_file_path;
  base::FilePath recipe_file_path;
  base::FilePath refresh_file_path;

  CapturedSiteParams();
  ~CapturedSiteParams();
  CapturedSiteParams(const CapturedSiteParams& other);
};

std::ostream& operator<<(std::ostream& os, const CapturedSiteParams& data);

std::vector<CapturedSiteParams> GetCapturedSites(
    const base::FilePath& replay_files_dir_path);

struct GetParamAsString {
  template <class ParamType>
  std::string operator()(const testing::TestParamInfo<ParamType>& info) const {
    if (info.param.scenario_dir.empty())
      return info.param.site_name;
    return base::StrCat({info.param.scenario_dir, "_", info.param.site_name});
  }
};

std::optional<base::FilePath> GetCommandFilePath();

// Prints tips on how to run captured-site tests.
// |test_file_name| should be without the .cc suffix.
void PrintInstructions(const char* test_file_name);

// IFrameWaiter
//
// IFrameWaiter is an waiter object that waits for an iframe befitting a
// criteria to appear. The criteria can be the iframe's 'name' attribute,
// the iframe's origin, or the iframe's full url.
class IFrameWaiter : public content::WebContentsObserver {
 public:
  explicit IFrameWaiter(content::WebContents* webcontents);

  IFrameWaiter(const IFrameWaiter&) = delete;
  IFrameWaiter& operator=(const IFrameWaiter&) = delete;

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
  raw_ptr<content::RenderFrameHost> target_frame_;
  std::string frame_name_;
  GURL origin_;
  GURL url_;
};

// WebPageReplayServerWrapper

// WebPageReplayServerWrapper is a helper wrapper that controls the configuring
// and running the WebPageReplay Server instance.
class WebPageReplayServerWrapper {
 public:
  explicit WebPageReplayServerWrapper(const bool start_as_replay,
                                      int hostHttpPort = 8080,
                                      int hostHttpsPort = 8081);

  WebPageReplayServerWrapper(const WebPageReplayServerWrapper&) = delete;
  WebPageReplayServerWrapper& operator=(const WebPageReplayServerWrapper&) =
      delete;

  ~WebPageReplayServerWrapper();

  bool Start(const base::FilePath& capture_file_path);
  bool Stop();

 private:
  bool RunWebPageReplayCmdAndWaitForExit(
      const std::vector<std::string>& args,
      const base::TimeDelta& timeout = base::Seconds(5));
  bool RunWebPageReplayCmd(const std::vector<std::string>& args);

  std::string cmd_name() { return start_as_replay_ ? "replay" : "record"; }
  // The Web Page Replay server that serves the captured sites.
  base::Process web_page_replay_server_;

  int host_http_port_;
  int host_https_port_;
  bool start_as_replay_;
};

class ProfileDataController {
 public:
  ProfileDataController();
  ~ProfileDataController();

  const autofill::CreditCard& credit_card() const { return card_; }
  const autofill::AutofillProfile& profile() { return profile_; }
  bool AddAutofillProfileInfo(const std::string& field_type,
                              const std::string& field_value);
  std::optional<std::u16string> cvc() const { return cvc_; }

 private:
  // If a CVC is available in the Action Recorder receipt, this test uses a
  // server card to autofill the payment form. So the "Enter CVC" dialog will
  // pop up for card autofill. Otherwise, this test uses a local card to
  // autofill the payment form.
  std::optional<std::u16string> cvc_;
  autofill::AutofillProfile profile_;
  autofill::CreditCard card_;
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
  TestRecipeReplayChromeFeatureActionExecutor(
      const TestRecipeReplayChromeFeatureActionExecutor&) = delete;
  TestRecipeReplayChromeFeatureActionExecutor& operator=(
      const TestRecipeReplayChromeFeatureActionExecutor&) = delete;

  // Chrome Autofill feature methods.
  // Triggers Chrome Autofill in the specified input element on the specified
  // document.
  virtual bool AutofillForm(
      const std::string& focus_element_css_selector,
      const std::vector<std::string>& iframe_path,
      const int attempts,
      content::RenderFrameHost* frame,
      std::optional<autofill::FieldType> triggered_field_type);
  virtual bool AddAutofillProfileInfo(const std::string& field_type,
                                      const std::string& field_value);
  virtual bool SetupAutofillProfile();
  // Chrome Password Manager feature methods.
  virtual bool AddCredential(const std::string& origin,
                             const std::string& username,
                             const std::string& password);
  virtual bool SavePassword();
  virtual bool UpdatePassword();
  virtual bool WaitForSaveFallback();
  virtual bool IsChromeShowingPasswordGenerationPrompt();
  virtual bool HasChromeShownSavePasswordPrompt();
  virtual bool HasChromeStoredCredential(const std::string& origin,
                                         const std::string& username,
                                         const std::string& password);

 protected:
  TestRecipeReplayChromeFeatureActionExecutor();
  ~TestRecipeReplayChromeFeatureActionExecutor();
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
  static constexpr int kHostHttpPort = 8080;
  static constexpr int kHostHttpsPort = 8081;
  static constexpr int kHostHttpRecordPort = 8082;
  static constexpr int kHostHttpsRecordPort = 8083;

  enum DomElementReadyState {
    kReadyStatePresent = 0,
    kReadyStateVisible = 1 << 0,
    kReadyStateEnabled = 1 << 1,
    kReadyStateOnTop   = 1 << 2
  };

  TestRecipeReplayer(
      Browser* browser,
      TestRecipeReplayChromeFeatureActionExecutor* feature_action_executor);

  TestRecipeReplayer(const TestRecipeReplayer&) = delete;
  TestRecipeReplayer& operator=(const TestRecipeReplayer&) = delete;

  ~TestRecipeReplayer();

  // Replay a test by:
  // 1. Starting a WPR server using the specified capture file.
  // 2. Replaying the specified Test Recipe file.
  bool ReplayTest(const base::FilePath& capture_file_path,
                  const base::FilePath& recipe_file_path,
                  const std::optional<base::FilePath>& command_file_path);

  const std::vector<testing::AssertionResult> GetValidationFailures() const;

  static void SetUpCommandLine(base::CommandLine* command_line);
  static void SetUpHostResolverRules(base::CommandLine* command_line);
  static bool ScrollElementIntoView(const std::string& element_xpath,
                                    content::RenderFrameHost* frame);
  static bool PlaceFocusOnElement(const std::string& element_xpath,
                                  const std::vector<std::string>& iframe_path,
                                  content::RenderFrameHost* frame);
  static bool GetBoundingRectOfTargetElement(
      const std::string& target_element_xpath,
      const std::vector<std::string>& iframe_path,
      content::RenderFrameHost* frame,
      gfx::Rect* output_rect);
  static bool SimulateLeftMouseClickAt(
      const gfx::Point& point,
      content::RenderFrameHost* render_frame_host);
  static bool SimulateMouseHoverAt(content::RenderFrameHost* render_frame_host,
                                   const gfx::Point& point);

 private:
  using IgnoreCase = base::StrongAlias<struct IgnoreCaseTag, bool>;

  static bool GetIFrameOffsetFromIFramePath(
      const std::vector<std::string>& iframe_path,
      content::RenderFrameHost* frame,
      gfx::Vector2d* offset);
  static bool GetBoundingRectOfTargetElement(
      const std::string& target_element_xpath,
      content::RenderFrameHost* frame,
      gfx::Rect* output_rect);

  Browser* browser();

  TestRecipeReplayChromeFeatureActionExecutor* feature_action_executor();
  WebPageReplayServerWrapper* web_page_replay_server_wrapper();
  content::WebContents* GetWebContents();
  void CleanupSiteData();
  bool StartWebPageReplayServer(base::Process* web_page_replay_server,
                                const base::FilePath& capture_file_path,
                                const bool start_as_replay = true);
  bool StopWebPageReplayServer(base::Process* web_page_replay_server);
  bool ReplayRecordedActions(
      const base::FilePath& recipe_file_path,
      const std::optional<base::FilePath>& command_file_path);
  bool InitializeBrowserToExecuteRecipe(base::Value::Dict& recipe);
  bool ExecuteAutofillAction(base::Value::Dict action);
  bool ExecuteClickAction(base::Value::Dict action);
  bool ExecuteClickIfNotSeenAction(base::Value::Dict action);
  bool ExecuteCoolOffAction(base::Value::Dict action);
  bool ExecuteCloseTabAction(base::Value::Dict action);
  bool ExecuteHoverAction(base::Value::Dict action);
  bool ExecuteForceLoadPage(base::Value::Dict action);
  bool ExecutePressEnterAction(base::Value::Dict action);
  bool ExecutePressEscapeAction(base::Value::Dict action);
  bool ExecutePressSpaceAction(base::Value::Dict action);
  bool ExecuteRunCommandAction(base::Value::Dict action);
  bool ExecuteSavePasswordAction(base::Value::Dict action);
  bool ExecuteSelectDropdownAction(base::Value::Dict action);
  bool ExecuteTypeAction(base::Value::Dict action);
  bool ExecuteTypePasswordAction(base::Value::Dict action);
  bool ExecuteUpdatePasswordAction(base::Value::Dict action);
  bool ExecuteValidateFieldValueAction(base::Value::Dict action);
  bool ExecuteValidateNoSavePasswordPromptAction(base::Value::Dict action);
  bool ExecuteValidatePasswordGenerationPromptAction(base::Value::Dict action);
  bool ExecuteValidateSaveFallbackAction(base::Value::Dict action);
  bool ExecuteWaitForStateAction(base::Value::Dict action);
  bool GetTargetHTMLElementXpathFromAction(const base::Value::Dict& action,
                                           std::string* xpath);
  bool GetTargetFrameFromAction(const base::Value::Dict& action,
                                content::RenderFrameHost** frame);
  bool GetIFramePathFromAction(const base::Value::Dict& action,
                               std::vector<std::string>* iframe_path);
  bool GetTargetHTMLElementVisibilityEnumFromAction(
      const base::Value::Dict& action,
      int* visibility_enum_val);
  bool ExtractFrameAndVerifyElement(const base::Value::Dict& action,
                                    std::string* xpath,
                                    content::RenderFrameHost** frame,
                                    bool set_focus = false,
                                    bool relaxed_visibility = false,
                                    bool ignore_failure = false);
  void ValidatePasswordGenerationPromptState(
      const content::ToRenderFrameHost& frame,
      const std::string& element_xpath,
      bool expect_to_be_shown);
  bool WaitForElementToBeReady(const std::string& xpath,
                               const int visibility_enum_val,
                               content::RenderFrameHost* frame,
                               bool ignore_failure = false);
  bool WaitForStateChange(
      content::RenderFrameHost* frame,
      const std::vector<std::string>& state_assertions,
      const base::TimeDelta& timeout = default_action_timeout,
      bool ignore_failure = false);
  bool AllAssertionsPassed(const content::ToRenderFrameHost& frame,
                           const std::vector<std::string>& assertions);
  bool ExecuteJavaScriptOnElementByXpath(
      const content::ToRenderFrameHost& frame,
      const std::string& element_xpath,
      const std::string& execute_function_body,
      const base::TimeDelta& time_to_wait_for_element = default_action_timeout);
  bool GetElementProperty(const content::ToRenderFrameHost& frame,
                          const std::string& element_xpath,
                          const std::string& get_property_function_body,
                          std::string* property);
  bool ExpectElementPropertyEqualsAnyOf(
      const content::ToRenderFrameHost& frame,
      const std::string& element_xpath,
      const std::string& get_property_function_body,
      const std::vector<std::string>& expected_value,
      const std::string& validation_field,
      IgnoreCase ignore_case = IgnoreCase(false));
  void SimulateKeyPressWrapper(content::WebContents* web_contents,
                               ui::DomKey key);
  bool HasChromeStoredCredential(const base::Value::Dict& action,
                                 bool* stored_cred);
  bool OverrideAutofillClock(const base::FilePath capture_file_path);
  bool SetupSavedAutofillProfile(
      base::Value::List saved_autofill_profile_container);
  bool SetupSavedPasswords(base::Value::List saved_password_list_container);

  // Wait until Chrome finishes loading a page and updating the page's visuals.
  // If Chrome finishes loading a page but continues to paint every half
  // second, exit after |continuous_paint_timeout| expires since Chrome
  // finished loading the page.
  void WaitTillPageIsIdle(
      base::TimeDelta continuous_paint_timeout = default_action_timeout);
  // Wait until Chrome makes at least 1 visual update, or until timeout
  // expires. Returns false if no visual update is observed before the given
  // timeout elapses.
  bool WaitForVisualUpdate(base::TimeDelta timeout = visual_update_timeout);

  raw_ptr<Browser> browser_;
  raw_ptr<TestRecipeReplayChromeFeatureActionExecutor> feature_action_executor_;
  // The Web Page Replay server that serves the captured sites.
  std::unique_ptr<captured_sites_test_utils::WebPageReplayServerWrapper>
      web_page_replay_server_wrapper_ =
          std::make_unique<WebPageReplayServerWrapper>(true);

  std::vector<testing::AssertionResult> validation_failures_;

  // Overrides the AutofillClock to use the recorded date.
  autofill::TestAutofillClock test_clock_;
};

}  // namespace captured_sites_test_utils

#endif  // CHROME_BROWSER_AUTOFILL_CAPTURED_SITES_TEST_UTILS_H_
