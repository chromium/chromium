// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/external_protocol/external_protocol_handler.h"

#include "base/run_loop.h"
#include "base/values.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

class FakeExternalProtocolHandlerWorker
    : public shell_integration::DefaultProtocolClientWorker {
 public:
  FakeExternalProtocolHandlerWorker(
      const shell_integration::DefaultWebClientWorkerCallback& callback,
      const std::string& protocol,
      shell_integration::DefaultWebClientState os_state)
      : shell_integration::DefaultProtocolClientWorker(callback, protocol),
        os_state_(os_state) {}

 private:
  ~FakeExternalProtocolHandlerWorker() override = default;

  shell_integration::DefaultWebClientState CheckIsDefaultImpl() override {
    return os_state_;
  }

  void SetAsDefaultImpl(const base::Closure& on_finished_callback) override {
    on_finished_callback.Run();
  }

  shell_integration::DefaultWebClientState os_state_;
};

class FakeExternalProtocolHandlerDelegate
    : public ExternalProtocolHandler::Delegate {
 public:
  explicit FakeExternalProtocolHandlerDelegate(base::OnceClosure on_complete)
      : block_state_(ExternalProtocolHandler::BLOCK),
        os_state_(shell_integration::UNKNOWN_DEFAULT),
        has_launched_(false),
        has_prompted_(false),
        has_blocked_(false),
        on_complete_(std::move(on_complete)) {}

  scoped_refptr<shell_integration::DefaultProtocolClientWorker>
  CreateShellWorker(
      const shell_integration::DefaultWebClientWorkerCallback& callback,
      const std::string& protocol) override {
    return new FakeExternalProtocolHandlerWorker(callback, protocol, os_state_);
  }

  ExternalProtocolHandler::BlockState GetBlockState(const std::string& scheme,
                                                    Profile* profile) override {
    return block_state_;
  }

  void BlockRequest() override {
    EXPECT_TRUE(block_state_ == ExternalProtocolHandler::BLOCK ||
                os_state_ == shell_integration::IS_DEFAULT);
    has_blocked_ = true;
    if (on_complete_)
      std::move(on_complete_).Run();
  }

  void RunExternalProtocolDialog(
      const GURL& url,
      content::WebContents* web_contents,
      ui::PageTransition page_transition,
      bool has_user_gesture,
      const base::Optional<url::Origin>& initiating_origin) override {
    EXPECT_EQ(block_state_, ExternalProtocolHandler::UNKNOWN);
    EXPECT_NE(os_state_, shell_integration::IS_DEFAULT);
    has_prompted_ = true;
    launch_or_prompt_url_ = url;
    initiating_origin_ = initiating_origin;
  }

  void LaunchUrlWithoutSecurityCheck(
      const GURL& url,
      content::WebContents* web_contents) override {
    EXPECT_EQ(block_state_, ExternalProtocolHandler::DONT_BLOCK);
    EXPECT_NE(os_state_, shell_integration::IS_DEFAULT);
    has_launched_ = true;
    launch_or_prompt_url_ = url;
  }

  void FinishedProcessingCheck() override {
    if (on_complete_)
      std::move(on_complete_).Run();
  }

  void set_os_state(shell_integration::DefaultWebClientState value) {
    os_state_ = value;
  }

  void set_block_state(ExternalProtocolHandler::BlockState value) {
    block_state_ = value;
  }

  bool has_launched() { return has_launched_; }
  bool has_prompted() { return has_prompted_; }
  bool has_blocked() { return has_blocked_; }
  const base::Optional<url::Origin>& initiating_origin() {
    return initiating_origin_;
  }

  const std::string& launch_or_prompt_url() {
    return launch_or_prompt_url_.spec();
  }

 private:
  ExternalProtocolHandler::BlockState block_state_;
  shell_integration::DefaultWebClientState os_state_;
  bool has_launched_;
  bool has_prompted_;
  bool has_blocked_;
  GURL launch_or_prompt_url_;
  base::Optional<url::Origin> initiating_origin_;
  base::OnceClosure on_complete_;
};

class ExternalProtocolHandlerTest : public testing::Test {
 protected:
  ExternalProtocolHandlerTest() : delegate_(run_loop_.QuitClosure()) {}

  void SetUp() override {
    profile_ = std::make_unique<TestingProfile>();
    rvh_test_enabler_ = std::make_unique<content::RenderViewHostTestEnabler>();
    web_contents_ = content::WebContentsTester::CreateTestWebContents(
        profile_.get(), nullptr);
  }

  void TearDown() override {
    // Ensure that g_accept_requests gets set back to true after test execution.
    ExternalProtocolHandler::PermitLaunchUrl();
    TestingBrowserProcess::GetGlobal()->SetLocalState(nullptr);
  }

  enum class Action { PROMPT, LAUNCH, BLOCK };

  void DoTest(ExternalProtocolHandler::BlockState block_state,
              shell_integration::DefaultWebClientState os_state,
              Action expected_action) {
    DoTest(block_state, os_state, expected_action,
           GURL("mailto:test@test.com"));
  }

  void DoTest(ExternalProtocolHandler::BlockState block_state,
              shell_integration::DefaultWebClientState os_state,
              Action expected_action,
              const GURL& url) {
    url::Origin initiating_origin =
        url::Origin::Create(GURL("https://example.test"));
    EXPECT_FALSE(delegate_.has_prompted());
    EXPECT_FALSE(delegate_.has_launched());
    EXPECT_FALSE(delegate_.has_blocked());
    ExternalProtocolHandler::SetDelegateForTesting(&delegate_);
    delegate_.set_block_state(block_state);
    delegate_.set_os_state(os_state);
    int process_id = web_contents_->GetRenderViewHost()->GetProcess()->GetID();
    int routing_id = web_contents_->GetRenderViewHost()->GetRoutingID();
    ExternalProtocolHandler::LaunchUrl(url, process_id, routing_id,
                                       ui::PAGE_TRANSITION_LINK, true,
                                       initiating_origin);
    run_loop_.Run();
    ExternalProtocolHandler::SetDelegateForTesting(nullptr);

    EXPECT_EQ(expected_action == Action::PROMPT, delegate_.has_prompted());
    EXPECT_EQ(expected_action == Action::LAUNCH, delegate_.has_launched());
    EXPECT_EQ(expected_action == Action::BLOCK, delegate_.has_blocked());
    if (expected_action == Action::PROMPT) {
      ASSERT_TRUE(delegate_.initiating_origin().has_value());
      EXPECT_EQ(initiating_origin, delegate_.initiating_origin().value());
    } else {
      EXPECT_FALSE(delegate_.initiating_origin().has_value());
    }
  }

  content::BrowserTaskEnvironment task_environment_;

  base::RunLoop run_loop_;
  FakeExternalProtocolHandlerDelegate delegate_;

  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<content::RenderViewHostTestEnabler> rvh_test_enabler_;
  std::unique_ptr<content::WebContents> web_contents_;
};

TEST_F(ExternalProtocolHandlerTest, TestLaunchSchemeBlockedChromeDefault) {
  DoTest(ExternalProtocolHandler::BLOCK, shell_integration::IS_DEFAULT,
         Action::BLOCK);
}

TEST_F(ExternalProtocolHandlerTest, TestLaunchSchemeBlockedChromeNotDefault) {
  DoTest(ExternalProtocolHandler::BLOCK, shell_integration::NOT_DEFAULT,
         Action::BLOCK);
}

TEST_F(ExternalProtocolHandlerTest, TestLaunchSchemeBlockedChromeUnknown) {
  DoTest(ExternalProtocolHandler::BLOCK, shell_integration::UNKNOWN_DEFAULT,
         Action::BLOCK);
}

TEST_F(ExternalProtocolHandlerTest,
       TestLaunchSchemeBlockedChromeOtherModeDefault) {
  DoTest(ExternalProtocolHandler::BLOCK,
         shell_integration::OTHER_MODE_IS_DEFAULT, Action::BLOCK);
}

TEST_F(ExternalProtocolHandlerTest, TestLaunchSchemeUnBlockedChromeDefault) {
  DoTest(ExternalProtocolHandler::DONT_BLOCK, shell_integration::IS_DEFAULT,
         Action::BLOCK);
}

TEST_F(ExternalProtocolHandlerTest, TestLaunchSchemeUnBlockedChromeNotDefault) {
  DoTest(ExternalProtocolHandler::DONT_BLOCK, shell_integration::NOT_DEFAULT,
         Action::LAUNCH);
}

TEST_F(ExternalProtocolHandlerTest, TestLaunchSchemeUnBlockedChromeUnknown) {
  DoTest(ExternalProtocolHandler::DONT_BLOCK,
         shell_integration::UNKNOWN_DEFAULT, Action::LAUNCH);
}

TEST_F(ExternalProtocolHandlerTest,
       TestLaunchSchemeUnBlockedChromeOtherModeDefault) {
  DoTest(ExternalProtocolHandler::DONT_BLOCK,
         shell_integration::OTHER_MODE_IS_DEFAULT, Action::LAUNCH);
}

TEST_F(ExternalProtocolHandlerTest, TestLaunchSchemeUnknownChromeDefault) {
  DoTest(ExternalProtocolHandler::UNKNOWN, shell_integration::IS_DEFAULT,
         Action::BLOCK);
}

TEST_F(ExternalProtocolHandlerTest, TestLaunchSchemeUnknownChromeNotDefault) {
  DoTest(ExternalProtocolHandler::UNKNOWN, shell_integration::NOT_DEFAULT,
         Action::PROMPT);
}

TEST_F(ExternalProtocolHandlerTest, TestLaunchSchemeUnknownChromeUnknown) {
  DoTest(ExternalProtocolHandler::UNKNOWN, shell_integration::UNKNOWN_DEFAULT,
         Action::PROMPT);
}

TEST_F(ExternalProtocolHandlerTest,
       TestLaunchSchemeUnknownChromeOtherModeDefault) {
  DoTest(ExternalProtocolHandler::UNKNOWN,
         shell_integration::OTHER_MODE_IS_DEFAULT, Action::PROMPT);
}

TEST_F(ExternalProtocolHandlerTest, TestUrlEscape) {
  GURL url("alert:test message\" --bad%2B\r\n 文本 \"file");
  DoTest(ExternalProtocolHandler::UNKNOWN, shell_integration::NOT_DEFAULT,
         Action::PROMPT, url);
  // Expect that the "\r\n" has been removed, and all other illegal URL
  // characters have been escaped.
  EXPECT_EQ("alert:test%20message%22%20--bad%2B%20%E6%96%87%E6%9C%AC%20%22file",
            delegate_.launch_or_prompt_url());
}

TEST_F(ExternalProtocolHandlerTest, TestGetBlockStateUnknown) {
  ExternalProtocolHandler::BlockState block_state =
      ExternalProtocolHandler::GetBlockState("tel", profile_.get());
  EXPECT_EQ(ExternalProtocolHandler::UNKNOWN, block_state);
  EXPECT_TRUE(
      profile_->GetPrefs()->GetDictionary(prefs::kExcludedSchemes)->empty());
}

TEST_F(ExternalProtocolHandlerTest, TestGetBlockStateDefaultBlock) {
  ExternalProtocolHandler::BlockState block_state =
      ExternalProtocolHandler::GetBlockState("afp", profile_.get());
  EXPECT_EQ(ExternalProtocolHandler::BLOCK, block_state);
  block_state = ExternalProtocolHandler::GetBlockState("res", profile_.get());
  EXPECT_EQ(ExternalProtocolHandler::BLOCK, block_state);
  block_state =
      ExternalProtocolHandler::GetBlockState("ie.http", profile_.get());
  EXPECT_EQ(ExternalProtocolHandler::BLOCK, block_state);
  EXPECT_TRUE(
      profile_->GetPrefs()->GetDictionary(prefs::kExcludedSchemes)->empty());
}

TEST_F(ExternalProtocolHandlerTest, TestGetBlockStateDefaultDontBlock) {
  ExternalProtocolHandler::BlockState block_state =
      ExternalProtocolHandler::GetBlockState("mailto", profile_.get());
  EXPECT_EQ(ExternalProtocolHandler::DONT_BLOCK, block_state);
  EXPECT_TRUE(
      profile_->GetPrefs()->GetDictionary(prefs::kExcludedSchemes)->empty());
}

TEST_F(ExternalProtocolHandlerTest, TestSetBlockState) {
  const char kScheme[] = "custom";
  ExternalProtocolHandler::BlockState block_state =
      ExternalProtocolHandler::GetBlockState(kScheme, profile_.get());
  EXPECT_EQ(ExternalProtocolHandler::UNKNOWN, block_state);
  EXPECT_TRUE(
      profile_->GetPrefs()->GetDictionary(prefs::kExcludedSchemes)->empty());

  // Set to DONT_BLOCK, and make sure it is written to prefs.
  ExternalProtocolHandler::SetBlockState(
      kScheme, ExternalProtocolHandler::DONT_BLOCK, profile_.get());
  block_state = ExternalProtocolHandler::GetBlockState(kScheme, profile_.get());
  EXPECT_EQ(ExternalProtocolHandler::DONT_BLOCK, block_state);
  base::Value expected_excluded_schemes(base::Value::Type::DICTIONARY);
  expected_excluded_schemes.SetKey(kScheme, base::Value(false));
  EXPECT_EQ(expected_excluded_schemes,
            *profile_->GetPrefs()->GetDictionary(prefs::kExcludedSchemes));

  // Note: BLOCK is no longer supported (it triggers a DCHECK in SetBlockState;
  // see https://crbug.com/724919).

  // Set back to UNKNOWN, and make sure this results in an empty dictionary.
  ExternalProtocolHandler::SetBlockState(
      kScheme, ExternalProtocolHandler::UNKNOWN, profile_.get());
  block_state = ExternalProtocolHandler::GetBlockState(kScheme, profile_.get());
  EXPECT_EQ(ExternalProtocolHandler::UNKNOWN, block_state);
  EXPECT_TRUE(
      profile_->GetPrefs()->GetDictionary(prefs::kExcludedSchemes)->empty());
}
