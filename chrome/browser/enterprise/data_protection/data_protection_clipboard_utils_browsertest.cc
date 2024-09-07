// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/data_protection/data_protection_clipboard_utils.h"

#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/connectors/test/deep_scanning_test_utils.h"
#include "chrome/browser/enterprise/data_controls/desktop_data_controls_dialog.h"
#include "chrome/browser/enterprise/data_controls/desktop_data_controls_dialog_test_helper.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/enterprise/data_controls/core/browser/features.h"
#include "components/enterprise/data_controls/core/browser/test_utils.h"
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

// Tests for functions and classes declared in data_protection_clipboard_utils.h
// For browser tests that test data protection integration with Chrome's
// clipboard logic, see clipboard_browsertests.cc
class DataControlsClipboardUtilsBrowserTest
    : public InProcessBrowserTest,
      public testing::WithParamInterface<bool> {
 public:
  DataControlsClipboardUtilsBrowserTest() {
    scoped_features_.InitAndEnableFeature(
        data_controls::kEnableDesktopDataControls);
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
  base::test::ScopedFeatureList scoped_features_;
  std::unique_ptr<enterprise_connectors::test::EventReportValidatorHelper>
      event_report_validator_helper_;
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
  auto event_validator = event_report_validator_helper_->CreateValidator();
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
      /*triggered_rules=*/{{0, {"id", "name"}}},
      /*event_result=*/"EVENT_RESULT_BLOCKED",
      /*profile_username=*/kUserName,
      /*profile_identifier=*/browser()->profile()->GetPath().AsUTF8Unsafe(),
      /*content_size=*/1234);

  data_controls::SetDataControls(browser()->profile()->GetPrefs(), {R"({
                                   "name": "name",
                                   "rule_id": "id",
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
}

IN_PROC_BROWSER_TEST_P(DataControlsClipboardUtilsBrowserTest,
                       PasteBlockedByDataControls_DestinationRule) {
  auto event_validator = event_report_validator_helper_->CreateValidator();
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
      /*triggered_rules=*/{{0, {"rule_id", "rule_name"}}},
      /*event_result=*/"EVENT_RESULT_BLOCKED",
      /*profile_username=*/kUserName,
      /*profile_identifier=*/browser()->profile()->GetPath().AsUTF8Unsafe(),
      /*content_size=*/1234);

  data_controls::SetDataControls(browser()->profile()->GetPrefs(), {R"({
                                   "name": "rule_name",
                                   "rule_id": "rule_id",
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
}

IN_PROC_BROWSER_TEST_P(DataControlsClipboardUtilsBrowserTest,
                       PasteWarnedByDataControls_BypassedDestinationRule) {
  auto event_validator = event_report_validator_helper_->CreateValidator();
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
      /*triggered_rules=*/{{0, {"warn_rule_ID", "warn_rule_name"}}},
      /*event_result=*/"EVENT_RESULT_WARNED",
      /*profile_username=*/kUserName,
      /*profile_identifier=*/browser()->profile()->GetPath().AsUTF8Unsafe(),
      /*content_size=*/1234);

  data_controls::SetDataControls(browser()->profile()->GetPrefs(), {R"({
                                   "name": "warn_rule_name",
                                   "rule_id": "warn_rule_ID",
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

  // The first warn event should already be reported before the dialog has been
  // initialized, so it can be reassigned so that the bypass event can be
  // validated.
  event_validator = event_report_validator_helper_->CreateValidator();
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
      /*triggered_rules=*/{{0, {"warn_rule_ID", "warn_rule_name"}}},
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
}

IN_PROC_BROWSER_TEST_P(DataControlsClipboardUtilsBrowserTest,
                       PasteWarnedByDataControls_CanceledDestinationRule) {
  auto event_validator = event_report_validator_helper_->CreateValidator();
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
      /*triggered_rules=*/{{0, {"warn rule ID", "warn rule name"}}},
      /*event_result=*/"EVENT_RESULT_WARNED",
      /*profile_username=*/kUserName,
      /*profile_identifier=*/browser()->profile()->GetPath().AsUTF8Unsafe(),
      /*content_size=*/1234);

  data_controls::SetDataControls(browser()->profile()->GetPrefs(), {R"({
                                   "name": "warn rule name",
                                   "rule_id": "warn rule ID",
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
}

// Ash requires extra boilerplate to run this test, and since copy-pasting
// between profiles on Ash isn't a meaningful test it is simply omitted from
// running this.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
IN_PROC_BROWSER_TEST_P(DataControlsClipboardUtilsBrowserTest,
                       PasteBlockedByDataControls_SourceRule) {
  auto event_validator = event_report_validator_helper_->CreateValidator();
  event_validator.ExpectNoReport();

  data_controls::SetDataControls(browser()->profile()->GetPrefs(), {R"({
                                   "name": "report_rule_name",
                                   "rule_id": "report_rule_ID",
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
                                   "rule_id": "report_rule_ID",
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
                                   "rule_id": "report_rule_ID",
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
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

IN_PROC_BROWSER_TEST_P(DataControlsClipboardUtilsBrowserTest,
                       PasteReportedByDataControls_DestinationRule) {
  auto event_validator = event_report_validator_helper_->CreateValidator();
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
      /*triggered_rules=*/{{0, {"report_rule_ID", "report_rule_name"}}},
      /*event_result=*/"EVENT_RESULT_ALLOWED",
      /*profile_username=*/kUserName,
      /*profile_identifier=*/browser()->profile()->GetPath().AsUTF8Unsafe(),
      /*content_size=*/1234);

  data_controls::SetDataControls(browser()->profile()->GetPrefs(), {R"({
                                   "name": "report_rule_name",
                                   "rule_id": "report_rule_ID",
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
}

// Ash requires extra boilerplate to run this test, and since copy-pasting
// between profiles on Ash isn't a meaningful test it is simply omitted from
// running this.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
IN_PROC_BROWSER_TEST_P(DataControlsClipboardUtilsBrowserTest,
                       PasteReportedByDataControls_SourceRule) {
  auto event_validator = event_report_validator_helper_->CreateValidator();
  event_validator.ExpectNoReport();

  data_controls::SetDataControls(browser()->profile()->GetPrefs(), {R"({
                                   "name": "report_rule_name",
                                   "rule_id": "report_rule_ID",
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
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

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
  auto event_validator = event_report_validator_helper_->CreateValidator();
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
      /*triggered_rules=*/{{0, {"report_only_id", "report_only"}}},
      /*event_result=*/"EVENT_RESULT_ALLOWED",
      /*profile_username=*/kUserName,
      /*profile_identifier=*/browser()->profile()->GetPath().AsUTF8Unsafe(),
      /*content_size=*/1234);

  data_controls::SetDataControls(browser()->profile()->GetPrefs(), {R"({
                                   "name": "report_only",
                                   "rule_id": "report_only_id",
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
}

IN_PROC_BROWSER_TEST_P(DataControlsClipboardUtilsBrowserTest, CopyBlocked) {
  auto event_validator = event_report_validator_helper_->CreateValidator();
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
      /*triggered_rules=*/{{0, {"block_id", "block"}}},
      /*event_result=*/"EVENT_RESULT_BLOCKED",
      /*profile_username=*/kUserName,
      /*profile_identifier=*/browser()->profile()->GetPath().AsUTF8Unsafe(),
      /*content_size=*/1234);

  data_controls::SetDataControls(browser()->profile()->GetPrefs(), {R"({
                                   "name": "block",
                                   "rule_id": "block_id",
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
}

IN_PROC_BROWSER_TEST_P(DataControlsClipboardUtilsBrowserTest,
                       CopyWarnedThenCanceled) {
  auto event_validator = event_report_validator_helper_->CreateValidator();
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
      /*triggered_rules=*/{{0, {"warn_id", "warn"}}},
      /*event_result=*/"EVENT_RESULT_WARNED",
      /*profile_username=*/kUserName,
      /*profile_identifier=*/browser()->profile()->GetPath().AsUTF8Unsafe(),
      /*content_size=*/1234);

  data_controls::SetDataControls(browser()->profile()->GetPrefs(), {R"({
                                   "name": "warn",
                                   "rule_id": "warn_id",
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

  EXPECT_FALSE(future.IsReady());
}

IN_PROC_BROWSER_TEST_P(DataControlsClipboardUtilsBrowserTest,
                       CopyWarnedThenCanceled_OsClipboardDestination) {
  auto event_validator = event_report_validator_helper_->CreateValidator();
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
      /*triggered_rules=*/{{0, {"warn_cancel_id", "warn_cancel"}}},
      /*event_result=*/"EVENT_RESULT_WARNED",
      /*profile_username=*/kUserName,
      /*profile_identifier=*/browser()->profile()->GetPath().AsUTF8Unsafe(),
      /*content_size=*/1234);

  data_controls::SetDataControls(browser()->profile()->GetPrefs(), {R"({
                                   "name": "warn_cancel",
                                   "rule_id": "warn_cancel_id",
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

  EXPECT_FALSE(future.IsReady());
}

IN_PROC_BROWSER_TEST_P(DataControlsClipboardUtilsBrowserTest,
                       CopyWarnedThenBypassed) {
  auto event_validator = event_report_validator_helper_->CreateValidator();
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
      /*triggered_rules=*/{{0, {"warn_bypass_id", "warn_bypass"}}},
      /*event_result=*/"EVENT_RESULT_WARNED",
      /*profile_username=*/kUserName,
      /*profile_identifier=*/browser()->profile()->GetPath().AsUTF8Unsafe(),
      /*content_size=*/1234);

  data_controls::SetDataControls(browser()->profile()->GetPrefs(), {R"({
                                   "name": "warn_bypass",
                                   "rule_id": "warn_bypass_id",
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

  // The dialog will stay up until a user action dismisses it, so `future`
  // shouldn't be ready yet.
  EXPECT_FALSE(future.IsReady());

  // The first warn event should already be reported before the dialog has been
  // initialized, so it can be reassigned so that the bypass event can be
  // validated.
  event_validator = event_report_validator_helper_->CreateValidator();
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
      /*triggered_rules=*/{{0, {"warn_bypass_id", "warn_bypass"}}},
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
}

IN_PROC_BROWSER_TEST_P(DataControlsClipboardUtilsBrowserTest,
                       CopyWarnedThenBypassed_OsClipboardDestination) {
  auto event_validator = event_report_validator_helper_->CreateValidator();
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
      /*triggered_rules=*/{{0, {"warn_bypass_os_id", "warn_bypass_os"}}},
      /*event_result=*/"EVENT_RESULT_WARNED",
      /*profile_username=*/kUserName,
      /*profile_identifier=*/browser()->profile()->GetPath().AsUTF8Unsafe(),
      /*content_size=*/1234);

  data_controls::SetDataControls(browser()->profile()->GetPrefs(), {R"({
                                   "name": "warn_bypass_os",
                                   "rule_id": "warn_bypass_os_id",
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

  // The first warn event should already be reported before the dialog has been
  // initialized, so it can be reassigned so that the bypass event can be
  // validated.
  event_validator = event_report_validator_helper_->CreateValidator();
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
      /*triggered_rules=*/{{0, {"warn_bypass_os_id", "warn_bypass_os"}}},
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
}

}  // namespace enterprise_data_protection
