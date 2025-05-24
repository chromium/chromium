// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/data_protection/data_protection_clipboard_utils.h"

#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/connectors/test/deep_scanning_test_utils.h"
#include "chrome/browser/enterprise/data_controls/desktop_data_controls_dialog.h"
#include "chrome/browser/enterprise/data_controls/desktop_data_controls_dialog_test_helper.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/enterprise/data_controls/core/browser/test_utils.h"
#include "components/safe_browsing/core/common/features.h"
#include "content/public/test/browser_test.h"
#include "ui/views/widget/widget_delegate.h"

namespace enterprise_data_protection {

namespace {

constexpr char kGoogleUrl[] = "https://google.com/";
constexpr char kUserName[] = "test-user@chromium.org";

content::ClipboardPasteData MakeClipboardPasteData(
    std::string text,
    std::string image,
    std::vector<base::FilePath> file_paths) {
  content::ClipboardPasteData clipboard_paste_data;
  clipboard_paste_data.text = base::UTF8ToUTF16(text);
  clipboard_paste_data.png = std::vector<uint8_t>(image.begin(), image.end());
  clipboard_paste_data.file_paths = std::move(file_paths);
  return clipboard_paste_data;
}

// TODO(crbug.com/387484337): Set up equivalent browser tests for Clank.
// Tests for functions and classes declared in data_protection_clipboard_utils.h
// For browser tests that test data protection integration with Chrome's
// clipboard logic, see clipboard_browsertests.cc
class DataControlsClipboardUtilsBrowserTest
    : public InProcessBrowserTest,
      public testing::WithParamInterface<bool> {
 public:
  DataControlsClipboardUtilsBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(
        safe_browsing::kLocalIpAddressInEvents);
  }

  ~DataControlsClipboardUtilsBrowserTest() override = default;

  bool machine_scope() const { return GetParam(); }

  void SetUpOnMainThread() override {
    event_report_validator_helper_ = std::make_unique<
        enterprise_connectors::test::EventReportValidatorHelper>(
        browser()->profile(), /*browser_test=*/true);
  }

  void TearDownOnMainThread() override {
    event_report_validator_helper_.reset();
  }

  content::WebContents* contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

 protected:
  std::unique_ptr<enterprise_connectors::test::EventReportValidatorHelper>
      event_report_validator_helper_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         DataControlsClipboardUtilsBrowserTest,
                         testing::Bool());

}  // namespace

IN_PROC_BROWSER_TEST_P(DataControlsClipboardUtilsBrowserTest,
                       PasteAllowed_NoSource) {
  auto event_validator = event_report_validator_helper_->CreateValidator();
  event_validator.ExpectNoReport();

  data_controls::DesktopDataControlsDialogTestHelper helper(
      data_controls::DataControlsDialog::Type::kClipboardPasteBlock);
  base::test::TestFuture<std::optional<content::ClipboardPasteData>> future;
  PasteIfAllowedByPolicy(
      /*source=*/content::ClipboardEndpoint(std::nullopt),
      /*destination=*/
      content::ClipboardEndpoint(
          ui::DataTransferEndpoint(GURL("https://google.com")),
          base::BindLambdaForTesting(
              [this]() { return contents()->GetBrowserContext(); }),
          *contents()->GetPrimaryMainFrame()),
      /*metadata=*/{.size = 1234}, MakeClipboardPasteData("text", "image", {}),
      future.GetCallback());

  auto paste_data = future.Get();
  EXPECT_TRUE(paste_data);
  EXPECT_EQ(paste_data->text, u"text");
  EXPECT_EQ(std::string(paste_data->png.begin(), paste_data->png.end()),
            "image");

  EXPECT_FALSE(helper.dialog());
}

IN_PROC_BROWSER_TEST_P(DataControlsClipboardUtilsBrowserTest,
                       PasteAllowed_SameSource) {
  base::RunLoop run_loop;
  auto event_validator = event_report_validator_helper_->CreateValidator();
  event_validator.SetDoneClosure(run_loop.QuitClosure());
  event_validator.ExpectDataControlsSensitiveDataEvent(
      /*expected_url=*/
      kGoogleUrl,
      /*expected_tab_url=*/kGoogleUrl,
      /*source=*/"CLIPBOARD",
      /*destination=*/kGoogleUrl,
      /*mime_types=*/
      []() {
        static std::set<std::string> set = {"text/html"};
        return &set;
      }(),
      /*trigger=*/"WEB_CONTENT_UPLOAD",
      /*triggered_rules=*/{{0, {"123", "name"}}},
      /*event_result=*/"EVENT_RESULT_BLOCKED",
      /*profile_username=*/kUserName,
      /*profile_identifier=*/browser()->profile()->GetPath().AsUTF8Unsafe(),
      /*content_size=*/1234);

  data_controls::SetDataControls(browser()->profile()->GetPrefs(), {R"({
                                   "name": "name",
                                   "rule_id": "123",
                                   "destinations": {
                                     "urls": ["google.com"]
                                   },
                                   "restrictions": [
                                     {"class": "CLIPBOARD", "level": "BLOCK"}
                                   ]
                                 })"},
                                 machine_scope());
  data_controls::DesktopDataControlsDialogTestHelper helper(
      data_controls::DataControlsDialog::Type::kClipboardPasteBlock);

  base::test::TestFuture<std::optional<content::ClipboardPasteData>> future;
  PasteIfAllowedByPolicy(
      /*source=*/content::ClipboardEndpoint(std::nullopt),
      /*destination=*/
      content::ClipboardEndpoint(
          ui::DataTransferEndpoint(GURL("https://google.com")),
          base::BindLambdaForTesting(
              [this]() { return contents()->GetBrowserContext(); }),
          *contents()->GetPrimaryMainFrame()),
      /*metadata=*/
      {
          .size = 1234,
          .format_type = ui::ClipboardFormatType::HtmlType(),
      },
      MakeClipboardPasteData("text", "image", {}), future.GetCallback());

  auto paste_data = future.Get();
  EXPECT_FALSE(paste_data);

  helper.WaitForDialogToInitialize();
  helper.CloseDialogWithoutBypass();
  helper.WaitForDialogToClose();
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_P(DataControlsClipboardUtilsBrowserTest,
                       PasteBlockedByDataControls_DestinationRule) {
  base::RunLoop run_loop;
  auto event_validator = event_report_validator_helper_->CreateValidator();
  event_validator.SetDoneClosure(run_loop.QuitClosure());
  event_validator.ExpectDataControlsSensitiveDataEvent(
      /*expected_url=*/
      kGoogleUrl,
      /*expected_tab_url=*/kGoogleUrl,
      /*source=*/"CLIPBOARD",
      /*destination=*/kGoogleUrl,
      /*mime_types=*/
      []() {
        static std::set<std::string> set = {"text/plain"};
        return &set;
      }(),
      /*trigger=*/"WEB_CONTENT_UPLOAD",
      /*triggered_rules=*/{{0, {"222", "rule_name"}}},
      /*event_result=*/"EVENT_RESULT_BLOCKED",
      /*profile_username=*/kUserName,
      /*profile_identifier=*/browser()->profile()->GetPath().AsUTF8Unsafe(),
      /*content_size=*/1234);

  data_controls::SetDataControls(browser()->profile()->GetPrefs(), {R"({
                                   "name": "rule_name",
                                   "rule_id": "222",
                                   "destinations": {
                                     "urls": ["google.com"]
                                   },
                                   "restrictions": [
                                     {"class": "CLIPBOARD", "level": "BLOCK"}
                                   ]
                                 })"},
                                 machine_scope());
  data_controls::DesktopDataControlsDialogTestHelper helper(
      data_controls::DataControlsDialog::Type::kClipboardPasteBlock);

  base::test::TestFuture<std::optional<content::ClipboardPasteData>> future;
  PasteIfAllowedByPolicy(
      /*source=*/content::ClipboardEndpoint(std::nullopt),
      /*destination=*/
      content::ClipboardEndpoint(
          ui::DataTransferEndpoint(GURL("https://google.com")),
          base::BindLambdaForTesting(
              [this]() { return contents()->GetBrowserContext(); }),
          *contents()->GetPrimaryMainFrame()),
      /*metadata=*/
      {
          .size = 1234,
          .format_type = ui::ClipboardFormatType::PlainTextType(),
      },
      MakeClipboardPasteData("text", "image", {}), future.GetCallback());

  auto paste_data = future.Get();
  EXPECT_FALSE(paste_data);

  helper.WaitForDialogToInitialize();
  helper.CloseDialogWithoutBypass();
  helper.WaitForDialogToClose();
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_P(DataControlsClipboardUtilsBrowserTest,
                       PasteWarnedByDataControls_BypassedDestinationRule) {
  base::RunLoop run_loop_warn;
  auto event_validator = event_report_validator_helper_->CreateValidator();
  event_validator.SetDoneClosure(run_loop_warn.QuitClosure());
  event_validator.ExpectDataControlsSensitiveDataEvent(
      /*expected_url=*/
      kGoogleUrl,
      /*expected_tab_url=*/kGoogleUrl,
      /*source=*/"CLIPBOARD",
      /*destination=*/kGoogleUrl,
      /*mime_types=*/
      []() {
        static std::set<std::string> set = {"image/svg+xml"};
        return &set;
      }(),
      /*trigger=*/"WEB_CONTENT_UPLOAD",
      /*triggered_rules=*/{{0, {"333", "warn_rule_name"}}},
      /*event_result=*/"EVENT_RESULT_WARNED",
      /*profile_username=*/kUserName,
      /*profile_identifier=*/browser()->profile()->GetPath().AsUTF8Unsafe(),
      /*content_size=*/1234);

  data_controls::SetDataControls(browser()->profile()->GetPrefs(), {R"({
                                   "name": "warn_rule_name",
                                   "rule_id": "333",
                                   "destinations": {
                                     "urls": ["google.com"]
                                   },
                                   "restrictions": [
                                     {"class": "CLIPBOARD", "level": "WARN"}
                                   ]
                                 })"},
                                 machine_scope());
  data_controls::DesktopDataControlsDialogTestHelper helper(
      data_controls::DataControlsDialog::Type::kClipboardPasteWarn);

  base::test::TestFuture<std::optional<content::ClipboardPasteData>> future;
  PasteIfAllowedByPolicy(
      /*source=*/content::ClipboardEndpoint(std::nullopt),
      /*destination=*/
      content::ClipboardEndpoint(
          ui::DataTransferEndpoint(GURL("https://google.com")),
          base::BindLambdaForTesting(
              [this]() { return contents()->GetBrowserContext(); }),
          *contents()->GetPrimaryMainFrame()),
      /*metadata=*/
      {
          .size = 1234,
          .format_type = ui::ClipboardFormatType::SvgType(),
      },
      MakeClipboardPasteData("text", "image", {}), future.GetCallback());

  helper.WaitForDialogToInitialize();
  run_loop_warn.Run();

  // The first warn event should already be reported before the dialog has been
  // initialized, so it can be reassigned so that the bypass event can be
  // validated.
  base::RunLoop run_loop_bypass;
  event_validator = event_report_validator_helper_->CreateValidator();
  event_validator.SetDoneClosure(run_loop_bypass.QuitClosure());
  event_validator.ExpectDataControlsSensitiveDataEvent(
      /*expected_url=*/
      kGoogleUrl,
      /*expected_tab_url=*/kGoogleUrl,
      /*source=*/"CLIPBOARD",
      /*destination=*/kGoogleUrl,
      /*mime_types=*/
      []() {
        static std::set<std::string> set = {"image/svg+xml"};
        return &set;
      }(),
      /*trigger=*/"WEB_CONTENT_UPLOAD",
      /*triggered_rules=*/{{0, {"333", "warn_rule_name"}}},
      /*event_result=*/"EVENT_RESULT_BYPASSED",
      /*profile_username=*/kUserName,
      /*profile_identifier=*/browser()->profile()->GetPath().AsUTF8Unsafe(),
      /*content_size=*/1234);

  // The dialog will stay up until a user action dismisses it, so `future`
  // shouldn't be ready yet.
  EXPECT_FALSE(future.IsReady());

  helper.BypassWarning();
  helper.WaitForDialogToClose();

  auto paste_data = future.Get();
  EXPECT_TRUE(paste_data);
  EXPECT_EQ(paste_data->text, u"text");
  EXPECT_EQ(std::string(paste_data->png.begin(), paste_data->png.end()),
            "image");
  run_loop_bypass.Run();
}

IN_PROC_BROWSER_TEST_P(DataControlsClipboardUtilsBrowserTest,
                       PasteWarnedByDataControls_CanceledDestinationRule) {
  base::RunLoop run_loop;
  auto event_validator = event_report_validator_helper_->CreateValidator();
  event_validator.SetDoneClosure(run_loop.QuitClosure());
  event_validator.ExpectDataControlsSensitiveDataEvent(
      /*expected_url=*/
      kGoogleUrl,
      /*expected_tab_url=*/kGoogleUrl,
      /*source=*/"CLIPBOARD",
      /*destination=*/kGoogleUrl,
      /*mime_types=*/
      []() {
        static std::set<std::string> set = {"image/png"};
        return &set;
      }(),
      /*trigger=*/"WEB_CONTENT_UPLOAD",
      /*triggered_rules=*/{{0, {"1416", "warn rule name"}}},
      /*event_result=*/"EVENT_RESULT_WARNED",
      /*profile_username=*/kUserName,
      /*profile_identifier=*/browser()->profile()->GetPath().AsUTF8Unsafe(),
      /*content_size=*/1234);

  data_controls::SetDataControls(browser()->profile()->GetPrefs(), {R"({
                                   "name": "warn rule name",
                                   "rule_id": "1416",
                                   "destinations": {
                                     "urls": ["google.com"]
                                   },
                                   "restrictions": [
                                     {"class": "CLIPBOARD", "level": "WARN"}
                                   ]
                                 })"},
                                 machine_scope());
  data_controls::DesktopDataControlsDialogTestHelper helper(
      data_controls::DataControlsDialog::Type::kClipboardPasteWarn);

  base::test::TestFuture<std::optional<content::ClipboardPasteData>> future;
  PasteIfAllowedByPolicy(
      /*source=*/content::ClipboardEndpoint(std::nullopt),
      /*destination=*/
      content::ClipboardEndpoint(
          ui::DataTransferEndpoint(GURL("https://google.com")),
          base::BindLambdaForTesting(
              [this]() { return contents()->GetBrowserContext(); }),
          *contents()->GetPrimaryMainFrame()),
      /*metadata=*/
      {
          .size = 1234,
          .format_type = ui::ClipboardFormatType::PngType(),
      },
      MakeClipboardPasteData("text", "image", {}), future.GetCallback());

  helper.WaitForDialogToInitialize();

  // The dialog will stay up until a user action dismisses it, so `future`
  // shouldn't be ready yet.
  EXPECT_FALSE(future.IsReady());

  helper.CloseDialogWithoutBypass();
  helper.WaitForDialogToClose();

  auto paste_data = future.Get();
  EXPECT_FALSE(paste_data);
  run_loop.Run();
}

// ChromeOS requires extra boilerplate to run this test, and since copy-pasting
// between profiles on ChromeOS isn't a meaningful test it is simply omitted
// from running this.
#if !BUILDFLAG(IS_CHROMEOS)
IN_PROC_BROWSER_TEST_P(DataControlsClipboardUtilsBrowserTest,
                       PasteBlockedByDataControls_SourceRule) {
  auto event_validator = event_report_validator_helper_->CreateValidator();
  event_validator.ExpectNoReport();

  data_controls::SetDataControls(browser()->profile()->GetPrefs(), {R"({
                                   "name": "report_rule_name",
                                   "rule_id": "4321",
                                   "destinations": {
                                     "urls": ["google.com"]
                                   },
                                   "restrictions": [
                                     {"class": "CLIPBOARD", "level": "BLOCK"}
                                   ]
                                 })"},
                                 machine_scope());
  data_controls::DesktopDataControlsDialogTestHelper helper(
      data_controls::DataControlsDialog::Type::kClipboardPasteBlock);

  // By making a new profile for this test, we ensure we can prevent pasting to
  // it by having the rule set in the source profile.
  std::unique_ptr<Profile> destination_profile;
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    destination_profile = Profile::CreateProfile(
        g_browser_process->profile_manager()->user_data_dir().Append(
            FILE_PATH_LITERAL("DC Test Profile")),
        /*delegate=*/nullptr, Profile::CreateMode::kSynchronous);
  }

  base::test::TestFuture<std::optional<content::ClipboardPasteData>> future;
  PasteIfAllowedByPolicy(
      /*source=*/content::ClipboardEndpoint(
          ui::DataTransferEndpoint(GURL("https://foo.com")),
          base::BindLambdaForTesting(
              [this]() { return contents()->GetBrowserContext(); }),
          *contents()->GetPrimaryMainFrame()),
      /*destination=*/
      content::ClipboardEndpoint(
          ui::DataTransferEndpoint(GURL("https://google.com")),
          base::BindLambdaForTesting(
              [&destination_profile]() -> content::BrowserContext* {
                return destination_profile.get();
              }),
          *contents()->GetPrimaryMainFrame()),
      /*metadata=*/
      {.size = 1234}, MakeClipboardPasteData("text", "image", {}),
      future.GetCallback());

  auto paste_data = future.Get();
  EXPECT_FALSE(paste_data);

  helper.WaitForDialogToInitialize();
  helper.CloseDialogWithoutBypass();
  helper.WaitForDialogToClose();
}

IN_PROC_BROWSER_TEST_P(DataControlsClipboardUtilsBrowserTest,
                       PasteWarnedByDataControls_BypassedSourceRule) {
  auto event_validator = event_report_validator_helper_->CreateValidator();
  event_validator.ExpectNoReport();

  data_controls::SetDataControls(browser()->profile()->GetPrefs(), {R"({
                                   "name": "report_rule_name",
                                   "rule_id": "6543",
                                   "destinations": {
                                     "urls": ["google.com"]
                                   },
                                   "restrictions": [
                                     {"class": "CLIPBOARD", "level": "WARN"}
                                   ]
                                 })"},
                                 machine_scope());
  data_controls::DesktopDataControlsDialogTestHelper helper(
      data_controls::DataControlsDialog::Type::kClipboardPasteWarn);

  // By making a new profile for this test, we ensure we can prevent pasting to
  // it by having the rule set in the source profile.
  std::unique_ptr<Profile> destination_profile;
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    destination_profile = Profile::CreateProfile(
        g_browser_process->profile_manager()->user_data_dir().Append(
            FILE_PATH_LITERAL("DC Test Profile")),
        /*delegate=*/nullptr, Profile::CreateMode::kSynchronous);
  }

  base::test::TestFuture<std::optional<content::ClipboardPasteData>> future;
  PasteIfAllowedByPolicy(
      /*source=*/content::ClipboardEndpoint(
          ui::DataTransferEndpoint(GURL("https://foo.com")),
          base::BindLambdaForTesting(
              [this]() { return contents()->GetBrowserContext(); }),
          *contents()->GetPrimaryMainFrame()),
      /*destination=*/
      content::ClipboardEndpoint(
          ui::DataTransferEndpoint(GURL("https://google.com")),
          base::BindLambdaForTesting(
              [&destination_profile]() -> content::BrowserContext* {
                return destination_profile.get();
              }),
          *contents()->GetPrimaryMainFrame()),
      /*metadata=*/{.size = 1234}, MakeClipboardPasteData("text", "image", {}),
      future.GetCallback());

  // The dialog will stay up until a user action dismisses it, so `future`
  // shouldn't be ready yet.
  EXPECT_FALSE(future.IsReady());

  helper.BypassWarning();
  helper.WaitForDialogToClose();

  auto paste_data = future.Get();
  EXPECT_TRUE(paste_data);
  EXPECT_EQ(paste_data->text, u"text");
  EXPECT_EQ(std::string(paste_data->png.begin(), paste_data->png.end()),
            "image");
}

IN_PROC_BROWSER_TEST_P(DataControlsClipboardUtilsBrowserTest,
                       PasteWarnedByDataControls_CanceledSourceRule) {
  auto event_validator = event_report_validator_helper_->CreateValidator();
  event_validator.ExpectNoReport();

  data_controls::SetDataControls(browser()->profile()->GetPrefs(), {R"({
                                   "name": "report_rule_name",
                                   "rule_id": "7654",
                                   "destinations": {
                                     "urls": ["google.com"]
                                   },
                                   "restrictions": [
                                     {"class": "CLIPBOARD", "level": "WARN"}
                                   ]
                                 })"},
                                 machine_scope());
  data_controls::DesktopDataControlsDialogTestHelper helper(
      data_controls::DataControlsDialog::Type::kClipboardPasteWarn);

  // By making a new profile for this test, we ensure we can prevent pasting to
  // it by having the rule set in the source profile.
  std::unique_ptr<Profile> destination_profile;
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    destination_profile = Profile::CreateProfile(
        g_browser_process->profile_manager()->user_data_dir().Append(
            FILE_PATH_LITERAL("DC Test Profile")),
        /*delegate=*/nullptr, Profile::CreateMode::kSynchronous);
  }

  base::test::TestFuture<std::optional<content::ClipboardPasteData>> future;
  PasteIfAllowedByPolicy(
      /*source=*/content::ClipboardEndpoint(
          ui::DataTransferEndpoint(GURL("https://foo.com")),
          base::BindLambdaForTesting(
              [this]() { return contents()->GetBrowserContext(); }),
          *contents()->GetPrimaryMainFrame()),
      /*destination=*/
      content::ClipboardEndpoint(
          ui::DataTransferEndpoint(GURL("https://google.com")),
          base::BindLambdaForTesting(
              [&destination_profile]() -> content::BrowserContext* {
                return destination_profile.get();
              }),
          *contents()->GetPrimaryMainFrame()),
      /*metadata=*/{.size = 1234}, MakeClipboardPasteData("text", "image", {}),
      future.GetCallback());

  // The dialog will stay up until a user action dismisses it, so `future`
  // shouldn't be ready yet.
  EXPECT_FALSE(future.IsReady());

  helper.CloseDialogWithoutBypass();
  helper.WaitForDialogToClose();

  auto paste_data = future.Get();
  EXPECT_FALSE(paste_data);
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

IN_PROC_BROWSER_TEST_P(DataControlsClipboardUtilsBrowserTest,
                       PasteReportedByDataControls_DestinationRule) {
  base::RunLoop run_loop;
  auto event_validator = event_report_validator_helper_->CreateValidator();
  event_validator.SetDoneClosure(run_loop.QuitClosure());
  event_validator.ExpectDataControlsSensitiveDataEvent(
      /*expected_url=*/
      kGoogleUrl,
      /*expected_tab_url=*/kGoogleUrl,
      /*source=*/"CLIPBOARD",
      /*destination=*/kGoogleUrl,
      /*mime_types=*/
      []() {
        static std::set<std::string> set = {"image/svg+xml"};
        return &set;
      }(),
      /*trigger=*/"WEB_CONTENT_UPLOAD",
      /*triggered_rules=*/{{0, {"8765", "report_rule_name"}}},
      /*event_result=*/"EVENT_RESULT_ALLOWED",
      /*profile_username=*/kUserName,
      /*profile_identifier=*/browser()->profile()->GetPath().AsUTF8Unsafe(),
      /*content_size=*/1234);

  data_controls::SetDataControls(browser()->profile()->GetPrefs(), {R"({
                                   "name": "report_rule_name",
                                   "rule_id": "8765",
                                   "destinations": {
                                     "urls": ["google.com"]
                                   },
                                   "restrictions": [
                                     {"class": "CLIPBOARD", "level": "REPORT"}
                                   ]
                                 })"},
                                 machine_scope());
  data_controls::DesktopDataControlsDialogTestHelper helper(
      data_controls::DataControlsDialog::Type::kClipboardPasteWarn);

  base::test::TestFuture<std::optional<content::ClipboardPasteData>> future;
  PasteIfAllowedByPolicy(
      /*source=*/content::ClipboardEndpoint(std::nullopt),
      /*destination=*/
      content::ClipboardEndpoint(
          ui::DataTransferEndpoint(GURL("https://google.com")),
          base::BindLambdaForTesting(
              [this]() { return contents()->GetBrowserContext(); }),
          *contents()->GetPrimaryMainFrame()),
      /*metadata=*/
      {
          .size = 1234,
          .format_type = ui::ClipboardFormatType::SvgType(),
      },
      MakeClipboardPasteData("text", "image", {}), future.GetCallback());

  ASSERT_FALSE(helper.dialog());
  auto paste_data = future.Get();
  EXPECT_TRUE(paste_data);
  EXPECT_EQ(paste_data->text, u"text");
  EXPECT_EQ(std::string(paste_data->png.begin(), paste_data->png.end()),
            "image");
  run_loop.Run();
}

// ChromeOS requires extra boilerplate to run this test, and since copy-pasting
// between profiles on ChromeOS isn't a meaningful test it is simply omitted
// from running this.
#if !BUILDFLAG(IS_CHROMEOS)
IN_PROC_BROWSER_TEST_P(DataControlsClipboardUtilsBrowserTest,
                       PasteReportedByDataControls_SourceRule) {
  auto event_validator = event_report_validator_helper_->CreateValidator();
  event_validator.ExpectNoReport();

  data_controls::SetDataControls(browser()->profile()->GetPrefs(), {R"({
                                   "name": "report_rule_name",
                                   "rule_id": "9753",
                                   "destinations": {
                                     "urls": ["google.com"]
                                   },
                                   "restrictions": [
                                     {"class": "CLIPBOARD", "level": "REPORT"}
                                   ]
                                 })"},
                                 machine_scope());
  data_controls::DesktopDataControlsDialogTestHelper helper(
      data_controls::DataControlsDialog::Type::kClipboardPasteBlock);

  // By making a new profile for this test, we ensure we can prevent pasting to
  // it by having the rule set in the source profile.
  std::unique_ptr<Profile> destination_profile;
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    destination_profile = Profile::CreateProfile(
        g_browser_process->profile_manager()->user_data_dir().Append(
            FILE_PATH_LITERAL("DC Test Profile")),
        /*delegate=*/nullptr, Profile::CreateMode::kSynchronous);
  }

  base::test::TestFuture<std::optional<content::ClipboardPasteData>> future;
  PasteIfAllowedByPolicy(
      /*source=*/content::ClipboardEndpoint(
          ui::DataTransferEndpoint(GURL("https://foo.com")),
          base::BindLambdaForTesting(
              [this]() { return contents()->GetBrowserContext(); }),
          *contents()->GetPrimaryMainFrame()),
      /*destination=*/
      content::ClipboardEndpoint(
          ui::DataTransferEndpoint(GURL("https://google.com")),
          base::BindLambdaForTesting(
              [&destination_profile]() -> content::BrowserContext* {
                return destination_profile.get();
              }),
          *contents()->GetPrimaryMainFrame()),
      /*metadata=*/
      {.size = 1234}, MakeClipboardPasteData("text", "image", {}),
      future.GetCallback());

  ASSERT_FALSE(helper.dialog());
  auto paste_data = future.Get();
  EXPECT_TRUE(paste_data);
  EXPECT_EQ(paste_data->text, u"text");
  EXPECT_EQ(std::string(paste_data->png.begin(), paste_data->png.end()),
            "image");
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

IN_PROC_BROWSER_TEST_P(DataControlsClipboardUtilsBrowserTest, CopyAllowed) {
  auto event_validator = event_report_validator_helper_->CreateValidator();
  event_validator.ExpectNoReport();

  base::test::TestFuture<const ui::ClipboardFormatType&,
                         const content::ClipboardPasteData&,
                         std::optional<std::u16string>>
      future;
  IsClipboardCopyAllowedByPolicy(
      /*source=*/content::ClipboardEndpoint(
          ui::DataTransferEndpoint(GURL("https://google.com")),
          base::BindLambdaForTesting(
              [this]() { return contents()->GetBrowserContext(); }),
          *contents()->GetPrimaryMainFrame()),
      /*metadata=*/{.size = 1234}, MakeClipboardPasteData("foo", "", {}),
      future.GetCallback());

  auto data = future.Get<content::ClipboardPasteData>();
  EXPECT_EQ(data.text, u"foo");

  auto replacement = future.Get<std::optional<std::u16string>>();
  EXPECT_FALSE(replacement);
}

IN_PROC_BROWSER_TEST_P(DataControlsClipboardUtilsBrowserTest, CopyReported) {
  base::RunLoop run_loop;
  auto event_validator = event_report_validator_helper_->CreateValidator();
  event_validator.SetDoneClosure(run_loop.QuitClosure());
  event_validator.ExpectDataControlsSensitiveDataEvent(
      /*expected_url=*/
      kGoogleUrl,
      /*expected_tab_url=*/kGoogleUrl,
      /*source=*/kGoogleUrl,
      /*destination=*/"",
      /*mime_types=*/
      []() {
        static std::set<std::string> set = {"image/svg+xml"};
        return &set;
      }(),
      /*trigger=*/"CLIPBOARD_COPY",
      /*triggered_rules=*/{{0, {"1248", "report_only"}}},
      /*event_result=*/"EVENT_RESULT_ALLOWED",
      /*profile_username=*/kUserName,
      /*profile_identifier=*/browser()->profile()->GetPath().AsUTF8Unsafe(),
      /*content_size=*/1234);

  data_controls::SetDataControls(browser()->profile()->GetPrefs(), {R"({
                                   "name": "report_only",
                                   "rule_id": "1248",
                                   "sources": {
                                     "urls": ["google.com"]
                                   },
                                   "restrictions": [
                                     {"class": "CLIPBOARD", "level": "REPORT"}
                                   ]
                                 })"},
                                 machine_scope());

  base::test::TestFuture<const ui::ClipboardFormatType&,
                         const content::ClipboardPasteData&,
                         std::optional<std::u16string>>
      future;
  IsClipboardCopyAllowedByPolicy(
      /*source=*/content::ClipboardEndpoint(
          ui::DataTransferEndpoint(GURL("https://google.com")),
          base::BindLambdaForTesting(
              [this]() { return contents()->GetBrowserContext(); }),
          *contents()->GetPrimaryMainFrame()),
      /*metadata=*/
      {
          .size = 1234,
          .format_type = ui::ClipboardFormatType::SvgType(),
      },
      MakeClipboardPasteData("foo", "", {}), future.GetCallback());

  auto data = future.Get<content::ClipboardPasteData>();
  EXPECT_EQ(data.text, u"foo");

  auto replacement = future.Get<std::optional<std::u16string>>();
  EXPECT_FALSE(replacement);
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_P(DataControlsClipboardUtilsBrowserTest, CopyBlocked) {
  base::RunLoop run_loop;
  auto event_validator = event_report_validator_helper_->CreateValidator();
  event_validator.SetDoneClosure(run_loop.QuitClosure());
  event_validator.ExpectDataControlsSensitiveDataEvent(
      /*expected_url=*/
      kGoogleUrl,
      /*expected_tab_url=*/kGoogleUrl,
      /*source=*/kGoogleUrl,
      /*destination=*/"",
      /*mime_types=*/
      []() {
        static std::set<std::string> set = {"image/svg+xml"};
        return &set;
      }(),
      /*trigger=*/"CLIPBOARD_COPY",
      /*triggered_rules=*/{{0, {"987", "block"}}},
      /*event_result=*/"EVENT_RESULT_BLOCKED",
      /*profile_username=*/kUserName,
      /*profile_identifier=*/browser()->profile()->GetPath().AsUTF8Unsafe(),
      /*content_size=*/1234);

  data_controls::SetDataControls(browser()->profile()->GetPrefs(), {R"({
                                   "name": "block",
                                   "rule_id": "987",
                                   "sources": {
                                     "urls": ["google.com"]
                                   },
                                   "restrictions": [
                                     {"class": "CLIPBOARD", "level": "BLOCK"}
                                   ]
                                 })"},
                                 machine_scope());
  data_controls::DesktopDataControlsDialogTestHelper helper(
      data_controls::DataControlsDialog::Type::kClipboardCopyBlock);

  base::test::TestFuture<const ui::ClipboardFormatType&,
                         const content::ClipboardPasteData&,
                         std::optional<std::u16string>>
      future;
  IsClipboardCopyAllowedByPolicy(
      /*source=*/content::ClipboardEndpoint(
          ui::DataTransferEndpoint(GURL("https://google.com")),
          base::BindLambdaForTesting(
              [this]() { return contents()->GetBrowserContext(); }),
          *contents()->GetPrimaryMainFrame()),
      /*metadata=*/
      {
          .size = 1234,
          .format_type = ui::ClipboardFormatType::SvgType(),
      },
      MakeClipboardPasteData("foo", "", {}), future.GetCallback());

  helper.WaitForDialogToInitialize();
  helper.CloseDialogWithoutBypass();
  helper.WaitForDialogToClose();

  EXPECT_FALSE(future.IsReady());
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_P(DataControlsClipboardUtilsBrowserTest,
                       CopyWarnedThenCanceled) {
  base::RunLoop run_loop;
  auto event_validator = event_report_validator_helper_->CreateValidator();
  event_validator.SetDoneClosure(run_loop.QuitClosure());
  event_validator.ExpectDataControlsSensitiveDataEvent(
      /*expected_url=*/
      kGoogleUrl,
      /*expected_tab_url=*/kGoogleUrl,
      /*source=*/kGoogleUrl,
      /*destination=*/"",
      /*mime_types=*/
      []() {
        static std::set<std::string> set = {"image/png"};
        return &set;
      }(),
      /*trigger=*/"CLIPBOARD_COPY",
      /*triggered_rules=*/{{0, {"3927", "warn"}}},
      /*event_result=*/"EVENT_RESULT_WARNED",
      /*profile_username=*/kUserName,
      /*profile_identifier=*/browser()->profile()->GetPath().AsUTF8Unsafe(),
      /*content_size=*/1234);

  data_controls::SetDataControls(browser()->profile()->GetPrefs(), {R"({
                                   "name": "warn",
                                   "rule_id": "3927",
                                   "sources": {
                                     "urls": ["google.com"]
                                   },
                                   "restrictions": [
                                     {"class": "CLIPBOARD", "level": "WARN"}
                                   ]
                                 })"},
                                 machine_scope());
  data_controls::DesktopDataControlsDialogTestHelper helper(
      data_controls::DataControlsDialog::Type::kClipboardCopyWarn);

  base::test::TestFuture<const ui::ClipboardFormatType&,
                         const content::ClipboardPasteData&,
                         std::optional<std::u16string>>
      future;
  IsClipboardCopyAllowedByPolicy(
      /*source=*/content::ClipboardEndpoint(
          ui::DataTransferEndpoint(GURL("https://google.com")),
          base::BindLambdaForTesting(
              [this]() { return contents()->GetBrowserContext(); }),
          *contents()->GetPrimaryMainFrame()),
      /*metadata=*/
      {
          .size = 1234,
          .format_type = ui::ClipboardFormatType::PngType(),
      },
      MakeClipboardPasteData("foo", "", {}), future.GetCallback());

  helper.WaitForDialogToInitialize();

  // The dialog will stay up until a user action dismisses it, so `future`
  // shouldn't be ready yet.
  EXPECT_FALSE(future.IsReady());

  helper.CloseDialogWithoutBypass();
  helper.WaitForDialogToClose();

  auto data = future.Get<content::ClipboardPasteData>();
  EXPECT_EQ(data.text, u"");

  auto replacement = future.Get<std::optional<std::u16string>>();
  EXPECT_FALSE(replacement);
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_P(DataControlsClipboardUtilsBrowserTest,
                       CopyWarnedThenCanceled_OsClipboardDestination) {
  base::RunLoop run_loop;
  auto event_validator = event_report_validator_helper_->CreateValidator();
  event_validator.SetDoneClosure(run_loop.QuitClosure());
  event_validator.ExpectDataControlsSensitiveDataEvent(
      /*expected_url=*/
      kGoogleUrl,
      /*expected_tab_url=*/kGoogleUrl,
      /*source=*/kGoogleUrl,
      /*destination=*/"",
      /*mime_types=*/
      []() {
        static std::set<std::string> set = {"text/plain"};
        return &set;
      }(),
      /*trigger=*/"CLIPBOARD_COPY",
      /*triggered_rules=*/{{0, {"101", "warn_cancel"}}},
      /*event_result=*/"EVENT_RESULT_WARNED",
      /*profile_username=*/kUserName,
      /*profile_identifier=*/browser()->profile()->GetPath().AsUTF8Unsafe(),
      /*content_size=*/1234);

  data_controls::SetDataControls(browser()->profile()->GetPrefs(), {R"({
                                   "name": "warn_cancel",
                                   "rule_id": "101",
                                   "sources": {
                                     "urls": ["google.com"]
                                   },
                                   "destinations": {
                                     "os_clipboard": true
                                   },
                                   "restrictions": [
                                     {"class": "CLIPBOARD", "level": "WARN"}
                                   ]
                                 })"},
                                 machine_scope());
  data_controls::DesktopDataControlsDialogTestHelper helper(
      data_controls::DataControlsDialog::Type::kClipboardCopyWarn);

  base::test::TestFuture<const ui::ClipboardFormatType&,
                         const content::ClipboardPasteData&,
                         std::optional<std::u16string>>
      future;
  IsClipboardCopyAllowedByPolicy(
      /*source=*/content::ClipboardEndpoint(
          ui::DataTransferEndpoint(GURL("https://google.com")),
          base::BindLambdaForTesting(
              [this]() { return contents()->GetBrowserContext(); }),
          *contents()->GetPrimaryMainFrame()),
      /*metadata=*/
      {
          .size = 1234,
          .format_type = ui::ClipboardFormatType::PlainTextType(),
      },
      MakeClipboardPasteData("foo", "", {}), future.GetCallback());

  helper.WaitForDialogToInitialize();

  // The dialog will stay up until a user action dismisses it, so `future`
  // shouldn't be ready yet.
  EXPECT_FALSE(future.IsReady());

  helper.CloseDialogWithoutBypass();
  helper.WaitForDialogToClose();

  auto data = future.Get<content::ClipboardPasteData>();
  EXPECT_EQ(data.text, u"");

  auto replacement = future.Get<std::optional<std::u16string>>();
  EXPECT_FALSE(replacement);
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_P(DataControlsClipboardUtilsBrowserTest,
                       CopyWarnedThenBypassed) {
  base::RunLoop run_loop_warn;
  auto event_validator = event_report_validator_helper_->CreateValidator();
  event_validator.SetDoneClosure(run_loop_warn.QuitClosure());
  event_validator.ExpectDataControlsSensitiveDataEvent(
      /*expected_url=*/
      kGoogleUrl,
      /*expected_tab_url=*/kGoogleUrl,
      /*source=*/kGoogleUrl,
      /*destination=*/"",
      /*mime_types=*/
      []() {
        static std::set<std::string> set = {"text/html"};
        return &set;
      }(),
      /*trigger=*/"CLIPBOARD_COPY",
      /*triggered_rules=*/{{0, {"12345", "warn_bypass"}}},
      /*event_result=*/"EVENT_RESULT_WARNED",
      /*profile_username=*/kUserName,
      /*profile_identifier=*/browser()->profile()->GetPath().AsUTF8Unsafe(),
      /*content_size=*/1234);

  data_controls::SetDataControls(browser()->profile()->GetPrefs(), {R"({
                                   "name": "warn_bypass",
                                   "rule_id": "12345",
                                   "sources": {
                                     "urls": ["google.com"]
                                   },
                                   "restrictions": [
                                     {"class": "CLIPBOARD", "level": "WARN"}
                                   ]
                                 })"},
                                 machine_scope());
  data_controls::DesktopDataControlsDialogTestHelper helper(
      data_controls::DataControlsDialog::Type::kClipboardCopyWarn);

  base::test::TestFuture<const ui::ClipboardFormatType&,
                         const content::ClipboardPasteData&,
                         std::optional<std::u16string>>
      future;
  IsClipboardCopyAllowedByPolicy(
      /*source=*/content::ClipboardEndpoint(
          ui::DataTransferEndpoint(GURL("https://google.com")),
          base::BindLambdaForTesting(
              [this]() { return contents()->GetBrowserContext(); }),
          *contents()->GetPrimaryMainFrame()),
      /*metadata=*/
      {
          .size = 1234,
          .format_type = ui::ClipboardFormatType::HtmlType(),
      },
      MakeClipboardPasteData("foo", "", {}), future.GetCallback());

  helper.WaitForDialogToInitialize();

  run_loop_warn.Run();

  // The dialog will stay up until a user action dismisses it, so `future`
  // shouldn't be ready yet.
  EXPECT_FALSE(future.IsReady());

  // The first warn event should already be reported before the dialog has been
  // initialized, so it can be reassigned so that the bypass event can be
  // validated.
  base::RunLoop run_loop_bypass;
  event_validator = event_report_validator_helper_->CreateValidator();
  event_validator.SetDoneClosure(run_loop_bypass.QuitClosure());
  event_validator.ExpectDataControlsSensitiveDataEvent(
      /*expected_url=*/
      kGoogleUrl,
      /*expected_tab_url=*/kGoogleUrl,
      /*source=*/kGoogleUrl,
      /*destination=*/"",
      /*mime_types=*/
      []() {
        static std::set<std::string> set = {"text/html"};
        return &set;
      }(),
      /*trigger=*/"CLIPBOARD_COPY",
      /*triggered_rules=*/{{0, {"12345", "warn_bypass"}}},
      /*event_result=*/"EVENT_RESULT_BYPASSED",
      /*profile_username=*/kUserName,
      /*profile_identifier=*/browser()->profile()->GetPath().AsUTF8Unsafe(),
      /*content_size=*/1234);

  helper.BypassWarning();
  helper.WaitForDialogToClose();

  auto data = future.Get<content::ClipboardPasteData>();
  EXPECT_EQ(data.text, u"foo");

  auto replacement = future.Get<std::optional<std::u16string>>();
  EXPECT_FALSE(replacement);
  run_loop_bypass.Run();
}

IN_PROC_BROWSER_TEST_P(DataControlsClipboardUtilsBrowserTest,
                       CopyWarnedThenBypassed_OsClipboardDestination) {
  base::RunLoop run_loop_warn;
  auto event_validator = event_report_validator_helper_->CreateValidator();
  event_validator.SetDoneClosure(run_loop_warn.QuitClosure());
  event_validator.ExpectDataControlsSensitiveDataEvent(
      /*expected_url=*/
      kGoogleUrl,
      /*expected_tab_url=*/kGoogleUrl,
      /*source=*/kGoogleUrl,
      /*destination=*/"",
      /*mime_types=*/
      []() {
        static std::set<std::string> set = {"text/html"};
        return &set;
      }(),
      /*trigger=*/"CLIPBOARD_COPY",
      /*triggered_rules=*/{{0, {"111", "warn_bypass_os"}}},
      /*event_result=*/"EVENT_RESULT_WARNED",
      /*profile_username=*/kUserName,
      /*profile_identifier=*/browser()->profile()->GetPath().AsUTF8Unsafe(),
      /*content_size=*/1234);

  data_controls::SetDataControls(browser()->profile()->GetPrefs(), {R"({
                                   "name": "warn_bypass_os",
                                   "rule_id": "111",
                                   "sources": {
                                     "urls": ["google.com"]
                                   },
                                   "destinations": {
                                     "os_clipboard": true
                                   },
                                   "restrictions": [
                                     {"class": "CLIPBOARD", "level": "WARN"}
                                   ]
                                 })"},
                                 machine_scope());
  data_controls::DesktopDataControlsDialogTestHelper helper(
      data_controls::DataControlsDialog::Type::kClipboardCopyWarn);

  base::test::TestFuture<const ui::ClipboardFormatType&,
                         const content::ClipboardPasteData&,
                         std::optional<std::u16string>>
      future;
  IsClipboardCopyAllowedByPolicy(
      /*source=*/content::ClipboardEndpoint(
          ui::DataTransferEndpoint(GURL("https://google.com")),
          base::BindLambdaForTesting(
              [this]() { return contents()->GetBrowserContext(); }),
          *contents()->GetPrimaryMainFrame()),
      /*metadata=*/
      {
          .size = 1234,
          .format_type = ui::ClipboardFormatType::HtmlType(),
      },
      MakeClipboardPasteData("foo", "", {}), future.GetCallback());

  helper.WaitForDialogToInitialize();

  // The dialog will stay up until a user action dismisses it, so `future`
  // shouldn't be ready yet.
  EXPECT_FALSE(future.IsReady());
  run_loop_warn.Run();

  // The first warn event should already be reported before the dialog has been
  // initialized, so it can be reassigned so that the bypass event can be
  // validated.
  base::RunLoop run_loop_bypass;
  event_validator = event_report_validator_helper_->CreateValidator();
  event_validator.SetDoneClosure(run_loop_bypass.QuitClosure());
  event_validator.ExpectDataControlsSensitiveDataEvent(
      /*expected_url=*/
      kGoogleUrl,
      /*expected_tab_url=*/kGoogleUrl,
      /*source=*/kGoogleUrl,
      /*destination=*/"",
      /*mime_types=*/
      []() {
        static std::set<std::string> set = {"text/html"};
        return &set;
      }(),
      /*trigger=*/"CLIPBOARD_COPY",
      /*triggered_rules=*/{{0, {"111", "warn_bypass_os"}}},
      /*event_result=*/"EVENT_RESULT_BYPASSED",
      /*profile_username=*/kUserName,
      /*profile_identifier=*/browser()->profile()->GetPath().AsUTF8Unsafe(),
      /*content_size=*/1234);

  helper.BypassWarning();
  helper.WaitForDialogToClose();

  auto data = future.Get<content::ClipboardPasteData>();
  EXPECT_EQ(data.text, u"foo");

  auto replacement = future.Get<std::optional<std::u16string>>();
  EXPECT_FALSE(replacement);
  run_loop_bypass.Run();
}

}  // namespace enterprise_data_protection
