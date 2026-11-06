// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/enterprise/connectors/reporting/realtime_reporting_client.h"
#include "chrome/browser/enterprise/connectors/reporting/realtime_reporting_client_factory.h"
#include "chrome/browser/enterprise/connectors/test/deep_scanning_test_utils.h"
#include "chrome/browser/enterprise/data_controls/desktop_data_controls_dialog.h"
#include "chrome/browser/enterprise/data_controls/desktop_data_controls_dialog_test_helper.h"
#include "chrome/browser/enterprise/data_protection/paste_allowed_request.h"
#include "chrome/browser/glic/gemini_enterprise/geic_enabling.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/host/guest_util.h"
#include "chrome/browser/glic/public/context/glic_sharing_manager.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_provider.h"
#include "chrome/browser/glic/public/glic_provider_latch.h"
#include "chrome/browser/glic/service/glic_instance_impl.h"
#include "chrome/browser/glic/test_support/glic_browser_test.h"
#include "chrome/browser/glic/test_support/scoped_page_context_eligibility.h"
#include "chrome/browser/glic/test_support/test_result.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/common/chrome_switches.h"
#include "components/enterprise/common/proto/synced/dlp_policy_event.pb.h"
#include "components/enterprise/data_controls/content/browser/last_replaced_clipboard_data.h"
#include "components/enterprise/data_controls/core/browser/test_utils.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/test/clipboard_test_util.h"
#include "ui/base/clipboard/test/test_clipboard.h"
#include "url/gurl.h"

namespace glic {
namespace {

// Mirrors `GlicPasteFailedEligibilityReason::kPageContextIneligible`, which is
// local to chrome/browser/glic/host/guest_util.cc.
constexpr int kPasteFailedPageContextIneligible = 0;

class GeicPastePolicyBrowserTest : public GlicBrowserTest {
 public:
  GeicPastePolicyBrowserTest() {
    feature_list_.InitAndEnableFeature(features::kGeic);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    GlicBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(geic::kGeicGuestURLSwitch,
                                    GetGuestURL().spec());
    command_line->AppendSwitch(::switches::kGlicDev);
  }

  void SetUpOnMainThread() override {
    ui::TestClipboard::CreateForCurrentThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    embedded_test_server()->ServeFilesFromSourceDirectory("chrome/test/data");
    GlicBrowserTest::SetUpOnMainThread();
    event_report_validator_helper_ = std::make_unique<
        enterprise_connectors::test::EventReportValidatorHelper>(
        GetProfile(), /*browser_test=*/true);
  }

  void TearDownOnMainThread() override {
    glic::ResetClipboardEligibilityMonitorForTesting();
    enterprise_data_protection::PasteAllowedRequest::
        CleanupRequestsForTesting();
    event_report_validator_helper_.reset();
    data_controls::GetLastReplacedClipboardData() = {};
    GlicBrowserTest::TearDownOnMainThread();
    ui::Clipboard::DestroyClipboardForCurrentThread();
  }

 protected:
  std::unique_ptr<enterprise_connectors::test::EventReportValidatorHelper>
      event_report_validator_helper_;

  chrome::cros::reporting::proto::DlpSensitiveDataEvent
  CreateExpectedSensitiveDataEvent(
      const GURL& source_url,
      const GURL& destination_url,
      chrome::cros::reporting::proto::EventResult result,
      int64_t rule_id,
      const std::string& rule_name,
      int64_t content_size) {
    chrome::cros::reporting::proto::DlpSensitiveDataEvent event;
    event.set_url(destination_url.spec());
    event.set_tab_url(destination_url.spec());
    event.set_source(source_url.spec());
    event.set_destination(destination_url.spec());
    event.set_content_type("text/plain");
    event.set_content_size(content_size);
    event.set_trigger(chrome::cros::reporting::proto::DataTransferEventTrigger::
                          WEB_CONTENT_UPLOAD);
    event.set_event_result(result);
    auto* triggered_rule = event.add_triggered_rule_info();
    triggered_rule->set_rule_id(rule_id);
    triggered_rule->set_rule_name(rule_name);
    event.set_profile_identifier(GetProfile()->GetPath().AsUTF8Unsafe());
    event.set_profile_user_name(GetProfile()->GetProfileUserName());
    return event;
  }

  static std::u16string ReadCurrentClipboardText() {
    return ui::clipboard_test_util::ReadText(
        ui::Clipboard::GetForCurrentThread(), ui::ClipboardBuffer::kCopyPaste,
        /*data_dst=*/nullptr);
  }

  // Returns the GEiC guest `WebContents`, or nullptr (with a recorded test
  // failure) if the GEiC client never became ready.
  content::WebContents* GetReadyGeicGuest() {
    if (TestResult<void> ready = WaitForGlicClient(); !ready.has_value()) {
      ADD_FAILURE() << "GEiC client did not become ready: " << ready.error();
      return nullptr;
    }
    EXPECT_EQ(GlicProviderLatch::GetProvider(*GetProfile()),
              GlicProvider::kGeminiEnterprise);
    EXPECT_TRUE(GlicKeyedService::IsNoWebviewEnabled(GetProfile()));

    GlicInstanceImpl* instance = GetOnlyGlicInstance();
    if (!instance) {
      ADD_FAILURE() << "No Glic instance found";
      return nullptr;
    }
    content::WebContents* geic_guest = instance->host().web_client_contents();
    EXPECT_TRUE(geic_guest != nullptr && IsGlicGuest(geic_guest));
    return geic_guest;
  }

  // Performs a real renderer DOM selection and `WebContents::Copy()` in
  // `source_contents`, verifying that the text lands on the clipboard through
  // `ClipboardHostImpl` and `ChromeContentBrowserClient`.
  static void CopyFromTabDom(content::WebContents* source_contents,
                             const std::u16string& text) {
    ASSERT_TRUE(content::ExecJs(source_contents, content::JsReplace(
                                                     R"(
              let p = document.getElementById('geic-page-text');
              if (!p) {
                p = document.createElement('p');
                p.id = 'geic-page-text';
                document.body.appendChild(p);
              }
              p.textContent = $1;

              let el = document.getElementById('geic-copy-source');
              if (!el) {
                el = document.createElement('textarea');
                el.id = 'geic-copy-source';
                document.body.appendChild(el);
              }
              el.value = $1;
              el.focus();
              el.select();
            )",
                                                     base::UTF16ToUTF8(text))));
    source_contents->Copy();
    ASSERT_TRUE(base::test::RunUntil(
        [&]() { return ReadCurrentClipboardText() == text; }));
  }

  // Installs and focuses an editable `<textarea>` inside the GEiC guest so
  // `geic_guest->Paste()` exercises the full Blink -> `ClipboardHostImpl` ->
  // `RenderFrameHostImpl::IsClipboardPasteAllowedByPolicy` pipeline.
  static void PrepareGuestPasteTarget(content::WebContents* geic_guest) {
    geic_guest->Focus();
    ASSERT_TRUE(content::ExecJs(geic_guest, R"(
      let target = document.getElementById('geic-paste-target');
      if (!target) {
        target = document.createElement('textarea');
        target.id = 'geic-paste-target';
        document.body.appendChild(target);
      }
      target.value = '';
      target.focus();
    )"));
  }

  static std::string GetGuestPasteTargetValue(
      content::WebContents* geic_guest) {
    return content::EvalJs(geic_guest,
                           "document.getElementById('geic-paste-target').value")
        .ExtractString();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  ScopedPageContextEligibilityForTesting scoped_eligibility_;
};

// Verifies that under `GlicProvider::kGeminiEnterprise` (which runs in
// `NoWebview` mode), an Enterprise `DataControlsRules` CLIPBOARD BLOCK rule
// with `"gemini_in_chrome": true`:
// 1. Intercepts a real renderer `Paste()` into the GEiC guest DOM, shows
//    `kClipboardPasteBlock`, uploads an `EVENT_RESULT_BLOCKED` audit report,
//    and prevents the DOM `<textarea>` from receiving the copied text.
// 2. Does not block `GlicSharingManager::GetContextFromTab()` (which backs
//    `getContextFromFocusedTab()`) from extracting page text from that same
//    DLP-restricted tab.
IN_PROC_BROWSER_TEST_F(
    GeicPastePolicyBrowserTest,
    BlockPasteRuleBlocksDomPasteAndReportsAuditWhileTabContextSucceeds) {
  data_controls::SetDataControls(GetProfile()->GetPrefs(), {R"({
        "name": "block_eligible_tab_to_geic",
        "rule_id": "123",
        "sources": {
          "urls": ["eligible.example.com"]
        },
        "destinations": {
          "gemini_in_chrome": true
        },
        "restrictions": [
          {"class": "CLIPBOARD", "level": "BLOCK"}
        ]
      })"});

  const GURL url =
      embedded_test_server()->GetURL("eligible.example.com", "/title1.html");
  tabs::TabInterface* active_tab = GetTabListInterface()->GetActiveTab();
  content::WebContents* source_contents = active_tab->GetContents();
  ASSERT_TRUE(content::NavigateToURL(source_contents, url));

  ASSERT_OK(OpenGlicForActiveTab());
  content::WebContents* geic_guest = GetReadyGeicGuest();
  ASSERT_TRUE(geic_guest);

  ASSERT_NO_FATAL_FAILURE(
      CopyFromTabDom(source_contents, u"confidential geic secret"));

  base::RunLoop report_loop;
  auto validator = event_report_validator_helper_->CreateValidator();
  validator.SetDoneClosure(report_loop.QuitClosure());
  validator.ExpectSensitiveDataEvent(CreateExpectedSensitiveDataEvent(
      url, geic_guest->GetLastCommittedURL(),
      chrome::cros::reporting::proto::EventResult::EVENT_RESULT_BLOCKED, 123,
      "block_eligible_tab_to_geic",
      std::u16string_view(u"confidential geic secret").size() *
          sizeof(char16_t)));

  data_controls::DesktopDataControlsDialogTestHelper block_dialog(
      data_controls::DataControlsDialog::Type::kClipboardPasteBlock);
  base::HistogramTester histogram_tester;

  ASSERT_NO_FATAL_FAILURE(PrepareGuestPasteTarget(geic_guest));
  geic_guest->Paste();

  block_dialog.WaitForDialogToInitialize();
  report_loop.Run();
  histogram_tester.ExpectTotalCount("Glic.Paste.FailedEligibilityReason.Web",
                                    0);

  block_dialog.CloseDialogWithoutBypass();
  block_dialog.WaitForDialogToClose();

  // Flush any pending renderer tasks and verify the GEiC guest DOM target never
  // received the blocked clipboard text.
  EXPECT_EQ(GetGuestPasteTargetValue(geic_guest), "");

  // Meanwhile, focused-tab context extraction (`GetContextFromTab`, which backs
  // `getContextFromFocusedTab`) on the same DLP-restricted tab is unaffected by
  // the CLIPBOARD Data Controls rule and reads the page text that was blocked
  // from being pasted.
  auto options = mojom::TabContextOptions::New();
  options->inner_text = true;
  options->inner_text_bytes_limit = 10000;
  options->annotated_page_content = true;
  base::test::TestFuture<GlicGetContextResult> context_future;
  GlicInstanceImpl* instance = GetOnlyGlicInstance();
  ASSERT_TRUE(instance);
  instance->GetSharingManagerInternal().GetContextFromTab(
      active_tab->GetHandle(), *options, context_future.GetCallback());
  GlicGetContextResult context_result = context_future.Take();
  ASSERT_TRUE(context_result.has_value()) << context_result.error().message;
  ASSERT_TRUE(context_result.value()->is_tab_context());
  const mojom::TabContextResultPtr& tab_context =
      context_result.value()->get_tab_context();
  ASSERT_TRUE(tab_context->web_page_data);
  ASSERT_TRUE(tab_context->web_page_data->main_document);
  EXPECT_THAT(tab_context->web_page_data->main_document->inner_text,
              testing::HasSubstr("confidential geic secret"));
  EXPECT_TRUE(tab_context->annotated_page_data);
}

// Verifies that a CLIPBOARD WARN rule targeting `"gemini_in_chrome": true`
// shows `kClipboardPasteWarn` over the GEiC `NoWebview` guest:
// 1. Cancelling the warning leaves the GEiC guest `<textarea>` empty and
//    reports `EVENT_RESULT_WARNED`.
// 2. Bypassing the warning populates the GEiC guest `<textarea>` with the
//    pasted text and reports `EVENT_RESULT_WARNED` + `EVENT_RESULT_BYPASSED`.
IN_PROC_BROWSER_TEST_F(GeicPastePolicyBrowserTest,
                       WarnPasteRuleCancelAndBypassOnDomPasteIntoGeicGuest) {
  data_controls::SetDataControls(GetProfile()->GetPrefs(), {R"({
        "name": "warn_eligible_tab_to_geic",
        "rule_id": "456",
        "sources": {
          "urls": ["eligible.example.com"]
        },
        "destinations": {
          "gemini_in_chrome": true
        },
        "restrictions": [
          {"class": "CLIPBOARD", "level": "WARN"}
        ]
      })"});

  const GURL url =
      embedded_test_server()->GetURL("eligible.example.com", "/title1.html");
  tabs::TabInterface* active_tab = GetTabListInterface()->GetActiveTab();
  content::WebContents* source_contents = active_tab->GetContents();
  ASSERT_TRUE(content::NavigateToURL(source_contents, url));

  ASSERT_OK(OpenGlicForActiveTab());
  content::WebContents* geic_guest = GetReadyGeicGuest();
  ASSERT_TRUE(geic_guest);

  ASSERT_NO_FATAL_FAILURE(
      CopyFromTabDom(source_contents, u"warned geic secret"));

  // 1. User cancels the warning dialog: DOM paste is blocked and only WARNED is
  // reported.
  {
    base::RunLoop warn_report_loop;
    auto validator = event_report_validator_helper_->CreateValidator();
    validator.SetDoneClosure(warn_report_loop.QuitClosure());
    validator.ExpectSensitiveDataEvent(CreateExpectedSensitiveDataEvent(
        url, geic_guest->GetLastCommittedURL(),
        chrome::cros::reporting::proto::EventResult::EVENT_RESULT_WARNED, 456,
        "warn_eligible_tab_to_geic",
        std::u16string_view(u"warned geic secret").size() * sizeof(char16_t)));

    data_controls::DesktopDataControlsDialogTestHelper warn_dialog(
        data_controls::DataControlsDialog::Type::kClipboardPasteWarn);
    ASSERT_NO_FATAL_FAILURE(PrepareGuestPasteTarget(geic_guest));
    geic_guest->Paste();

    warn_dialog.WaitForDialogToInitialize();
    warn_report_loop.Run();
    warn_dialog.CloseDialogWithoutBypass();
    warn_dialog.WaitForDialogToClose();

    EXPECT_EQ(GetGuestPasteTargetValue(geic_guest), "");
  }

  // Clear the 5s per-(rfh_id, seqno) multi-format paste dedup cache before
  // simulating a second independent user paste attempt.
  enterprise_data_protection::PasteAllowedRequest::CleanupRequestsForTesting();

  // 2. User bypasses the warning dialog: DOM `<textarea>` in the GEiC guest
  // receives the pasted text and both WARNED and BYPASSED are reported.
  {
    base::RunLoop bypass_report_loop;
    auto validator = event_report_validator_helper_->CreateValidator();
    validator.SetDoneClosure(bypass_report_loop.QuitClosure());
    validator.ExpectSensitiveDataEventWarnThenBypass(
        CreateExpectedSensitiveDataEvent(
            url, geic_guest->GetLastCommittedURL(),
            chrome::cros::reporting::proto::EventResult::EVENT_RESULT_WARNED,
            456, "warn_eligible_tab_to_geic",
            std::u16string_view(u"warned geic secret").size() *
                sizeof(char16_t)),
        CreateExpectedSensitiveDataEvent(
            url, geic_guest->GetLastCommittedURL(),
            chrome::cros::reporting::proto::EventResult::EVENT_RESULT_BYPASSED,
            456, "warn_eligible_tab_to_geic",
            std::u16string_view(u"warned geic secret").size() *
                sizeof(char16_t)));

    data_controls::DesktopDataControlsDialogTestHelper warn_dialog(
        data_controls::DataControlsDialog::Type::kClipboardPasteWarn);
    ASSERT_NO_FATAL_FAILURE(PrepareGuestPasteTarget(geic_guest));
    geic_guest->Paste();

    warn_dialog.WaitForDialogToInitialize();
    warn_dialog.BypassWarning();
    warn_dialog.WaitForDialogToClose();

    ASSERT_TRUE(base::test::RunUntil([&]() {
      return GetGuestPasteTargetValue(geic_guest) == "warned geic secret";
    }));

    bypass_report_loop.Run();
  }
}

// Verifies that copying from a page-context-ineligible tab and pasting into the
// GEiC guest is blocked by `GlicClipboardEligibilityMonitor` even when no
// Enterprise DataControlsRules are configured.
IN_PROC_BROWSER_TEST_F(GeicPastePolicyBrowserTest,
                       IneligibleTabCopyBlockedFromPastingIntoGeicGuest) {
  const GURL ineligible_url =
      embedded_test_server()->GetURL("ineligible.example.com", "/title1.html");
  tabs::TabInterface* active_tab = GetTabListInterface()->GetActiveTab();
  content::WebContents* source_contents = active_tab->GetContents();
  ASSERT_TRUE(content::NavigateToURL(source_contents, ineligible_url));

  ASSERT_OK(OpenGlicForActiveTab());
  content::WebContents* geic_guest = GetReadyGeicGuest();
  ASSERT_TRUE(geic_guest);

  ASSERT_NO_FATAL_FAILURE(
      CopyFromTabDom(source_contents, u"ineligible tab secret"));

  base::HistogramTester histogram_tester;
  ASSERT_NO_FATAL_FAILURE(PrepareGuestPasteTarget(geic_guest));
  geic_guest->Paste();

  ASSERT_TRUE(base::test::RunUntil([&]() {
    return histogram_tester.GetBucketCount(
               "Glic.Paste.FailedEligibilityReason.Web",
               kPasteFailedPageContextIneligible) >= 1;
  }));
  histogram_tester.ExpectUniqueSample("Glic.Paste.FailedEligibilityReason.Web",
                                      kPasteFailedPageContextIneligible, 1);
  EXPECT_EQ(GetGuestPasteTargetValue(geic_guest), "");
}

}  // namespace
}  // namespace glic
