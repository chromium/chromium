// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/external_protocol/external_protocol_handler.h"

#include <memory>
#include <utility>

#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/weak_document_ptr.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/jni_android.h"
#include "components/navigation_interception/intercept_navigation_delegate.h"
#else
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#endif  // BUILDFLAG(IS_ANDROID)

class FakeExternalProtocolHandlerWorker
    : public shell_integration::DefaultSchemeClientWorker {
 public:
  FakeExternalProtocolHandlerWorker(
      const GURL& url,
      shell_integration::DefaultWebClientState os_state,
      const std::u16string& program_name)
      : shell_integration::DefaultSchemeClientWorker(url),
        os_state_(os_state),
        program_name_(program_name) {}

 private:
  ~FakeExternalProtocolHandlerWorker() override = default;

  shell_integration::DefaultWebClientState CheckIsDefaultImpl() override {
    return os_state_;
  }

  std::u16string GetDefaultClientNameImpl() override { return program_name_; }

  void SetAsDefaultImpl(base::OnceClosure on_finished_callback) override {
    std::move(on_finished_callback).Run();
  }

  shell_integration::DefaultWebClientState os_state_;
  std::u16string program_name_;
};

class FakeExternalProtocolHandlerDelegate
    : public ExternalProtocolHandler::Delegate {
 public:
  explicit FakeExternalProtocolHandlerDelegate(base::OnceClosure on_complete)
      : block_state_(ExternalProtocolHandler::BLOCK),
        os_state_(shell_integration::UNKNOWN_DEFAULT),
        complete_on_launch_(false),
        has_launched_(false),
        has_prompted_(false),
        has_blocked_(false),
        on_complete_(std::move(on_complete)),
        program_name_(u"") {}

  scoped_refptr<shell_integration::DefaultSchemeClientWorker> CreateShellWorker(
      const GURL& url) override {
    return new FakeExternalProtocolHandlerWorker(url, os_state_, program_name_);
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
      const std::optional<url::Origin>& initiating_origin,
      const std::u16string& program_name) override {
    EXPECT_EQ(block_state_, ExternalProtocolHandler::UNKNOWN);
    EXPECT_NE(os_state_, shell_integration::IS_DEFAULT);
    EXPECT_EQ(program_name_, program_name);
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
    if (complete_on_launch_ && on_complete_)
      std::move(on_complete_).Run();
  }

  void FinishedProcessingCheck() override {
    if (on_complete_)
      std::move(on_complete_).Run();
  }

  void set_os_state(shell_integration::DefaultWebClientState value) {
    os_state_ = value;
  }

  void set_program_name(const std::u16string& value) { program_name_ = value; }

  void set_block_state(ExternalProtocolHandler::BlockState value) {
    block_state_ = value;
  }

  // Set this to true if you need the test to be completed upon calling into
  // LaunchUrlWithoutSecurityCheck which is the case when testing
  // ExternalProtocolHandler::LaunchUrlWithoutSecurityCheck.
  void set_complete_on_launch(bool value) { complete_on_launch_ = value; }

  bool has_launched() { return has_launched_; }
  bool has_prompted() { return has_prompted_; }
  bool has_blocked() { return has_blocked_; }
  const std::optional<url::Origin>& initiating_origin() {
    return initiating_origin_;
  }

  const std::string& launch_or_prompt_url() {
    return launch_or_prompt_url_.spec();
  }

 private:
  ExternalProtocolHandler::BlockState block_state_;
  shell_integration::DefaultWebClientState os_state_;
  bool complete_on_launch_;
  bool has_launched_;
  bool has_prompted_;
  bool has_blocked_;
  GURL launch_or_prompt_url_;
  std::optional<url::Origin> initiating_origin_;
  base::OnceClosure on_complete_;
  std::u16string program_name_;
};

class ExternalProtocolHandlerTest : public testing::Test {
 public:
  content::WebContents* GetWebContents() const { return web_contents_.get(); }

 protected:
  ExternalProtocolHandlerTest() : delegate_(run_loop_.QuitClosure()) {}

  void SetUp() override {
    profile_ = std::make_unique<TestingProfile>();
    rvh_test_enabler_ = std::make_unique<content::RenderViewHostTestEnabler>();
    web_contents_ = content::WebContentsTester::CreateTestWebContents(
        profile_.get(), nullptr);
#if !BUILDFLAG(IS_ANDROID)
    web_modal::WebContentsModalDialogManager::CreateForWebContents(
        web_contents_.get());
#endif  // !BUILDFLAG(IS_ANDROID)
  }

  void TearDown() override {
    // Ensure that g_accept_requests gets set back to true after test execution.
    ExternalProtocolHandler::PermitLaunchUrl();
    TestingBrowserProcess::GetGlobal()->SetLocalState(nullptr);
  }

  enum class Action { PROMPT, LAUNCH, BLOCK, NONE };

  void DoTest(ExternalProtocolHandler::BlockState block_state,
              shell_integration::DefaultWebClientState os_state,
              Action expected_action) {
    DoTest(block_state, os_state, expected_action, GURL("mailto:test@test.com"),
           url::Origin::Create(GURL("https://example.test")),
           url::Origin::Create(GURL("https://precursor.test")), u"TestApp");
  }

  // Launches |url| in the current WebContents and checks that the
  // ExternalProtocolHandler's delegate is called with the correct action (as
  // given in |expected_action|). |initiating_origin| is passed to the
  // ExternalProtocolHandler to attribute the request to launch the URL to a
  // particular site. If |initiating_origin| is opaque (in production, an
  // example would be a sandboxed iframe), then the delegate should be passed
  // the origin's precursor origin. The precursor origin is the origin that
  // created |initiating_origin|, and the expected precursor origin, if any, is
  // provided in |expected_initiating_precursor_origin|.
  void DoTest(ExternalProtocolHandler::BlockState block_state,
              shell_integration::DefaultWebClientState os_state,
              Action expected_action,
              const GURL& url,
              const url::Origin& initiating_origin,
              const url::Origin& expected_initiating_precursor_origin,
              const std::u16string& program_name) {
    EXPECT_FALSE(delegate_.has_prompted());
    EXPECT_FALSE(delegate_.has_launched());
    EXPECT_FALSE(delegate_.has_blocked());
    ExternalProtocolHandler::SetDelegateForTesting(&delegate_);
    delegate_.set_block_state(block_state);
    delegate_.set_os_state(os_state);
    delegate_.set_program_name(program_name);
    ExternalProtocolHandler::LaunchUrl(
        url,
        base::BindRepeating(&ExternalProtocolHandlerTest::GetWebContents,
                            base::Unretained(this)),
        ui::PAGE_TRANSITION_LINK, /*has_user_gesture=*/true,
        /*is_in_fenced_frame_tree=*/false, initiating_origin,
        content::WeakDocumentPtr()
#if BUILDFLAG(IS_ANDROID)
            ,
        nullptr
#endif
    );
    run_loop_.Run();
    ExternalProtocolHandler::SetDelegateForTesting(nullptr);

    EXPECT_EQ(expected_action == Action::PROMPT, delegate_.has_prompted());
    EXPECT_EQ(expected_action == Action::LAUNCH, delegate_.has_launched());
    EXPECT_EQ(expected_action == Action::BLOCK, delegate_.has_blocked());
    if (expected_action == Action::PROMPT) {
      ASSERT_TRUE(delegate_.initiating_origin().has_value());
      if (initiating_origin.opaque()) {
        EXPECT_EQ(expected_initiating_precursor_origin,
                  delegate_.initiating_origin().value());
      } else {
        EXPECT_EQ(initiating_origin, delegate_.initiating_origin().value());
      }
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

// Android doesn't use the external protocol dialog.
#if !BUILDFLAG(IS_ANDROID)

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
         Action::PROMPT, url, url::Origin::Create(GURL("https://example.test")),
         url::Origin::Create(GURL("https://precursor.test")), u"TestApp");
  // Expect that the "\r\n" has been removed, and all other illegal URL
  // characters have been escaped.
  EXPECT_EQ("alert:test%20message%22%20--bad%2B%20%E6%96%87%E6%9C%AC%20%22file",
            delegate_.launch_or_prompt_url());
}

TEST_F(ExternalProtocolHandlerTest, TestNoDialogWithoutManager) {
  // WebContents without a dialog manager should not prompt crbug.com/40064553.
  GetWebContents()->SetUserData(
      web_modal::WebContentsModalDialogManager::UserDataKey(), nullptr);
  EXPECT_EQ(nullptr, web_modal::WebContentsModalDialogManager::FromWebContents(
                         GetWebContents()));
  DoTest(ExternalProtocolHandler::UNKNOWN, shell_integration::UNKNOWN_DEFAULT,
         Action::NONE);
}

#else  // if !BUILDFLAG(IS_ANDROID)

class MockInterceptNavigationDelegate
    : public navigation_interception::InterceptNavigationDelegate {
 public:
  MockInterceptNavigationDelegate()
      : InterceptNavigationDelegate(base::android::AttachCurrentThread(),
                                    nullptr) {}

  MOCK_METHOD5(HandleSubframeExternalProtocol,
               void(const GURL&,
                    ui::PageTransition,
                    bool,
                    const std::optional<url::Origin>&,
                    mojo::PendingRemote<network::mojom::URLLoaderFactory>*));
};

TEST_F(ExternalProtocolHandlerTest, TestUrlEscape_Android) {
  GURL url("alert:test message\" --bad%2B\r\n 文本 \"file");
  GURL escaped(
      "alert:test%20message%22%20--bad%2B%20%E6%96%87%E6%9C%AC%20%22file");

  auto delegate = std::make_unique<MockInterceptNavigationDelegate>();

  url::Origin precursor_origin =
      url::Origin::Create(GURL("https://precursor.test"));
  url::Origin opaque_origin =
      url::Origin::Resolve(GURL("data:text/html,hi"), precursor_origin);

  EXPECT_CALL(*delegate.get(),
              HandleSubframeExternalProtocol(testing::Eq(escaped), testing::_,
                                             true, testing::Eq(opaque_origin),
                                             testing::Eq(nullptr)));

  navigation_interception::InterceptNavigationDelegate::Associate(
      web_contents_.get(), std::move(delegate));

  ExternalProtocolHandler::LaunchUrl(
      url,
      base::BindRepeating(&ExternalProtocolHandlerTest::GetWebContents,
                          base::Unretained(this)),
      ui::PAGE_TRANSITION_LINK, /*has_user_gesture=*/true,
      /*is_in_fenced_frame_tree=*/false, opaque_origin,
      content::WeakDocumentPtr(), nullptr);
}

#endif  // if !BUILDFLAG(IS_ANDROID)

TEST_F(ExternalProtocolHandlerTest, TestUrlEscapeNoChecks) {
  GURL url("alert:test message\" --bad%2B\r\n 文本 \"file");

  EXPECT_FALSE(delegate_.has_prompted());
  EXPECT_FALSE(delegate_.has_launched());
  EXPECT_FALSE(delegate_.has_blocked());
  ExternalProtocolHandler::SetDelegateForTesting(&delegate_);
  delegate_.set_block_state(ExternalProtocolHandler::DONT_BLOCK);
  delegate_.set_os_state(shell_integration::NOT_DEFAULT);
  delegate_.set_complete_on_launch(true);
  ExternalProtocolHandler::LaunchUrlWithoutSecurityCheck(
      url, web_contents_.get(), content::WeakDocumentPtr());
  run_loop_.Run();
  ExternalProtocolHandler::SetDelegateForTesting(nullptr);

  EXPECT_FALSE(delegate_.has_prompted());
  EXPECT_TRUE(delegate_.has_launched());
  EXPECT_FALSE(delegate_.has_blocked());
  EXPECT_FALSE(delegate_.initiating_origin().has_value());

  // Expect that the "\r\n" has been removed, and all other illegal URL
  // characters have been escaped.
  EXPECT_EQ("alert:test%20message%22%20--bad%2B%20%E6%96%87%E6%9C%AC%20%22file",
            delegate_.launch_or_prompt_url());
}

TEST_F(ExternalProtocolHandlerTest, TestGetBlockStateUnknown) {
  base::HistogramTester histogram_tester;

  ExternalProtocolHandler::BlockState block_state =
      ExternalProtocolHandler::GetBlockState("tel", nullptr, profile_.get());
  EXPECT_EQ(ExternalProtocolHandler::UNKNOWN, block_state);
  block_state =
      ExternalProtocolHandler::GetBlockState("news", nullptr, profile_.get());
  EXPECT_EQ(ExternalProtocolHandler::UNKNOWN, block_state);
  block_state =
      ExternalProtocolHandler::GetBlockState("snews", nullptr, profile_.get());
  EXPECT_EQ(ExternalProtocolHandler::UNKNOWN, block_state);

  EXPECT_TRUE(profile_->GetPrefs()
                  ->GetDict(prefs::kProtocolHandlerPerOriginAllowedProtocols)
                  .empty());
  histogram_tester.ExpectBucketCount(
      ExternalProtocolHandler::kBlockStateMetric,
      ExternalProtocolHandler::BlockStateMetric::kPrompt, 3);
}

TEST_F(ExternalProtocolHandlerTest, TestGetBlockStateDefaultBlock) {
  base::HistogramTester histogram_tester;

  ExternalProtocolHandler::BlockState block_state =
      ExternalProtocolHandler::GetBlockState("afp", nullptr, profile_.get());
  EXPECT_EQ(ExternalProtocolHandler::BLOCK, block_state);
  block_state =
      ExternalProtocolHandler::GetBlockState("res", nullptr, profile_.get());
  EXPECT_EQ(ExternalProtocolHandler::BLOCK, block_state);
  block_state = ExternalProtocolHandler::GetBlockState("ie.http", nullptr,
                                                       profile_.get());
  EXPECT_EQ(ExternalProtocolHandler::BLOCK, block_state);
  EXPECT_EQ("mk", GURL("mk:@FooBar:ie.http:res://foo.bar/baz").scheme());
  block_state =
      ExternalProtocolHandler::GetBlockState("mk", nullptr, profile_.get());
  EXPECT_EQ(ExternalProtocolHandler::BLOCK, block_state);

  EXPECT_TRUE(profile_->GetPrefs()
                  ->GetDict(prefs::kProtocolHandlerPerOriginAllowedProtocols)
                  .empty());
  histogram_tester.ExpectBucketCount(
      ExternalProtocolHandler::kBlockStateMetric,
      ExternalProtocolHandler::BlockStateMetric::kDeniedDefault, 4);
}

TEST_F(ExternalProtocolHandlerTest, TestGetBlockStateDefaultDontBlock) {
  base::HistogramTester histogram_tester;

  ExternalProtocolHandler::BlockState block_state =
      ExternalProtocolHandler::GetBlockState("mailto", nullptr, profile_.get());
  EXPECT_EQ(ExternalProtocolHandler::DONT_BLOCK, block_state);

  EXPECT_TRUE(profile_->GetPrefs()
                  ->GetDict(prefs::kProtocolHandlerPerOriginAllowedProtocols)
                  .empty());
  histogram_tester.ExpectBucketCount(
      ExternalProtocolHandler::kBlockStateMetric,
      ExternalProtocolHandler::BlockStateMetric::kAllowedDefaultMail, 1);
}

TEST_F(ExternalProtocolHandlerTest, TestSetBlockState) {
  base::HistogramTester histogram_tester;

  const char kScheme_1[] = "custom1";
  const char kScheme_2[] = "custom2";
  url::Origin example_origin_1 =
      url::Origin::Create(GURL("https://example.test"));
  url::Origin example_origin_2 =
      url::Origin::Create(GURL("https://example2.test"));
  ExternalProtocolHandler::BlockState block_state =
      ExternalProtocolHandler::GetBlockState(kScheme_1, &example_origin_1,
                                             profile_.get());
  EXPECT_EQ(ExternalProtocolHandler::UNKNOWN, block_state);
  block_state = ExternalProtocolHandler::GetBlockState(
      kScheme_2, &example_origin_1, profile_.get());
  EXPECT_EQ(ExternalProtocolHandler::UNKNOWN, block_state);
  block_state = ExternalProtocolHandler::GetBlockState(
      kScheme_1, &example_origin_2, profile_.get());
  EXPECT_EQ(ExternalProtocolHandler::UNKNOWN, block_state);
  block_state = ExternalProtocolHandler::GetBlockState(
      kScheme_2, &example_origin_2, profile_.get());
  EXPECT_EQ(ExternalProtocolHandler::UNKNOWN, block_state);
  EXPECT_TRUE(profile_->GetPrefs()
                  ->GetDict(prefs::kProtocolHandlerPerOriginAllowedProtocols)
                  .empty());
  histogram_tester.ExpectBucketCount(
      ExternalProtocolHandler::kBlockStateMetric,
      ExternalProtocolHandler::BlockStateMetric::kPrompt, 4);

  // Set to DONT_BLOCK for {kScheme_1, example_origin_1}, and make sure it is
  // written to prefs.
  ExternalProtocolHandler::SetBlockState(kScheme_1, example_origin_1,
                                         ExternalProtocolHandler::DONT_BLOCK,
                                         profile_.get());
  block_state = ExternalProtocolHandler::GetBlockState(
      kScheme_1, &example_origin_1, profile_.get());
  EXPECT_EQ(ExternalProtocolHandler::DONT_BLOCK, block_state);
  block_state = ExternalProtocolHandler::GetBlockState(
      kScheme_2, &example_origin_1, profile_.get());
  EXPECT_EQ(ExternalProtocolHandler::UNKNOWN, block_state);
  block_state = ExternalProtocolHandler::GetBlockState(
      kScheme_1, &example_origin_2, profile_.get());
  EXPECT_EQ(ExternalProtocolHandler::UNKNOWN, block_state);
  block_state = ExternalProtocolHandler::GetBlockState(
      kScheme_2, &example_origin_2, profile_.get());
  EXPECT_EQ(ExternalProtocolHandler::UNKNOWN, block_state);
  histogram_tester.ExpectBucketCount(
      ExternalProtocolHandler::kBlockStateMetric,
      ExternalProtocolHandler::BlockStateMetric::kAllowedByPreference, 1);
  histogram_tester.ExpectBucketCount(
      ExternalProtocolHandler::kBlockStateMetric,
      ExternalProtocolHandler::BlockStateMetric::kPrompt, 7);

  // Set to DONT_BLOCK for {kScheme_2, example_origin_2}, and make sure it is
  // written to prefs independently of {kScheme_1, example_origin_1}.
  ExternalProtocolHandler::SetBlockState(kScheme_2, example_origin_2,
                                         ExternalProtocolHandler::DONT_BLOCK,
                                         profile_.get());
  block_state = ExternalProtocolHandler::GetBlockState(
      kScheme_1, &example_origin_1, profile_.get());
  EXPECT_EQ(ExternalProtocolHandler::DONT_BLOCK, block_state);
  block_state = ExternalProtocolHandler::GetBlockState(
      kScheme_2, &example_origin_1, profile_.get());
  EXPECT_EQ(ExternalProtocolHandler::UNKNOWN, block_state);
  block_state = ExternalProtocolHandler::GetBlockState(
      kScheme_1, &example_origin_2, profile_.get());
  EXPECT_EQ(ExternalProtocolHandler::UNKNOWN, block_state);
  block_state = ExternalProtocolHandler::GetBlockState(
      kScheme_2, &example_origin_2, profile_.get());
  EXPECT_EQ(ExternalProtocolHandler::DONT_BLOCK, block_state);
  histogram_tester.ExpectBucketCount(
      ExternalProtocolHandler::kBlockStateMetric,
      ExternalProtocolHandler::BlockStateMetric::kAllowedByPreference, 3);
  histogram_tester.ExpectBucketCount(
      ExternalProtocolHandler::kBlockStateMetric,
      ExternalProtocolHandler::BlockStateMetric::kPrompt, 9);

  const base::Value::Dict& protocol_origin_pairs =
      profile_->GetPrefs()->GetDict(
          prefs::kProtocolHandlerPerOriginAllowedProtocols);
  base::Value::Dict expected_allowed_protocols_for_example_origin_1;
  expected_allowed_protocols_for_example_origin_1.Set(kScheme_1, true);
  const base::Value::Dict* allowed_protocols_for_example_origin_1 =
      protocol_origin_pairs.FindDict(example_origin_1.Serialize());
  EXPECT_EQ(expected_allowed_protocols_for_example_origin_1,
            *allowed_protocols_for_example_origin_1);
  base::Value::Dict expected_allowed_protocols_for_example_origin_2;
  expected_allowed_protocols_for_example_origin_2.Set(kScheme_2, true);
  const base::Value::Dict* allowed_protocols_for_example_origin_2 =
      protocol_origin_pairs.FindDict(example_origin_2.Serialize());
  EXPECT_EQ(expected_allowed_protocols_for_example_origin_2,
            *allowed_protocols_for_example_origin_2);

  // Note: BLOCK is no longer supported (it triggers a DCHECK in SetBlockState;
  // see https://crbug.com/724919).

  // Set back to UNKNOWN, and make sure this results in an empty dictionary.
  ExternalProtocolHandler::SetBlockState(kScheme_1, example_origin_1,
                                         ExternalProtocolHandler::UNKNOWN,
                                         profile_.get());
  ExternalProtocolHandler::SetBlockState(kScheme_2, example_origin_2,
                                         ExternalProtocolHandler::UNKNOWN,
                                         profile_.get());
  block_state = ExternalProtocolHandler::GetBlockState(
      kScheme_1, &example_origin_1, profile_.get());
  EXPECT_EQ(ExternalProtocolHandler::UNKNOWN, block_state);
  block_state = ExternalProtocolHandler::GetBlockState(
      kScheme_2, &example_origin_2, profile_.get());
  EXPECT_EQ(ExternalProtocolHandler::UNKNOWN, block_state);
  EXPECT_TRUE(profile_->GetPrefs()
                  ->GetDict(prefs::kProtocolHandlerPerOriginAllowedProtocols)
                  .empty());
  histogram_tester.ExpectBucketCount(
      ExternalProtocolHandler::kBlockStateMetric,
      ExternalProtocolHandler::BlockStateMetric::kAllowedByPreference, 3);
  histogram_tester.ExpectBucketCount(
      ExternalProtocolHandler::kBlockStateMetric,
      ExternalProtocolHandler::BlockStateMetric::kPrompt, 11);
}

TEST_F(ExternalProtocolHandlerTest, TestSetBlockStateWithUntrustowrthyOrigin) {
  const char kScheme[] = "custom";
  // This origin is untrustworthy because it is "http://"
  url::Origin untrustworthy_origin =
      url::Origin::Create(GURL("http://example.test"));

  ExternalProtocolHandler::BlockState block_state =
      ExternalProtocolHandler::GetBlockState(kScheme, &untrustworthy_origin,
                                             profile_.get());
  EXPECT_EQ(ExternalProtocolHandler::UNKNOWN, block_state);
  EXPECT_TRUE(profile_->GetPrefs()
                  ->GetDict(prefs::kProtocolHandlerPerOriginAllowedProtocols)
                  .empty());

  // Set to DONT_BLOCK for {kScheme, untrustworthy_origin}, and make sure it is
  // not written to prefs. Calling SetBlockState with a non-trustworthy origin
  // should not persist any state to prefs.
  ExternalProtocolHandler::SetBlockState(kScheme, untrustworthy_origin,
                                         ExternalProtocolHandler::DONT_BLOCK,
                                         profile_.get());
  block_state = ExternalProtocolHandler::GetBlockState(
      kScheme, &untrustworthy_origin, profile_.get());
  EXPECT_EQ(ExternalProtocolHandler::UNKNOWN, block_state);
  EXPECT_TRUE(profile_->GetPrefs()
                  ->GetDict(prefs::kProtocolHandlerPerOriginAllowedProtocols)
                  .empty());
}

#if !BUILDFLAG(IS_ANDROID)
// Test that an opaque initiating origin gets transformed to its precursor
// origin when the dialog is shown.
TEST_F(ExternalProtocolHandlerTest, TestOpaqueInitiatingOrigin) {
  url::Origin precursor_origin =
      url::Origin::Create(GURL("https://precursor.test"));
  url::Origin opaque_origin =
      url::Origin::Resolve(GURL("data:text/html,hi"), precursor_origin);
  DoTest(ExternalProtocolHandler::UNKNOWN, shell_integration::NOT_DEFAULT,
         Action::PROMPT, GURL("mailto:test@test.test"), opaque_origin,
         precursor_origin, u"TestApp");
}
#endif
