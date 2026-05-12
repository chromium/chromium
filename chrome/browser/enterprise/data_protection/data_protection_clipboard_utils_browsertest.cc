// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/data_protection/data_protection_clipboard_utils.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/connectors/test/active_user_test_mixin.h"
#include "chrome/browser/enterprise/connectors/test/deep_scanning_test_utils.h"
#include "chrome/browser/enterprise/data_controls/desktop_data_controls_dialog.h"
#include "chrome/browser/enterprise/data_controls/desktop_data_controls_dialog_test_helper.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "components/enterprise/connectors/core/features.h"
#include "components/enterprise/connectors/core/reporting_test_utils.h"
#include "components/enterprise/data_controls/content/browser/last_replaced_clipboard_data.h"
#include "components/enterprise/data_controls/core/browser/features.h"
#include "components/enterprise/data_controls/core/browser/test_utils.h"
#include "components/policy/core/common/cloud/realtime_reporting_job_configuration.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/clipboard_types.h"
#include "content/public/common/drop_data.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "ui/base/clipboard/clipboard_metadata.h"
#include "ui/base/clipboard/clipboard_monitor.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/base/clipboard/test/test_clipboard.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/widget/widget_delegate.h"

namespace enterprise_data_protection {

namespace {

constexpr char kWorkspaceUrlForUser0[] = "https://docs.google.com/u/0/";
constexpr char kWorkspaceUrlForUser1[] =
    "https://mail.google.com/foo/bar?authuser=1";
constexpr char kNonWorkspaceUrl[] = "https://not.workspace.com/";
constexpr char kUserName[] = "test-user@chromium.org";

constexpr char kContentAreaUser0[] = "foo@gmail.com";
constexpr char kContentAreaUser1[] = "bar@gmail.com";

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
    : public MixinBasedInProcessBrowserTest,
      public testing::WithParamInterface<std::tuple<bool, bool>> {
 public:
  DataControlsClipboardUtilsBrowserTest() {
    std::vector<base::test::FeatureRef> enabled_features = {
        data_controls::kDataControlsDragEnforcement,
        data_controls::kDataControlsSearchWith};
    std::vector<base::test::FeatureRef> disabled_features = {};

    scoped_feature_list_.InitWithFeatures(enabled_features, disabled_features);
    active_user_test_mixin_ =
        std::make_unique<enterprise_connectors::test::ActiveUserTestMixin>(
            &mixin_host_, this, &embedded_https_test_server(),
            std::vector<const char*>({kContentAreaUser0, kContentAreaUser1}));

    ui::TestClipboard::CreateForCurrentThread();
  }

  ~DataControlsClipboardUtilsBrowserTest() override {
    ui::Clipboard::DestroyClipboardForCurrentThread();
  }

  bool machine_scope() const { return std::get<0>(GetParam()); }

  bool use_workspace_urls() const { return std::get<1>(GetParam()); }

  const char* test_url_0() const {
    return use_workspace_urls() ? kWorkspaceUrlForUser0 : kNonWorkspaceUrl;
  }

  const char* test_url_1() const {
    return use_workspace_urls() ? kWorkspaceUrlForUser1 : kNonWorkspaceUrl;
  }

  void SetUpOnMainThread() override {
    event_report_validator_helper_ = std::make_unique<
        enterprise_connectors::test::EventReportValidatorHelper>(
        browser()->profile(), /*browser_test=*/true);

    MixinBasedInProcessBrowserTest::SetUpOnMainThread();
  }

  void TearDownOnMainThread() override {
    event_report_validator_helper_.reset();

    MixinBasedInProcessBrowserTest::TearDownOnMainThread();
  }

  content::WebContents* contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  content::ClipboardEndpoint CreateURLClipboardEndpoint(const char* url) {
    return content::ClipboardEndpoint(ui::DataTransferEndpoint(GURL(url)),
                                      base::BindLambdaForTesting([this]() {
                                        return contents()->GetBrowserContext();
                                      }),
                                      *contents()->GetPrimaryMainFrame());
  }

  Profile* CreateAdditionalProfile() {
    ProfileManager* profile_manager = g_browser_process->profile_manager();

    base::FilePath new_path =
        profile_manager->GenerateNextProfileDirectoryPath();
    Profile& profile =
        profiles::testing::CreateProfileSync(profile_manager, new_path);
    return &profile;
  }

 protected:
  std::unique_ptr<enterprise_connectors::test::EventReportValidatorHelper>
      event_report_validator_helper_;
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<enterprise_connectors::test::ActiveUserTestMixin>
      active_user_test_mixin_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         DataControlsClipboardUtilsBrowserTest,
                         testing::Combine(testing::Bool(),
                                          testing::Bool()));

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
      /*destination=*/CreateURLClipboardEndpoint("https://google.com"),
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
  event_validator.ExpectNoReport();

  data_controls::DesktopDataControlsDialogTestHelper helper(
      data_controls::DataControlsDialog::Type::kClipboardPasteBlock);
  base::test::TestFuture<std::optional<content::ClipboardPasteData>> future;
  PasteIfAllowedByPolicy(
      /*source=*/CreateURLClipboardEndpoint(test_url_0()),
      /*destination=*/CreateURLClipboardEndpoint(test_url_0()),
      /*metadata=*/
      {
          .size = 1234,
          .format_type = ui::ClipboardFormatType::HtmlType(),
      },
      MakeClipboardPasteData("text", "image", {}), future.GetCallback());

  auto paste_data = future.Get();
  EXPECT_TRUE(paste_data);
  EXPECT_EQ(paste_data->text, u"text");
  EXPECT_EQ(std::string(paste_data->png.begin(), paste_data->png.end()),
            "image");

  EXPECT_FALSE(helper.dialog());
}

IN_PROC_BROWSER_TEST_P(DataControlsClipboardUtilsBrowserTest,
                       PasteBlockedByDataControls_DestinationRule) {
  active_user_test_mixin_->SetFakeCookieValue();

  base::RunLoop run_loop;
  auto event_validator = event_report_validator_helper_->CreateValidator();
  event_validator.SetDoneClosure(run_loop.QuitClosure());

  chrome::cros::reporting::proto::DlpSensitiveDataEvent expected_event;
  if (use_workspace_urls()) {
    expected_event.set_web_app_signed_in_account(kContentAreaUser1);
  }
  expected_event.set_url(test_url_1());
  expected_event.set_tab_url(test_url_1());
  expected_event.set_source("CLIPBOARD");
  expected_event.set_destination(test_url_1());
  expected_event.set_content_type("text/plain");
  expected_event.set_content_size(1234);
  expected_event.set_trigger(chrome::cros::reporting::proto::
                                 DataTransferEventTrigger::WEB_CONTENT_UPLOAD);
  expected_event.set_event_result(
      chrome::cros::reporting::proto::EventResult::EVENT_RESULT_BLOCKED);

  ::chrome::cros::reporting::proto::TriggeredRuleInfo triggered_rule;
  triggered_rule.set_rule_id(222);
  triggered_rule.set_rule_name("rule_name");

  *expected_event.add_triggered_rule_info() = triggered_rule;
  expected_event.set_profile_identifier(
      browser()->profile()->GetPath().AsUTF8Unsafe());
  expected_event.set_profile_user_name(kUserName);

  event_validator.ExpectSensitiveDataEvent(std::move(expected_event));

  data_controls::SetDataControls(browser()->profile()->GetPrefs(), {R"({
                                   "name": "rule_name",
                                   "rule_id": "222",
                                   "destinations": {
                                     "urls": ["google.com", "not.workspace.com"]
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
      /*destination=*/CreateURLClipboardEndpoint(test_url_1()),
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
  active_user_test_mixin_->SetFakeCookieValue();

  base::RunLoop run_loop_warn;
  auto event_validator = event_report_validator_helper_->CreateValidator();
  event_validator.SetDoneClosure(run_loop_warn.QuitClosure());

  chrome::cros::reporting::proto::DlpSensitiveDataEvent
      expected_warned_dlp_event;
  if (use_workspace_urls()) {
    expected_warned_dlp_event.set_web_app_signed_in_account(kContentAreaUser0);
  }
  expected_warned_dlp_event.set_url(test_url_0());
  expected_warned_dlp_event.set_tab_url(test_url_0());
  expected_warned_dlp_event.set_source("CLIPBOARD");
  expected_warned_dlp_event.set_destination(test_url_0());
  expected_warned_dlp_event.set_content_type("image/svg+xml");
  expected_warned_dlp_event.set_content_size(1234);
  expected_warned_dlp_event.set_trigger(
      chrome::cros::reporting::proto::DataTransferEventTrigger::
          WEB_CONTENT_UPLOAD);
  expected_warned_dlp_event.set_event_result(
      chrome::cros::reporting::proto::EventResult::EVENT_RESULT_WARNED);

  ::chrome::cros::reporting::proto::TriggeredRuleInfo triggered_rule;
  triggered_rule.set_rule_id(333);
  triggered_rule.set_rule_name("warn_rule_name");

  *expected_warned_dlp_event.add_triggered_rule_info() = triggered_rule;
  expected_warned_dlp_event.set_profile_identifier(
      browser()->profile()->GetPath().AsUTF8Unsafe());
  expected_warned_dlp_event.set_profile_user_name(kUserName);

  event_validator.ExpectSensitiveDataEvent(
      std::move(expected_warned_dlp_event));

  data_controls::SetDataControls(browser()->profile()->GetPrefs(), {R"({
                                   "name": "warn_rule_name",
                                   "rule_id": "333",
                                   "destinations": {
                                     "urls": ["google.com", "not.workspace.com"]
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
      /*destination=*/CreateURLClipboardEndpoint(test_url_0()),
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

  chrome::cros::reporting::proto::DlpSensitiveDataEvent
      expected_bypassed_dlp_event;
  if (use_workspace_urls()) {
    expected_bypassed_dlp_event.set_web_app_signed_in_account(
        kContentAreaUser0);
  }
  expected_bypassed_dlp_event.set_url(test_url_0());
  expected_bypassed_dlp_event.set_tab_url(test_url_0());
  expected_bypassed_dlp_event.set_source("CLIPBOARD");
  expected_bypassed_dlp_event.set_destination(test_url_0());
  expected_bypassed_dlp_event.set_content_type("image/svg+xml");
  expected_bypassed_dlp_event.set_content_size(1234);
  expected_bypassed_dlp_event.set_trigger(
      chrome::cros::reporting::proto::DataTransferEventTrigger::
          WEB_CONTENT_UPLOAD);
  expected_bypassed_dlp_event.set_event_result(
      chrome::cros::reporting::proto::EventResult::EVENT_RESULT_BYPASSED);

  ::chrome::cros::reporting::proto::TriggeredRuleInfo triggered_bypassed_rule;
  triggered_bypassed_rule.set_rule_id(333);
  triggered_bypassed_rule.set_rule_name("warn_rule_name");

  *expected_bypassed_dlp_event.add_triggered_rule_info() =
      triggered_bypassed_rule;
  expected_bypassed_dlp_event.set_profile_identifier(
      browser()->profile()->GetPath().AsUTF8Unsafe());
  expected_bypassed_dlp_event.set_profile_user_name(kUserName);

  event_validator.ExpectSensitiveDataEvent(
      std::move(expected_bypassed_dlp_event));

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
  active_user_test_mixin_->SetFakeCookieValue();

  base::RunLoop run_loop;
  auto event_validator = event_report_validator_helper_->CreateValidator();
  event_validator.SetDoneClosure(run_loop.QuitClosure());

  chrome::cros::reporting::proto::DlpSensitiveDataEvent expected_event;
  if (use_workspace_urls()) {
    expected_event.set_web_app_signed_in_account(kContentAreaUser1);
  }
  expected_event.set_url(test_url_1());
  expected_event.set_tab_url(test_url_1());
  expected_event.set_source("CLIPBOARD");
  expected_event.set_destination(test_url_1());
  expected_event.set_content_type("image/png");
  expected_event.set_content_size(1234);
  expected_event.set_trigger(chrome::cros::reporting::proto::
                                 DataTransferEventTrigger::WEB_CONTENT_UPLOAD);
  expected_event.set_event_result(
      chrome::cros::reporting::proto::EventResult::EVENT_RESULT_WARNED);

  ::chrome::cros::reporting::proto::TriggeredRuleInfo triggered_rule;
  triggered_rule.set_rule_id(1416);
  triggered_rule.set_rule_name("warn rule name");

  *expected_event.add_triggered_rule_info() = triggered_rule;
  expected_event.set_profile_identifier(
      browser()->profile()->GetPath().AsUTF8Unsafe());
  expected_event.set_profile_user_name(kUserName);

  event_validator.ExpectSensitiveDataEvent(std::move(expected_event));

  data_controls::SetDataControls(browser()->profile()->GetPrefs(), {R"({
                                   "name": "warn rule name",
                                   "rule_id": "1416",
                                   "destinations": {
                                     "urls": ["google.com", "not.workspace.com"]
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
      /*destination=*/CreateURLClipboardEndpoint(test_url_1()),
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
  // By making a new profile for this test, we ensure we can prevent pasting to
  // it by having the rule set in the source profile only.
  Profile* source_profile = CreateAdditionalProfile();
  data_controls::SetDataControls(source_profile->GetPrefs(), {R"({
                                   "name": "report_rule_name",
                                   "rule_id": "4321",
                                   "destinations": {
                                     "urls": ["google.com", "not.workspace.com"]
                                   },
                                   "restrictions": [
                                     {"class": "CLIPBOARD", "level": "BLOCK"}
                                   ]
                                 })"},
                                 machine_scope());

  data_controls::DesktopDataControlsDialogTestHelper helper(
      data_controls::DataControlsDialog::Type::kClipboardPasteBlock);

  base::RunLoop report_run_loop;
  auto event_validator = event_report_validator_helper_->CreateValidator();

  // The event should only be reported when the policies are set at the machine
  // level as we would otherwise be reporting from a different unmanaged or
  // unaffiliated profile.
  if (machine_scope()) {
    event_validator.SetDoneClosure(report_run_loop.QuitClosure());
    chrome::cros::reporting::proto::DlpSensitiveDataEvent expected_event;
    expected_event.set_url(test_url_1());
    expected_event.set_tab_url(test_url_1());
    expected_event.set_source("OTHER_PROFILE");
    expected_event.set_destination(test_url_1());
    expected_event.set_content_type("text/plain");
    expected_event.set_content_size(1234);
    expected_event.set_trigger(
        chrome::cros::reporting::proto::DataTransferEventTrigger::
            WEB_CONTENT_UPLOAD);
    expected_event.set_event_result(
        chrome::cros::reporting::proto::EventResult::EVENT_RESULT_BLOCKED);

    ::chrome::cros::reporting::proto::TriggeredRuleInfo triggered_rule;
    triggered_rule.set_rule_id(4321);
    triggered_rule.set_rule_name("report_rule_name");

    *expected_event.add_triggered_rule_info() = triggered_rule;
    expected_event.set_profile_identifier(
        browser()->profile()->GetPath().AsUTF8Unsafe());
    expected_event.set_profile_user_name(kUserName);

    event_validator.ExpectSensitiveDataEvent(std::move(expected_event));
  } else {
    event_validator.ExpectNoReport();
  }

  base::test::TestFuture<std::optional<content::ClipboardPasteData>> future;
  PasteIfAllowedByPolicy(
      /*source=*/
      content::ClipboardEndpoint(
          ui::DataTransferEndpoint(GURL(test_url_0())),
          base::BindLambdaForTesting(
              [&source_profile]() -> content::BrowserContext* {
                return source_profile;
              }),
          *contents()->GetPrimaryMainFrame()),
      /*destination=*/
      content::ClipboardEndpoint(ui::DataTransferEndpoint(GURL(test_url_1())),
                                 base::BindLambdaForTesting([this]() {
                                   return contents()->GetBrowserContext();
                                 }),
                                 *contents()->GetPrimaryMainFrame()),
      /*metadata=*/
      {.size = 1234, .format_type = ui::ClipboardFormatType::PlainTextType()},
      MakeClipboardPasteData("text", "image", {}), future.GetCallback());

  auto paste_data = future.Get();
  EXPECT_FALSE(paste_data);

  helper.WaitForDialogToInitialize();
  helper.CloseDialogWithoutBypass();
  helper.WaitForDialogToClose();

  if (machine_scope()) {
    report_run_loop.Run();
  }
}

IN_PROC_BROWSER_TEST_P(DataControlsClipboardUtilsBrowserTest,
                       PasteWarnedByDataControls_BypassedSourceRule) {
  // By making a new profile for this test, we ensure we can prevent pasting to
  // it by having the rule set in the source profile only.
  Profile* source_profile = CreateAdditionalProfile();
  data_controls::SetDataControls(source_profile->GetPrefs(), {R"({
                                   "name": "report_rule_name",
                                   "rule_id": "6543",
                                   "destinations": {
                                     "urls": ["google.com", "not.workspace.com"]
                                   },
                                   "restrictions": [
                                     {"class": "CLIPBOARD", "level": "WARN"}
                                   ]
                                 })"},
                                 machine_scope());

  data_controls::DesktopDataControlsDialogTestHelper helper(
      data_controls::DataControlsDialog::Type::kClipboardPasteWarn);

  base::RunLoop run_loop_warn;
  auto event_validator = event_report_validator_helper_->CreateValidator();

  // The event should only be reported when the policies are set at the machine
  // level as we would otherwise be reporting from a different unmanaged or
  // unaffiliated profile.
  if (machine_scope()) {
    event_validator.SetDoneClosure(run_loop_warn.QuitClosure());
    chrome::cros::reporting::proto::DlpSensitiveDataEvent expected_event;
    expected_event.set_url(test_url_1());
    expected_event.set_tab_url(test_url_1());
    expected_event.set_source("OTHER_PROFILE");
    expected_event.set_destination(test_url_1());
    expected_event.set_content_type("text/plain");
    expected_event.set_content_size(1234);
    expected_event.set_trigger(
        chrome::cros::reporting::proto::DataTransferEventTrigger::
            WEB_CONTENT_UPLOAD);
    expected_event.set_event_result(
        chrome::cros::reporting::proto::EventResult::EVENT_RESULT_WARNED);

    ::chrome::cros::reporting::proto::TriggeredRuleInfo triggered_rule;
    triggered_rule.set_rule_id(6543);
    triggered_rule.set_rule_name("report_rule_name");

    *expected_event.add_triggered_rule_info() = triggered_rule;
    expected_event.set_profile_identifier(
        browser()->profile()->GetPath().AsUTF8Unsafe());
    expected_event.set_profile_user_name(kUserName);

    event_validator.ExpectSensitiveDataEvent(std::move(expected_event));
  } else {
    event_validator.ExpectNoReport();
  }

  base::test::TestFuture<std::optional<content::ClipboardPasteData>> future;
  PasteIfAllowedByPolicy(
      /*source=*/
      content::ClipboardEndpoint(
          ui::DataTransferEndpoint(GURL(test_url_0())),
          base::BindLambdaForTesting(
              [&source_profile]() -> content::BrowserContext* {
                return source_profile;
              }),
          *contents()->GetPrimaryMainFrame()),
      /*destination=*/
      content::ClipboardEndpoint(ui::DataTransferEndpoint(GURL(test_url_1())),
                                 base::BindLambdaForTesting([this]() {
                                   return contents()->GetBrowserContext();
                                 }),
                                 *contents()->GetPrimaryMainFrame()),
      /*metadata=*/
      {.size = 1234, .format_type = ui::ClipboardFormatType::PlainTextType()},
      MakeClipboardPasteData("text", "image", {}), future.GetCallback());

  // The dialog will stay up until a user action dismisses it, so `future`
  // shouldn't be ready yet.
  EXPECT_FALSE(future.IsReady());

  base::RunLoop run_loop_bypass;
  if (machine_scope()) {
    run_loop_warn.Run();

    // The first warn event should already be reported before the dialog has
    // been initialized, so it can be reassigned so that the bypass event can be
    // validated.
    event_validator = event_report_validator_helper_->CreateValidator();
    event_validator.SetDoneClosure(run_loop_bypass.QuitClosure());
    chrome::cros::reporting::proto::DlpSensitiveDataEvent expected_event;
    expected_event.set_url(test_url_1());
    expected_event.set_tab_url(test_url_1());
    expected_event.set_source("OTHER_PROFILE");
    expected_event.set_destination(test_url_1());
    expected_event.set_content_type("text/plain");
    expected_event.set_content_size(1234);
    expected_event.set_trigger(
        chrome::cros::reporting::proto::DataTransferEventTrigger::
            WEB_CONTENT_UPLOAD);
    expected_event.set_event_result(
        chrome::cros::reporting::proto::EventResult::EVENT_RESULT_BYPASSED);

    ::chrome::cros::reporting::proto::TriggeredRuleInfo triggered_rule;
    triggered_rule.set_rule_id(6543);
    triggered_rule.set_rule_name("report_rule_name");

    *expected_event.add_triggered_rule_info() = triggered_rule;
    expected_event.set_profile_identifier(
        browser()->profile()->GetPath().AsUTF8Unsafe());
    expected_event.set_profile_user_name(kUserName);

    event_validator.ExpectSensitiveDataEvent(std::move(expected_event));
  }

  helper.BypassWarning();
  helper.WaitForDialogToClose();

  auto paste_data = future.Get();
  EXPECT_TRUE(paste_data);
  EXPECT_EQ(paste_data->text, u"text");
  EXPECT_EQ(std::string(paste_data->png.begin(), paste_data->png.end()),
            "image");

  if (machine_scope()) {
    run_loop_bypass.Run();
  }
}

IN_PROC_BROWSER_TEST_P(DataControlsClipboardUtilsBrowserTest,
                       PasteWarnedByDataControls_CanceledSourceRule) {
  // By making a new profile for this test, we ensure we can prevent pasting to
  // it by having the rule set in the source profile only.
  Profile* source_profile = CreateAdditionalProfile();
  data_controls::SetDataControls(source_profile->GetPrefs(), {R"({
                                   "name": "report_rule_name",
                                   "rule_id": "7654",
                                   "destinations": {
                                     "urls": ["google.com", "not.workspace.com"]
                                   },
                                   "restrictions": [
                                     {"class": "CLIPBOARD", "level": "WARN"}
                                   ]
                                 })"},
                                 machine_scope());

  data_controls::DesktopDataControlsDialogTestHelper helper(
      data_controls::DataControlsDialog::Type::kClipboardPasteWarn);

  base::RunLoop report_run_loop;
  auto event_validator = event_report_validator_helper_->CreateValidator();

  // The event should only be reported when the policies are set at the machine
  // level as we would otherwise be reporting from a different unmanaged or
  // unaffiliated profile.
  if (machine_scope()) {
    event_validator.SetDoneClosure(report_run_loop.QuitClosure());
    chrome::cros::reporting::proto::DlpSensitiveDataEvent expected_event;
    expected_event.set_url(test_url_1());
    expected_event.set_tab_url(test_url_1());
    expected_event.set_source("OTHER_PROFILE");
    expected_event.set_destination(test_url_1());
    expected_event.set_content_type("text/plain");
    expected_event.set_content_size(1234);
    expected_event.set_trigger(
        chrome::cros::reporting::proto::DataTransferEventTrigger::
            WEB_CONTENT_UPLOAD);
    expected_event.set_event_result(
        chrome::cros::reporting::proto::EventResult::EVENT_RESULT_WARNED);

    ::chrome::cros::reporting::proto::TriggeredRuleInfo triggered_rule;
    triggered_rule.set_rule_id(7654);
    triggered_rule.set_rule_name("report_rule_name");

    *expected_event.add_triggered_rule_info() = triggered_rule;
    expected_event.set_profile_identifier(
        browser()->profile()->GetPath().AsUTF8Unsafe());
    expected_event.set_profile_user_name(kUserName);

    event_validator.ExpectSensitiveDataEvent(std::move(expected_event));
  } else {
    event_validator.ExpectNoReport();
  }

  base::test::TestFuture<std::optional<content::ClipboardPasteData>> future;
  PasteIfAllowedByPolicy(
      /*source=*/
      content::ClipboardEndpoint(
          ui::DataTransferEndpoint(GURL(test_url_0())),
          base::BindLambdaForTesting(
              [&source_profile]() -> content::BrowserContext* {
                return source_profile;
              }),
          *contents()->GetPrimaryMainFrame()),
      /*destination=*/
      content::ClipboardEndpoint(ui::DataTransferEndpoint(GURL(test_url_1())),
                                 base::BindLambdaForTesting([this]() {
                                   return contents()->GetBrowserContext();
                                 }),
                                 *contents()->GetPrimaryMainFrame()),
      /*metadata=*/
      {.size = 1234, .format_type = ui::ClipboardFormatType::PlainTextType()},
      MakeClipboardPasteData("text", "image", {}), future.GetCallback());

  // The dialog will stay up until a user action dismisses it, so `future`
  // shouldn't be ready yet.
  EXPECT_FALSE(future.IsReady());

  helper.CloseDialogWithoutBypass();
  helper.WaitForDialogToClose();

  auto paste_data = future.Get();
  EXPECT_FALSE(paste_data);

  if (machine_scope()) {
    report_run_loop.Run();
  }
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

IN_PROC_BROWSER_TEST_P(DataControlsClipboardUtilsBrowserTest,
                       PasteReportedByDataControls_DestinationRule) {
  active_user_test_mixin_->SetFakeCookieValue();

  base::RunLoop run_loop;
  auto event_validator = event_report_validator_helper_->CreateValidator();
  event_validator.SetDoneClosure(run_loop.QuitClosure());

  chrome::cros::reporting::proto::DlpSensitiveDataEvent expected_event;
  if (use_workspace_urls()) {
    expected_event.set_web_app_signed_in_account(kContentAreaUser0);
  }
  expected_event.set_url(test_url_0());
  expected_event.set_tab_url(test_url_0());
  expected_event.set_source("CLIPBOARD");
  expected_event.set_destination(test_url_0());
  expected_event.set_content_type("image/svg+xml");
  expected_event.set_content_size(1234);
  expected_event.set_trigger(chrome::cros::reporting::proto::
                                 DataTransferEventTrigger::WEB_CONTENT_UPLOAD);
  expected_event.set_event_result(
      chrome::cros::reporting::proto::EventResult::EVENT_RESULT_ALLOWED);

  ::chrome::cros::reporting::proto::TriggeredRuleInfo triggered_rule;
  triggered_rule.set_rule_id(8765);
  triggered_rule.set_rule_name("report_rule_name");

  *expected_event.add_triggered_rule_info() = triggered_rule;
  expected_event.set_profile_identifier(
      browser()->profile()->GetPath().AsUTF8Unsafe());
  expected_event.set_profile_user_name(kUserName);

  event_validator.ExpectSensitiveDataEvent(std::move(expected_event));

  data_controls::SetDataControls(browser()->profile()->GetPrefs(), {R"({
                                   "name": "report_rule_name",
                                   "rule_id": "8765",
                                   "destinations": {
                                     "urls": ["google.com", "not.workspace.com"]
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
      /*destination=*/CreateURLClipboardEndpoint(test_url_0()),
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
  // By making a new profile for this test, we ensure we can prevent pasting to
  // it by having the rule set in the source profile only.
  Profile* source_profile = CreateAdditionalProfile();
  data_controls::SetDataControls(source_profile->GetPrefs(), {R"({
                                   "name": "report_rule_name",
                                   "rule_id": "9753",
                                   "destinations": {
                                     "urls": ["google.com", "not.workspace.com"]
                                   },
                                   "restrictions": [
                                     {"class": "CLIPBOARD", "level": "REPORT"}
                                   ]
                                 })"},
                                 machine_scope());

  data_controls::DesktopDataControlsDialogTestHelper helper(
      data_controls::DataControlsDialog::Type::kClipboardPasteBlock);

  base::RunLoop report_run_loop;
  auto event_validator = event_report_validator_helper_->CreateValidator();

  // The event should only be reported when the policies are set at the machine
  // level as we would otherwise be reporting from a different unmanaged or
  // unaffiliated profile.
  if (machine_scope()) {
    event_validator.SetDoneClosure(report_run_loop.QuitClosure());
    chrome::cros::reporting::proto::DlpSensitiveDataEvent expected_event;
    expected_event.set_url(test_url_1());
    expected_event.set_tab_url(test_url_1());
    expected_event.set_source("OTHER_PROFILE");
    expected_event.set_destination(test_url_1());
    expected_event.set_content_type("text/plain");
    expected_event.set_content_size(1234);
    expected_event.set_trigger(
        chrome::cros::reporting::proto::DataTransferEventTrigger::
            WEB_CONTENT_UPLOAD);
    expected_event.set_event_result(
        chrome::cros::reporting::proto::EventResult::EVENT_RESULT_ALLOWED);

    ::chrome::cros::reporting::proto::TriggeredRuleInfo triggered_rule;
    triggered_rule.set_rule_id(9753);
    triggered_rule.set_rule_name("report_rule_name");

    *expected_event.add_triggered_rule_info() = triggered_rule;
    expected_event.set_profile_identifier(
        browser()->profile()->GetPath().AsUTF8Unsafe());
    expected_event.set_profile_user_name(kUserName);

    event_validator.ExpectSensitiveDataEvent(std::move(expected_event));
  } else {
    event_validator.ExpectNoReport();
  }

  base::test::TestFuture<std::optional<content::ClipboardPasteData>> future;
  PasteIfAllowedByPolicy(
      /*source=*/
      content::ClipboardEndpoint(
          ui::DataTransferEndpoint(GURL(test_url_0())),
          base::BindLambdaForTesting(
              [&source_profile]() -> content::BrowserContext* {
                return source_profile;
              }),
          *contents()->GetPrimaryMainFrame()),
      /*destination=*/
      content::ClipboardEndpoint(ui::DataTransferEndpoint(GURL(test_url_1())),
                                 base::BindLambdaForTesting([this]() {
                                   return contents()->GetBrowserContext();
                                 }),
                                 *contents()->GetPrimaryMainFrame()),
      /*metadata=*/
      {.size = 1234, .format_type = ui::ClipboardFormatType::PlainTextType()},
      MakeClipboardPasteData("text", "image", {}), future.GetCallback());

  ASSERT_FALSE(helper.dialog());
  auto paste_data = future.Get();
  EXPECT_TRUE(paste_data);
  EXPECT_EQ(paste_data->text, u"text");
  EXPECT_EQ(std::string(paste_data->png.begin(), paste_data->png.end()),
            "image");

  if (machine_scope()) {
    report_run_loop.Run();
  }
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

IN_PROC_BROWSER_TEST_P(DataControlsClipboardUtilsBrowserTest, CopyAllowed) {
  auto event_validator = event_report_validator_helper_->CreateValidator();
  event_validator.ExpectNoReport();

  auto source = content::ClipboardEndpoint(
      ui::DataTransferEndpoint(GURL("https://google.com")),
      base::BindLambdaForTesting(
          [this]() { return contents()->GetBrowserContext(); }),
      *contents()->GetPrimaryMainFrame());
  ui::ClipboardMetadata metadata = {.size = 1234};
  EXPECT_FALSE(IsCopyPolicyCheckRequired(source, metadata));

  base::test::TestFuture<const ui::ClipboardFormatType&,
                         const content::ClipboardPasteData&,
                         std::optional<std::u16string>>
      future;
  IsClipboardCopyAllowedByPolicy(source, metadata,
                                 MakeClipboardPasteData("foo", "", {}),
                                 future.GetCallback());

  auto data = future.Get<content::ClipboardPasteData>();
  EXPECT_EQ(data.text, u"foo");

  auto replacement = future.Get<std::optional<std::u16string>>();
  EXPECT_FALSE(replacement);
}

IN_PROC_BROWSER_TEST_P(DataControlsClipboardUtilsBrowserTest, CopyReported) {
  active_user_test_mixin_->SetFakeCookieValue();

  base::RunLoop run_loop;
  auto event_validator = event_report_validator_helper_->CreateValidator();
  event_validator.SetDoneClosure(run_loop.QuitClosure());

  chrome::cros::reporting::proto::DlpSensitiveDataEvent expected_event;
  if (use_workspace_urls()) {
    expected_event.set_web_app_signed_in_account(kContentAreaUser1);
    expected_event.set_source_web_app_signed_in_account(kContentAreaUser1);
  }
  expected_event.set_url(test_url_1());
  expected_event.set_tab_url(test_url_1());
  expected_event.set_source(test_url_1());
  expected_event.set_destination("");
  expected_event.set_content_type("image/svg+xml");
  expected_event.set_content_size(1234);
  expected_event.set_trigger(
      chrome::cros::reporting::proto::DataTransferEventTrigger::CLIPBOARD_COPY);
  expected_event.set_event_result(
      chrome::cros::reporting::proto::EventResult::EVENT_RESULT_ALLOWED);

  ::chrome::cros::reporting::proto::TriggeredRuleInfo triggered_rule;
  triggered_rule.set_rule_id(1248);
  triggered_rule.set_rule_name("report_only");

  *expected_event.add_triggered_rule_info() = triggered_rule;
  expected_event.set_profile_identifier(
      browser()->profile()->GetPath().AsUTF8Unsafe());
  expected_event.set_profile_user_name(kUserName);

  event_validator.ExpectSensitiveDataEvent(std::move(expected_event));

  data_controls::SetDataControls(browser()->profile()->GetPrefs(), {R"({
                                   "name": "report_only",
                                   "rule_id": "1248",
                                   "sources": {
                                     "urls": ["google.com", "not.workspace.com"]
                                   },
                                   "restrictions": [
                                     {"class": "CLIPBOARD", "level": "REPORT"}
                                   ]
                                 })"},
                                 machine_scope());

  auto source = CreateURLClipboardEndpoint(test_url_1());
  ui::ClipboardMetadata metadata = {
      .size = 1234,
      .format_type = ui::ClipboardFormatType::SvgType(),
  };
  EXPECT_TRUE(IsCopyPolicyCheckRequired(source, metadata));

  base::test::TestFuture<const ui::ClipboardFormatType&,
                         const content::ClipboardPasteData&,
                         std::optional<std::u16string>>
      future;
  IsClipboardCopyAllowedByPolicy(source, metadata,
                                 MakeClipboardPasteData("foo", "", {}),
                                 future.GetCallback());

  auto data = future.Get<content::ClipboardPasteData>();
  EXPECT_EQ(data.text, u"foo");

  auto replacement = future.Get<std::optional<std::u16string>>();
  EXPECT_FALSE(replacement);
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_P(DataControlsClipboardUtilsBrowserTest, CopyBlocked) {
  active_user_test_mixin_->SetFakeCookieValue();

  base::RunLoop run_loop;
  auto event_validator = event_report_validator_helper_->CreateValidator();
  event_validator.SetDoneClosure(run_loop.QuitClosure());

  chrome::cros::reporting::proto::DlpSensitiveDataEvent expected_event;
  if (use_workspace_urls()) {
    expected_event.set_web_app_signed_in_account(kContentAreaUser0);
    expected_event.set_source_web_app_signed_in_account(kContentAreaUser0);
  }
  expected_event.set_url(test_url_0());
  expected_event.set_tab_url(test_url_0());
  expected_event.set_source(test_url_0());
  expected_event.set_destination("");
  expected_event.set_content_type("image/svg+xml");
  expected_event.set_content_size(1234);
  expected_event.set_trigger(
      chrome::cros::reporting::proto::DataTransferEventTrigger::CLIPBOARD_COPY);
  expected_event.set_event_result(
      chrome::cros::reporting::proto::EventResult::EVENT_RESULT_BLOCKED);

  ::chrome::cros::reporting::proto::TriggeredRuleInfo triggered_rule;
  triggered_rule.set_rule_id(987);
  triggered_rule.set_rule_name("block");

  *expected_event.add_triggered_rule_info() = triggered_rule;
  expected_event.set_profile_identifier(
      browser()->profile()->GetPath().AsUTF8Unsafe());
  expected_event.set_profile_user_name(kUserName);

  event_validator.ExpectSensitiveDataEvent(std::move(expected_event));

  data_controls::SetDataControls(browser()->profile()->GetPrefs(), {R"({
                                   "name": "block",
                                   "rule_id": "987",
                                   "sources": {
                                     "urls": ["google.com", "not.workspace.com"]
                                   },
                                   "restrictions": [
                                     {"class": "CLIPBOARD", "level": "BLOCK"}
                                   ]
                                 })"},
                                 machine_scope());
  data_controls::DesktopDataControlsDialogTestHelper helper(
      data_controls::DataControlsDialog::Type::kClipboardCopyBlock);

  auto source = CreateURLClipboardEndpoint(test_url_0());
  ui::ClipboardMetadata metadata = {
      .size = 1234,
      .format_type = ui::ClipboardFormatType::SvgType(),
  };
  EXPECT_TRUE(IsCopyPolicyCheckRequired(source, metadata));

  base::test::TestFuture<const ui::ClipboardFormatType&,
                         const content::ClipboardPasteData&,
                         std::optional<std::u16string>>
      future;
  IsClipboardCopyAllowedByPolicy(source, metadata,
                                 MakeClipboardPasteData("foo", "", {}),
                                 future.GetCallback());

  helper.WaitForDialogToInitialize();
  helper.CloseDialogWithoutBypass();
  helper.WaitForDialogToClose();

  EXPECT_TRUE(future.IsReady());
  auto data = future.Get<content::ClipboardPasteData>();
  EXPECT_EQ(data.text, u"");

  auto replacement = future.Get<std::optional<std::u16string>>();
  EXPECT_FALSE(replacement);
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_P(DataControlsClipboardUtilsBrowserTest,
                       CopyWarnedThenCanceled) {
  active_user_test_mixin_->SetFakeCookieValue();

  base::RunLoop run_loop;
  auto event_validator = event_report_validator_helper_->CreateValidator();
  event_validator.SetDoneClosure(run_loop.QuitClosure());
  chrome::cros::reporting::proto::DlpSensitiveDataEvent expected_event;
  if (use_workspace_urls()) {
    expected_event.set_web_app_signed_in_account(kContentAreaUser1);
    expected_event.set_source_web_app_signed_in_account(kContentAreaUser1);
  }
  expected_event.set_url(test_url_1());
  expected_event.set_tab_url(test_url_1());
  expected_event.set_source(test_url_1());
  expected_event.set_destination("");
  expected_event.set_content_type("image/png");
  expected_event.set_content_size(1234);
  expected_event.set_trigger(
      chrome::cros::reporting::proto::DataTransferEventTrigger::CLIPBOARD_COPY);
  expected_event.set_event_result(
      chrome::cros::reporting::proto::EventResult::EVENT_RESULT_WARNED);

  ::chrome::cros::reporting::proto::TriggeredRuleInfo triggered_rule;
  triggered_rule.set_rule_id(3927);
  triggered_rule.set_rule_name("warn");

  *expected_event.add_triggered_rule_info() = triggered_rule;
  expected_event.set_profile_identifier(
      browser()->profile()->GetPath().AsUTF8Unsafe());
  expected_event.set_profile_user_name(kUserName);

  event_validator.ExpectSensitiveDataEvent(std::move(expected_event));

  data_controls::SetDataControls(browser()->profile()->GetPrefs(), {R"({
                                   "name": "warn",
                                   "rule_id": "3927",
                                   "sources": {
                                     "urls": ["google.com", "not.workspace.com"]
                                   },
                                   "restrictions": [
                                     {"class": "CLIPBOARD", "level": "WARN"}
                                   ]
                                 })"},
                                 machine_scope());
  data_controls::DesktopDataControlsDialogTestHelper helper(
      data_controls::DataControlsDialog::Type::kClipboardCopyWarn);

  auto source = CreateURLClipboardEndpoint(test_url_1());
  ui::ClipboardMetadata metadata = {
      .size = 1234,
      .format_type = ui::ClipboardFormatType::PngType(),
  };
  EXPECT_TRUE(IsCopyPolicyCheckRequired(source, metadata));

  base::test::TestFuture<const ui::ClipboardFormatType&,
                         const content::ClipboardPasteData&,
                         std::optional<std::u16string>>
      future;
  IsClipboardCopyAllowedByPolicy(source, metadata,
                                 MakeClipboardPasteData("foo", "", {}),
                                 future.GetCallback());

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
  active_user_test_mixin_->SetFakeCookieValue();

  base::RunLoop run_loop;
  auto event_validator = event_report_validator_helper_->CreateValidator();
  event_validator.SetDoneClosure(run_loop.QuitClosure());

  chrome::cros::reporting::proto::DlpSensitiveDataEvent expected_event;
  if (use_workspace_urls()) {
    expected_event.set_web_app_signed_in_account(kContentAreaUser0);
    expected_event.set_source_web_app_signed_in_account(kContentAreaUser0);
  }
  expected_event.set_url(test_url_0());
  expected_event.set_tab_url(test_url_0());
  expected_event.set_source(test_url_0());
  expected_event.set_destination("");
  expected_event.set_content_type("text/plain");
  expected_event.set_content_size(1234);
  expected_event.set_trigger(
      chrome::cros::reporting::proto::DataTransferEventTrigger::CLIPBOARD_COPY);
  expected_event.set_event_result(
      chrome::cros::reporting::proto::EventResult::EVENT_RESULT_WARNED);

  ::chrome::cros::reporting::proto::TriggeredRuleInfo triggered_rule;
  triggered_rule.set_rule_id(101);
  triggered_rule.set_rule_name("warn_cancel");

  *expected_event.add_triggered_rule_info() = triggered_rule;
  expected_event.set_profile_identifier(
      browser()->profile()->GetPath().AsUTF8Unsafe());
  expected_event.set_profile_user_name(kUserName);

  event_validator.ExpectSensitiveDataEvent(std::move(expected_event));

  data_controls::SetDataControls(browser()->profile()->GetPrefs(), {R"({
                                   "name": "warn_cancel",
                                   "rule_id": "101",
                                   "sources": {
                                     "urls": ["google.com", "not.workspace.com"]
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

  auto source = CreateURLClipboardEndpoint(test_url_0());
  ui::ClipboardMetadata metadata = {
      .size = 1234,
      .format_type = ui::ClipboardFormatType::PlainTextType(),
  };
  EXPECT_TRUE(IsCopyPolicyCheckRequired(source, metadata));

  base::test::TestFuture<const ui::ClipboardFormatType&,
                         const content::ClipboardPasteData&,
                         std::optional<std::u16string>>
      future;
  IsClipboardCopyAllowedByPolicy(source, metadata,
                                 MakeClipboardPasteData("foo", "", {}),
                                 future.GetCallback());

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
  active_user_test_mixin_->SetFakeCookieValue();

  base::RunLoop run_loop_warn;
  auto event_validator = event_report_validator_helper_->CreateValidator();
  event_validator.SetDoneClosure(run_loop_warn.QuitClosure());

  {
    chrome::cros::reporting::proto::DlpSensitiveDataEvent expected_event;
    if (use_workspace_urls()) {
      expected_event.set_web_app_signed_in_account(kContentAreaUser1);
      expected_event.set_source_web_app_signed_in_account(kContentAreaUser1);
    }
    expected_event.set_url(test_url_1());
    expected_event.set_tab_url(test_url_1());
    expected_event.set_source(test_url_1());
    expected_event.set_destination("");
    expected_event.set_content_type("text/html");
    expected_event.set_content_size(1234);
    expected_event.set_trigger(chrome::cros::reporting::proto::
                                   DataTransferEventTrigger::CLIPBOARD_COPY);
    expected_event.set_event_result(
        chrome::cros::reporting::proto::EventResult::EVENT_RESULT_WARNED);

    ::chrome::cros::reporting::proto::TriggeredRuleInfo triggered_rule;
    triggered_rule.set_rule_id(12345);
    triggered_rule.set_rule_name("warn_bypass");

    *expected_event.add_triggered_rule_info() = triggered_rule;
    expected_event.set_profile_identifier(
        browser()->profile()->GetPath().AsUTF8Unsafe());
    expected_event.set_profile_user_name(kUserName);

    event_validator.ExpectSensitiveDataEvent(std::move(expected_event));
  }

  data_controls::SetDataControls(browser()->profile()->GetPrefs(), {R"({
                                   "name": "warn_bypass",
                                   "rule_id": "12345",
                                   "sources": {
                                     "urls": ["google.com", "not.workspace.com"]
                                   },
                                   "restrictions": [
                                     {"class": "CLIPBOARD", "level": "WARN"}
                                   ]
                                 })"},
                                 machine_scope());
  data_controls::DesktopDataControlsDialogTestHelper helper(
      data_controls::DataControlsDialog::Type::kClipboardCopyWarn);

  auto source = CreateURLClipboardEndpoint(test_url_1());
  ui::ClipboardMetadata metadata = {
      .size = 1234,
      .format_type = ui::ClipboardFormatType::HtmlType(),
  };
  EXPECT_TRUE(IsCopyPolicyCheckRequired(source, metadata));

  base::test::TestFuture<const ui::ClipboardFormatType&,
                         const content::ClipboardPasteData&,
                         std::optional<std::u16string>>
      future;
  IsClipboardCopyAllowedByPolicy(source, metadata,
                                 MakeClipboardPasteData("foo", "", {}),
                                 future.GetCallback());

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
  chrome::cros::reporting::proto::DlpSensitiveDataEvent expected_event_bypass;
  if (use_workspace_urls()) {
    expected_event_bypass.set_web_app_signed_in_account(kContentAreaUser1);
    expected_event_bypass.set_source_web_app_signed_in_account(
        kContentAreaUser1);
  }
  expected_event_bypass.set_url(test_url_1());
  expected_event_bypass.set_tab_url(test_url_1());
  expected_event_bypass.set_source(test_url_1());
  expected_event_bypass.set_destination("");
  expected_event_bypass.set_content_type("text/html");
  expected_event_bypass.set_content_size(1234);
  expected_event_bypass.set_trigger(
      chrome::cros::reporting::proto::DataTransferEventTrigger::CLIPBOARD_COPY);
  expected_event_bypass.set_event_result(
      chrome::cros::reporting::proto::EventResult::EVENT_RESULT_BYPASSED);

  ::chrome::cros::reporting::proto::TriggeredRuleInfo triggered_rule_bypass;
  triggered_rule_bypass.set_rule_id(12345);
  triggered_rule_bypass.set_rule_name("warn_bypass");

  *expected_event_bypass.add_triggered_rule_info() = triggered_rule_bypass;
  expected_event_bypass.set_profile_identifier(
      browser()->profile()->GetPath().AsUTF8Unsafe());
  expected_event_bypass.set_profile_user_name(kUserName);

  event_validator.ExpectSensitiveDataEvent(std::move(expected_event_bypass));

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
  active_user_test_mixin_->SetFakeCookieValue();

  base::RunLoop run_loop_warn;
  auto event_validator = event_report_validator_helper_->CreateValidator();
  event_validator.SetDoneClosure(run_loop_warn.QuitClosure());

  {
    chrome::cros::reporting::proto::DlpSensitiveDataEvent expected_event;
    if (use_workspace_urls()) {
      expected_event.set_web_app_signed_in_account(kContentAreaUser0);
      expected_event.set_source_web_app_signed_in_account(kContentAreaUser0);
    }
    expected_event.set_url(test_url_0());
    expected_event.set_tab_url(test_url_0());
    expected_event.set_source(test_url_0());
    expected_event.set_destination("");
    expected_event.set_content_type("text/html");
    expected_event.set_content_size(1234);
    expected_event.set_trigger(chrome::cros::reporting::proto::
                                   DataTransferEventTrigger::CLIPBOARD_COPY);
    expected_event.set_event_result(
        chrome::cros::reporting::proto::EventResult::EVENT_RESULT_WARNED);

    ::chrome::cros::reporting::proto::TriggeredRuleInfo triggered_rule;
    triggered_rule.set_rule_id(111);
    triggered_rule.set_rule_name("warn_bypass_os");

    *expected_event.add_triggered_rule_info() = triggered_rule;
    expected_event.set_profile_identifier(
        browser()->profile()->GetPath().AsUTF8Unsafe());
    expected_event.set_profile_user_name(kUserName);

    event_validator.ExpectSensitiveDataEvent(std::move(expected_event));
  }

  data_controls::SetDataControls(browser()->profile()->GetPrefs(), {R"({
                                   "name": "warn_bypass_os",
                                   "rule_id": "111",
                                   "sources": {
                                     "urls": ["google.com", "not.workspace.com"]
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

  auto source = CreateURLClipboardEndpoint(test_url_0());
  ui::ClipboardMetadata metadata = {
      .size = 1234,
      .format_type = ui::ClipboardFormatType::HtmlType(),
  };
  EXPECT_TRUE(IsCopyPolicyCheckRequired(source, metadata));

  base::test::TestFuture<const ui::ClipboardFormatType&,
                         const content::ClipboardPasteData&,
                         std::optional<std::u16string>>
      future;
  IsClipboardCopyAllowedByPolicy(source, metadata,
                                 MakeClipboardPasteData("foo", "", {}),
                                 future.GetCallback());

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

  {
    chrome::cros::reporting::proto::DlpSensitiveDataEvent expected_event;
    if (use_workspace_urls()) {
      expected_event.set_web_app_signed_in_account(kContentAreaUser0);
      expected_event.set_source_web_app_signed_in_account(kContentAreaUser0);
    }
    expected_event.set_url(test_url_0());
    expected_event.set_tab_url(test_url_0());
    expected_event.set_source(test_url_0());
    expected_event.set_destination("");
    expected_event.set_content_type("text/html");
    expected_event.set_content_size(1234);
    expected_event.set_trigger(chrome::cros::reporting::proto::
                                   DataTransferEventTrigger::CLIPBOARD_COPY);
    expected_event.set_event_result(
        chrome::cros::reporting::proto::EventResult::EVENT_RESULT_BYPASSED);

    ::chrome::cros::reporting::proto::TriggeredRuleInfo triggered_rule;
    triggered_rule.set_rule_id(111);
    triggered_rule.set_rule_name("warn_bypass_os");

    *expected_event.add_triggered_rule_info() = triggered_rule;
    expected_event.set_profile_identifier(
        browser()->profile()->GetPath().AsUTF8Unsafe());
    expected_event.set_profile_user_name(kUserName);

    event_validator.ExpectSensitiveDataEvent(std::move(expected_event));
  }

  helper.BypassWarning();
  helper.WaitForDialogToClose();

  auto data = future.Get<content::ClipboardPasteData>();
  EXPECT_EQ(data.text, u"foo");

  auto replacement = future.Get<std::optional<std::u16string>>();
  EXPECT_FALSE(replacement);
  run_loop_bypass.Run();
}

IN_PROC_BROWSER_TEST_P(DataControlsClipboardUtilsBrowserTest,
                       CopyBlockedOsClipboardThenPasteWarnedThenBypassed) {
  // Set up a block rule for copying to the OS clipboard and a warn rule for all
  // pastes.
  data_controls::SetDataControls(browser()->profile()->GetPrefs(),
                                 {R"({
                                   "name": "block_os_clipboard",
                                   "rule_id": "121",
                                   "sources": {
                                     "urls": ["*"]
                                   },
                                   "destinations": {
                                     "os_clipboard": true
                                   },
                                   "restrictions": [
                                     {"class": "CLIPBOARD", "level": "BLOCK"}
                                   ]
                                 })",
                                  R"({
                                   "name": "warn_on_all_pastes",
                                   "rule_id": "131",
                                   "sources": {
                                     "urls": ["*"]
                                   },
                                   "destinations": {
                                     "urls": ["*"]
                                   },
                                   "restrictions": [
                                     {"class": "CLIPBOARD", "level": "WARN"}
                                   ]
                                 })"},
                                 machine_scope());

  ui::ClipboardMetadata metadata = {
      .size = 1234,
      .format_type = ui::ClipboardFormatType::PlainTextType(),
      .seqno = ui::Clipboard::GetForCurrentThread()->GetSequenceNumber(
          ui::ClipboardBuffer::kCopyPaste),
  };

  auto source = CreateURLClipboardEndpoint("https://source.com");
  EXPECT_TRUE(IsCopyPolicyCheckRequired(source, metadata));

  base::test::TestFuture<const ui::ClipboardFormatType&,
                         const content::ClipboardPasteData&,
                         std::optional<std::u16string>>
      copy_future;
  IsClipboardCopyAllowedByPolicy(source, metadata,
                                 MakeClipboardPasteData("foo", "", {}),
                                 copy_future.GetCallback());

  ui::ClipboardMonitor::GetInstance()->NotifyClipboardDataChanged();

  // Check that replacement is populated as copying to the OS clipboard is
  // blocked.
  auto replacement = copy_future.Get<std::optional<std::u16string>>();
  EXPECT_TRUE(replacement);

  base::RunLoop run_loop_warn;
  auto event_validator = event_report_validator_helper_->CreateValidator();
  event_validator.SetDoneClosure(run_loop_warn.QuitClosure());

  chrome::cros::reporting::proto::DlpSensitiveDataEvent
      expected_warned_dlp_event;
  expected_warned_dlp_event.set_url("https://destination.com/");
  expected_warned_dlp_event.set_tab_url("https://destination.com/");
  expected_warned_dlp_event.set_source("https://source.com/");
  expected_warned_dlp_event.set_destination("https://destination.com/");
  expected_warned_dlp_event.set_content_type("text/plain");
  expected_warned_dlp_event.set_content_size(1234);
  expected_warned_dlp_event.set_trigger(
      chrome::cros::reporting::proto::DataTransferEventTrigger::
          WEB_CONTENT_UPLOAD);
  expected_warned_dlp_event.set_event_result(
      chrome::cros::reporting::proto::EventResult::EVENT_RESULT_WARNED);

  ::chrome::cros::reporting::proto::TriggeredRuleInfo triggered_warned_rule;
  triggered_warned_rule.set_rule_id(131);
  triggered_warned_rule.set_rule_name("warn_on_all_pastes");

  *expected_warned_dlp_event.add_triggered_rule_info() = triggered_warned_rule;
  expected_warned_dlp_event.set_profile_identifier(
      browser()->profile()->GetPath().AsUTF8Unsafe());
  expected_warned_dlp_event.set_profile_user_name(kUserName);

  event_validator.ExpectSensitiveDataEvent(
      std::move(expected_warned_dlp_event));

  data_controls::DesktopDataControlsDialogTestHelper helper(
      data_controls::DataControlsDialog::Type::kClipboardPasteWarn);
  base::test::TestFuture<std::optional<content::ClipboardPasteData>>
      paste_future;
  // Simulate clipboard paste data being replaced.
  PasteIfAllowedByPolicy(CreateURLClipboardEndpoint("https://source.com/"),
                         CreateURLClipboardEndpoint("https://destination.com"),
                         metadata,
                         MakeClipboardPasteData("replacement", "", {}),
                         paste_future.GetCallback());

  helper.WaitForDialogToInitialize();
  run_loop_warn.Run();

  base::RunLoop run_loop_bypass;
  event_validator = event_report_validator_helper_->CreateValidator();
  event_validator.SetDoneClosure(run_loop_bypass.QuitClosure());

  chrome::cros::reporting::proto::DlpSensitiveDataEvent
      expected_bypassed_dlp_event;
  expected_bypassed_dlp_event.set_url("https://destination.com/");
  expected_bypassed_dlp_event.set_tab_url("https://destination.com/");
  expected_bypassed_dlp_event.set_source("https://source.com/");
  expected_bypassed_dlp_event.set_destination("https://destination.com/");
  expected_bypassed_dlp_event.set_content_type("text/plain");
  expected_bypassed_dlp_event.set_content_size(1234);
  expected_bypassed_dlp_event.set_trigger(
      chrome::cros::reporting::proto::DataTransferEventTrigger::
          WEB_CONTENT_UPLOAD);
  expected_bypassed_dlp_event.set_event_result(
      chrome::cros::reporting::proto::EventResult::EVENT_RESULT_BYPASSED);

  ::chrome::cros::reporting::proto::TriggeredRuleInfo triggered_bypassed_rule;
  triggered_bypassed_rule.set_rule_id(131);
  triggered_bypassed_rule.set_rule_name("warn_on_all_pastes");

  *expected_bypassed_dlp_event.add_triggered_rule_info() =
      triggered_bypassed_rule;
  expected_bypassed_dlp_event.set_profile_identifier(
      browser()->profile()->GetPath().AsUTF8Unsafe());
  expected_bypassed_dlp_event.set_profile_user_name(kUserName);

  event_validator.ExpectSensitiveDataEvent(
      std::move(expected_bypassed_dlp_event));

  EXPECT_FALSE(paste_future.IsReady());

  helper.BypassWarning();
  helper.WaitForDialogToClose();

  // Check that the paste data is replaced back to the original data after the
  // bypass.
  auto paste_data = paste_future.Get();
  EXPECT_TRUE(paste_data);
  EXPECT_EQ(paste_data->text, u"foo");
  run_loop_bypass.Run();
}

IN_PROC_BROWSER_TEST_P(DataControlsClipboardUtilsBrowserTest,
                       FindBar_CopyAllowed) {
  auto event_validator = event_report_validator_helper_->CreateValidator();
  event_validator.ExpectNoReport();

  // Without any restriction, selected text is allowed to reach the find bar.
  EXPECT_TRUE(CanPopulateFindBarFromSelection(contents()));

  // Without any restriction, text in the find bar is allowed to be copied and
  // isn't replaced.
  const std::u16string kText = u"foo";
  std::u16string copy_replacement;
  EXPECT_FALSE(ReplaceCopyFromFindBar(kText, contents(), &copy_replacement));
  EXPECT_TRUE(copy_replacement.empty());
}

IN_PROC_BROWSER_TEST_P(DataControlsClipboardUtilsBrowserTest,
                       FindBar_CopyBlocked) {
  data_controls::SetDataControls(browser()->profile()->GetPrefs(), {R"({
                                   "name": "block",
                                   "rule_id": "987",
                                   "destinations": {
                                     "os_clipboard": true
                                   },
                                   "restrictions": [
                                     {"class": "CLIPBOARD", "level": "BLOCK"}
                                   ]
                                 })"},
                                 machine_scope());

  // With a blocking Data Controls rule, selected text is not allowed to reach
  // the find bar.
  EXPECT_FALSE(CanPopulateFindBarFromSelection(contents()));

  // With a blocking Data Controls rule, text is not allowed to be copied from
  // the find bar and is instead replaced by a warning message.
  const std::u16string kText = u"foo";
  std::u16string replacement;
  EXPECT_TRUE(ReplaceCopyFromFindBar(kText, contents(), &replacement));
  EXPECT_EQ(replacement,
            l10n_util::GetStringUTF16(
                IDS_ENTERPRISE_DATA_CONTROLS_COPY_PREVENTION_WARNING_MESSAGE));

  ui::ClipboardMonitor::GetInstance()->NotifyClipboardDataChanged();

  // Since the current rules don't restrict pasting inside the browser, the
  // original data is replaced back after pasting.
  base::test::TestFuture<std::optional<content::ClipboardPasteData>>
      paste_future;
  PasteIfAllowedByPolicy(
      CreateURLClipboardEndpoint("https://source.com/"),
      CreateURLClipboardEndpoint("https://destination.com"),
      {
          .size = 1234,
          .format_type = ui::ClipboardFormatType::PlainTextType(),
          .seqno = ui::Clipboard::GetForCurrentThread()->GetSequenceNumber(
              ui::ClipboardBuffer::kCopyPaste),
      },
      MakeClipboardPasteData("replacement", "", {}),
      paste_future.GetCallback());
  auto paste_data = paste_future.Get();
  EXPECT_TRUE(paste_data);
  EXPECT_EQ(paste_data->text, kText);
}

IN_PROC_BROWSER_TEST_P(DataControlsClipboardUtilsBrowserTest, FindBar_Paste) {
  // Without any restriction, text pasted in the find bar will be replaced if
  // necessary.
  base::test::TestFuture<std::optional<std::u16string>> replace_future;
  ReplacePasteToFindBar(contents(), replace_future.GetCallback());
  EXPECT_FALSE(replace_future.Get());

  {
    ui::ScopedClipboardWriter writer(ui::ClipboardBuffer::kCopyPaste,
                                     std::make_unique<ui::DataTransferEndpoint>(
                                         contents()->GetLastCommittedURL()));
    content::AddSourceDataToClipboardWriter(writer,
                                            *contents()->GetPrimaryMainFrame());
    writer.WriteText(u"warning");
  }
  content::ClipboardPasteData data;
  data.text = u"replaced";
  data_controls::LastReplacedClipboardDataObserver::GetInstance()
      ->AddDataToNextSeqno(data);
  ui::ClipboardMonitor::GetInstance()->NotifyClipboardDataChanged();

  base::test::TestFuture<std::optional<std::u16string>> replace_future2;
  ReplacePasteToFindBar(contents(), replace_future2.GetCallback());
  auto paste_replacement = replace_future2.Get();
  EXPECT_TRUE(paste_replacement);
  EXPECT_EQ(*paste_replacement, u"replaced");

  // With a triggered Data Controls rule, the data isn't replaced.
  data_controls::SetDataControls(browser()->profile()->GetPrefs(), {R"({
                                   "name": "block",
                                   "rule_id": "987",
                                   "destinations": {
                                     "urls": ["*"]
                                   },
                                   "restrictions": [
                                     {"class": "CLIPBOARD", "level": "BLOCK"}
                                   ]
                                 })"},
                                 machine_scope());

  base::test::TestFuture<std::optional<std::u16string>> replace_future3;
  ReplacePasteToFindBar(contents(), replace_future3.GetCallback());
  EXPECT_FALSE(replace_future3.Get());
}

IN_PROC_BROWSER_TEST_P(DataControlsClipboardUtilsBrowserTest, DragAllowed) {
  base::HistogramTester histogram_tester;
  auto event_validator = event_report_validator_helper_->CreateValidator();
  event_validator.ExpectNoReport();

  bool allowed = IsDragAllowedByPolicy(
      /*source=*/CreateURLClipboardEndpoint("https://google.com"),
      /*drop_data=*/content::DropData());

  EXPECT_TRUE(allowed);
  histogram_tester.ExpectUniqueSample(
      "Enterprise.DataControls.DragAndDrop.Verdict", 0 /* Allowed */, 1);
  histogram_tester.ExpectTotalCount(
      "Enterprise.DataControls.DragAndDrop.EvaluationLatency", 1);
}

IN_PROC_BROWSER_TEST_P(DataControlsClipboardUtilsBrowserTest, DragBlocked) {
  base::HistogramTester histogram_tester;
  active_user_test_mixin_->SetFakeCookieValue();

  base::RunLoop run_loop;
  auto event_validator = event_report_validator_helper_->CreateValidator();
  event_validator.SetDoneClosure(run_loop.QuitClosure());

  chrome::cros::reporting::proto::DlpSensitiveDataEvent expected_event;
  if (use_workspace_urls()) {
    expected_event.set_web_app_signed_in_account(kContentAreaUser0);
    expected_event.set_source_web_app_signed_in_account(kContentAreaUser0);
  }
  expected_event.set_url(test_url_0());
  expected_event.set_tab_url(test_url_0());
  expected_event.set_source(test_url_0());
  expected_event.set_destination("");
  expected_event.set_content_type("text/plain");
  expected_event.set_content_size(28);
  expected_event.set_trigger(
      chrome::cros::reporting::proto::DataTransferEventTrigger::CLIPBOARD_COPY);
  expected_event.set_event_result(
      chrome::cros::reporting::proto::EventResult::EVENT_RESULT_BLOCKED);

  ::chrome::cros::reporting::proto::TriggeredRuleInfo triggered_rule;
  triggered_rule.set_rule_id(987);
  triggered_rule.set_rule_name("block");

  *expected_event.add_triggered_rule_info() = triggered_rule;
  expected_event.set_profile_identifier(
      browser()->profile()->GetPath().AsUTF8Unsafe());
  expected_event.set_profile_user_name(kUserName);

  event_validator.ExpectSensitiveDataEvent(std::move(expected_event));

  data_controls::SetDataControls(browser()->profile()->GetPrefs(), {R"({
                                   "name": "block",
                                   "rule_id": "987",
                                   "sources": {
                                     "urls": ["google.com", "not.workspace.com"]
                                   },
                                   "restrictions": [
                                     {"class": "CLIPBOARD", "level": "BLOCK"}
                                   ]
                                 })"},
                                 machine_scope());
  data_controls::DesktopDataControlsDialogTestHelper helper(
      data_controls::DataControlsDialog::Type::kClipboardDragBlock);

  content::DropData drop_data;
  drop_data.text = u"Sensitive Data";

  bool allowed = IsDragAllowedByPolicy(
      /*source=*/CreateURLClipboardEndpoint(test_url_0()),
      /*drop_data=*/drop_data);

  EXPECT_FALSE(allowed);
  histogram_tester.ExpectUniqueSample(
      "Enterprise.DataControls.DragAndDrop.Verdict", 1 /* Blocked */, 1);
  histogram_tester.ExpectTotalCount(
      "Enterprise.DataControls.DragAndDrop.EvaluationLatency", 1);

  helper.WaitForDialogToInitialize();
  helper.CloseDialogWithoutBypass();
  helper.WaitForDialogToClose();
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_P(DataControlsClipboardUtilsBrowserTest,
                       CopyAndDragConsistentSize) {
  active_user_test_mixin_->SetFakeCookieValue();

  data_controls::SetDataControls(browser()->profile()->GetPrefs(), {R"({
                                   "name": "report_rule",
                                   "rule_id": "987",
                                   "sources": {
                                     "urls": ["google.com", "not.workspace.com"]
                                   },
                                   "restrictions": [
                                     {"class": "CLIPBOARD", "level": "REPORT"}
                                   ]
                                 })"},
                                 machine_scope());

  chrome::cros::reporting::proto::DlpSensitiveDataEvent expected_event;
  if (use_workspace_urls()) {
    expected_event.set_web_app_signed_in_account(kContentAreaUser0);
    expected_event.set_source_web_app_signed_in_account(kContentAreaUser0);
  }
  expected_event.set_url(test_url_0());
  expected_event.set_tab_url(test_url_0());
  expected_event.set_source(test_url_0());
  expected_event.set_destination("");
  expected_event.set_content_type("text/html");
  expected_event.set_content_size(26);
  expected_event.set_trigger(
      chrome::cros::reporting::proto::DataTransferEventTrigger::CLIPBOARD_COPY);
  expected_event.set_event_result(
      chrome::cros::reporting::proto::EventResult::EVENT_RESULT_ALLOWED);

  ::chrome::cros::reporting::proto::TriggeredRuleInfo triggered_rule;
  triggered_rule.set_rule_id(987);
  triggered_rule.set_rule_name("report_rule");

  *expected_event.add_triggered_rule_info() = triggered_rule;
  expected_event.set_profile_identifier(
      browser()->profile()->GetPath().AsUTF8Unsafe());
  expected_event.set_profile_user_name(kUserName);

  // Both Copy and Drag should emit exactly the same report (same size, same
  // type).
  {
    base::RunLoop run_loop;
    auto event_validator = event_report_validator_helper_->CreateValidator();
    event_validator.SetDoneClosure(run_loop.QuitClosure());
    event_validator.ExpectSensitiveDataEvent(expected_event);

    content::ClipboardPasteData data;
    data.html = u"<html></html>";

    auto source = CreateURLClipboardEndpoint(test_url_0());
    ui::ClipboardMetadata metadata = {
        .size = 26,
        .format_type = ui::ClipboardFormatType::HtmlType(),
    };
    EXPECT_TRUE(IsCopyPolicyCheckRequired(source, metadata));

    base::test::TestFuture<const ui::ClipboardFormatType&,
                           const content::ClipboardPasteData&,
                           std::optional<std::u16string>>
        copy_future;
    IsClipboardCopyAllowedByPolicy(source, metadata,
                                   /*data=*/data,
                                   /*callback=*/copy_future.GetCallback());

    EXPECT_TRUE(copy_future.IsReady());
    run_loop.Run();
  }

  {
    base::RunLoop run_loop;
    auto event_validator = event_report_validator_helper_->CreateValidator();
    event_validator.SetDoneClosure(run_loop.QuitClosure());
    event_validator.ExpectSensitiveDataEvent(std::move(expected_event));

    content::DropData drop_data;
    drop_data.html = u"<html></html>";

    EXPECT_TRUE(IsDragAllowedByPolicy(
        /*source=*/CreateURLClipboardEndpoint(test_url_0()),
        /*drop_data=*/drop_data));

    run_loop.Run();
  }
}

IN_PROC_BROWSER_TEST_P(DataControlsClipboardUtilsBrowserTest,
                       IsSearchWithAllowed_Allowed) {
  ASSERT_TRUE(content::NavigateToURL(contents(), GURL("about:blank")));
  EXPECT_TRUE(IsSearchWithAllowed(contents()));
}

IN_PROC_BROWSER_TEST_P(DataControlsClipboardUtilsBrowserTest,
                       IsSearchWithAllowed_Blocked) {
  ASSERT_TRUE(content::NavigateToURL(contents(), GURL("about:blank")));

  data_controls::SetDataControls(browser()->profile()->GetPrefs(), {R"({
                                   "name": "block_rule",
                                   "rule_id": "444",
                                   "sources": {
                                     "urls": ["*"]
                                   },
                                   "restrictions": [
                                     {"class": "CLIPBOARD", "level": "BLOCK"}
                                   ]
                                 })"},
                                 machine_scope());

  EXPECT_FALSE(IsSearchWithAllowed(contents()));
}

IN_PROC_BROWSER_TEST_P(DataControlsClipboardUtilsBrowserTest,
                       IsSearchWithAllowed_Warned) {
  ASSERT_TRUE(content::NavigateToURL(contents(), GURL("about:blank")));

  data_controls::SetDataControls(browser()->profile()->GetPrefs(), {R"({
                                   "name": "warn_rule",
                                   "rule_id": "333",
                                   "sources": {
                                     "urls": ["*"]
                                   },
                                   "restrictions": [
                                     {"class": "CLIPBOARD", "level": "WARN"}
                                   ]
                                 })"},
                                 machine_scope());

  EXPECT_TRUE(IsSearchWithAllowed(contents()));
}

IN_PROC_BROWSER_TEST_P(DataControlsClipboardUtilsBrowserTest,
                       ShouldAllowSearchWith_Allowed) {
  auto event_validator = event_report_validator_helper_->CreateValidator();
  event_validator.ExpectNoReport();

  base::test::TestFuture<void> callback_future;
  ShouldAllowSearchWith(contents(), 10, callback_future.GetCallback());
  EXPECT_TRUE(callback_future.Wait());
}

IN_PROC_BROWSER_TEST_P(DataControlsClipboardUtilsBrowserTest,
                       ShouldAllowSearchWith_WarnedBypassed) {
  ASSERT_TRUE(content::NavigateToURL(contents(), GURL("about:blank")));

  data_controls::SetDataControls(browser()->profile()->GetPrefs(), {R"({
                                   "name": "warn_rule",
                                   "rule_id": "333",
                                   "sources": {
                                     "urls": ["*"]
                                   },
                                   "restrictions": [
                                     {"class": "CLIPBOARD", "level": "WARN"}
                                   ]
                                 })"},
                                 machine_scope());

  data_controls::DesktopDataControlsDialogTestHelper helper(
      data_controls::DataControlsDialog::Type::kClipboardActionWarn);

  base::RunLoop run_loop;
  auto event_validator = event_report_validator_helper_->CreateValidator();
  event_validator.SetDoneClosure(run_loop.QuitClosure());
  chrome::cros::reporting::proto::DlpSensitiveDataEvent expected_event;
  expected_event.set_url("about:blank");
  expected_event.set_tab_url("about:blank");
  expected_event.set_source("about:blank");
  expected_event.set_destination("");
  expected_event.set_content_type("text/plain");
  expected_event.set_content_size(10);
  expected_event.set_trigger(
      chrome::cros::reporting::proto::DataTransferEventTrigger::CLIPBOARD_COPY);
  expected_event.set_event_result(
      chrome::cros::reporting::proto::EventResult::EVENT_RESULT_BYPASSED);

  ::chrome::cros::reporting::proto::TriggeredRuleInfo triggered_rule;
  triggered_rule.set_rule_id(333);
  triggered_rule.set_rule_name("warn_rule");

  *expected_event.add_triggered_rule_info() = triggered_rule;
  expected_event.set_profile_identifier(
      browser()->profile()->GetPath().AsUTF8Unsafe());
  expected_event.set_profile_user_name(kUserName);

  event_validator.ExpectSensitiveDataEvent(std::move(expected_event));

  base::test::TestFuture<void> callback_future;
  ShouldAllowSearchWith(contents(), 10, callback_future.GetCallback());

  helper.WaitForDialogToInitialize();
  EXPECT_FALSE(callback_future.IsReady());

  helper.BypassWarning();
  helper.WaitForDialogToClose();

  run_loop.Run();

  EXPECT_TRUE(callback_future.Wait());
}

IN_PROC_BROWSER_TEST_P(DataControlsClipboardUtilsBrowserTest,
                       ShouldAllowSearchWith_WarnedCanceled) {
  ASSERT_TRUE(content::NavigateToURL(contents(), GURL("about:blank")));

  data_controls::SetDataControls(browser()->profile()->GetPrefs(), {R"({
                                   "name": "warn_rule",
                                   "rule_id": "333",
                                   "sources": {
                                     "urls": ["*"]
                                   },
                                   "restrictions": [
                                     {"class": "CLIPBOARD", "level": "WARN"}
                                   ]
                                 })"},
                                 machine_scope());

  data_controls::DesktopDataControlsDialogTestHelper helper(
      data_controls::DataControlsDialog::Type::kClipboardActionWarn);

  base::RunLoop run_loop;
  auto event_validator = event_report_validator_helper_->CreateValidator();
  event_validator.SetDoneClosure(run_loop.QuitClosure());
  chrome::cros::reporting::proto::DlpSensitiveDataEvent expected_event;
  expected_event.set_url("about:blank");
  expected_event.set_tab_url("about:blank");
  expected_event.set_source("about:blank");
  expected_event.set_destination("");
  expected_event.set_content_type("text/plain");
  expected_event.set_content_size(10);
  expected_event.set_trigger(
      chrome::cros::reporting::proto::DataTransferEventTrigger::CLIPBOARD_COPY);
  expected_event.set_event_result(
      chrome::cros::reporting::proto::EventResult::EVENT_RESULT_WARNED);

  ::chrome::cros::reporting::proto::TriggeredRuleInfo triggered_rule;
  triggered_rule.set_rule_id(333);
  triggered_rule.set_rule_name("warn_rule");

  *expected_event.add_triggered_rule_info() = triggered_rule;
  expected_event.set_profile_identifier(
      browser()->profile()->GetPath().AsUTF8Unsafe());
  expected_event.set_profile_user_name(kUserName);

  event_validator.ExpectSensitiveDataEvent(std::move(expected_event));

  base::test::TestFuture<void> callback_future;
  ShouldAllowSearchWith(contents(), 10, callback_future.GetCallback());

  helper.WaitForDialogToInitialize();
  EXPECT_FALSE(callback_future.IsReady());

  helper.CloseDialogWithoutBypass();
  helper.WaitForDialogToClose();

  run_loop.Run();

  EXPECT_FALSE(callback_future.IsReady());
}

}  // namespace enterprise_data_protection
