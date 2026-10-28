// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "build/build_config.h"
#include "chrome/browser/enterprise/connectors/reporting/realtime_reporting_client.h"
#include "chrome/browser/enterprise/connectors/reporting/realtime_reporting_client_factory.h"
#include "chrome/browser/enterprise/connectors/test/deep_scanning_test_utils.h"
#include "chrome/browser/enterprise/data_controls/desktop_data_controls_dialog.h"
#include "chrome/browser/enterprise/data_controls/desktop_data_controls_dialog_test_helper.h"
#include "chrome/browser/enterprise/data_protection/data_protection_clipboard_utils.h"
#include "chrome/browser/enterprise/data_protection/paste_allowed_request.h"
#include "chrome/browser/glic/host/guest_util.h"
#include "chrome/browser/glic/test_support/glic_api_test.h"
#include "chrome/browser/glic/test_support/glic_browser_test.h"
#include "chrome/browser/glic/test_support/scoped_page_context_eligibility.h"
#include "chrome/browser/policy/dm_token_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/enterprise/buildflags/buildflags.h"
#include "components/enterprise/common/proto/synced/dlp_policy_event.pb.h"
#include "components/enterprise/data_controls/content/browser/last_replaced_clipboard_data.h"
#include "components/enterprise/data_controls/core/browser/test_utils.h"
#include "components/policy/core/common/cloud/dm_token.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_client.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/clipboard_types.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/common/content_client.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"
#include "ui/base/base_window.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/base/clipboard/test/clipboard_test_util.h"
#include "ui/base/clipboard/test/test_clipboard.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_delegate.h"
#include "url/gurl.h"

#if BUILDFLAG(ENTERPRISE_CLOUD_CONTENT_ANALYSIS)
#include "chrome/browser/enterprise/connectors/analysis/content_analysis_delegate.h"
#include "chrome/browser/enterprise/connectors/test/fake_clipboard_request_handler.h"
#include "chrome/browser/enterprise/connectors/test/fake_content_analysis_delegate.h"
#include "components/enterprise/connectors/core/cloud_content_scanning/clipboard_request_handler.h"
#include "components/enterprise/connectors/core/features.h"
#endif

namespace glic {
namespace {

#if BUILDFLAG(ENTERPRISE_CLOUD_CONTENT_ANALYSIS)
class PendingClipboardScanController {
 public:
  PendingClipboardScanController() = default;
  ~PendingClipboardScanController() = default;

  void HoldScan(base::OnceClosure resume_closure) {
    resume_closure_ = std::move(resume_closure);
    if (upload_started_loop_) {
      upload_started_loop_->Quit();
    }
  }

  void WaitForUploadStart() {
    if (resume_closure_) {
      return;
    }
    upload_started_loop_ = std::make_unique<base::RunLoop>();
    upload_started_loop_->Run();
    upload_started_loop_.reset();
  }

  void CompletePendingScan() {
    ASSERT_TRUE(resume_closure_);
    std::move(resume_closure_).Run();
  }

 private:
  base::OnceClosure resume_closure_;
  std::unique_ptr<base::RunLoop> upload_started_loop_;
};

class ControllableClipboardAnalysisDelegate
    : public enterprise_connectors::test::FakeContentAnalysisDelegate {
 public:
  ControllableClipboardAnalysisDelegate(
      PendingClipboardScanController* controller,
      base::RepeatingClosure delete_closure,
      StatusCallback status_callback,
      std::string dm_token,
      content::WebContents* web_contents,
      Data data,
      CompletionCallback callback,
      enterprise_connectors::DeepScanAccessPoint access_point)
      : FakeContentAnalysisDelegate(std::move(delete_closure),
                                    std::move(status_callback),
                                    std::move(dm_token),
                                    web_contents,
                                    std::move(data),
                                    std::move(callback),
                                    access_point),
        controller_(controller) {}

  static std::unique_ptr<enterprise_connectors::ContentAnalysisDelegate> Create(
      PendingClipboardScanController* controller,
      base::RepeatingClosure delete_closure,
      StatusCallback status_callback,
      std::string dm_token,
      content::WebContents* web_contents,
      Data data,
      CompletionCallback callback,
      enterprise_connectors::DeepScanAccessPoint access_point) {
    auto ret = std::make_unique<ControllableClipboardAnalysisDelegate>(
        controller, std::move(delete_closure), std::move(status_callback),
        std::move(dm_token), web_contents, std::move(data), std::move(callback),
        access_point);
    enterprise_connectors::ClipboardRequestHandler::SetFactoryForTesting(
        base::BindRepeating(
            &enterprise_connectors::test::FakeClipboardRequestHandler::Create,
            base::Unretained(ret.get())));
    return ret;
  }

  void FakeUploadClipboardDataForDeepScanning(
      enterprise_connectors::ClipboardRequestHandler::Type type,
      std::unique_ptr<enterprise_connectors::BinaryUploadRequest> request)
      override {
    if (!controller_) {
      FakeContentAnalysisDelegate::FakeUploadClipboardDataForDeepScanning(
          type, std::move(request));
      return;
    }
    controller_->HoldScan(
        base::BindOnce(&ControllableClipboardAnalysisDelegate::ResumeUpload,
                       weak_factory_.GetWeakPtr(), type, std::move(request)));
  }

 private:
  void ResumeUpload(
      enterprise_connectors::ClipboardRequestHandler::Type type,
      std::unique_ptr<enterprise_connectors::BinaryUploadRequest> request) {
    FakeContentAnalysisDelegate::FakeUploadClipboardDataForDeepScanning(
        type, std::move(request));
  }

  raw_ptr<PendingClipboardScanController> controller_;
  base::WeakPtrFactory<ControllableClipboardAnalysisDelegate> weak_factory_{
      this};
};
#endif  // BUILDFLAG(ENTERPRISE_CLOUD_CONTENT_ANALYSIS)

class GlicPastePolicyBrowserTest : public GlicBrowserTest {
 public:
  GlicPastePolicyBrowserTest() {
#if BUILDFLAG(ENTERPRISE_CLOUD_CONTENT_ANALYSIS)
    cloud_scan_feature_list_.InitAndEnableFeature(
        enterprise_connectors::kContentAnalysisClipboardCopy);
#endif
    ui::TestClipboard::CreateForCurrentThread();
  }

  ~GlicPastePolicyBrowserTest() override {
    ui::Clipboard::DestroyClipboardForCurrentThread();
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    embedded_test_server()->ServeFilesFromSourceDirectory("chrome/test/data");
    GlicBrowserTest::SetUpOnMainThread();
  }

  void TearDownOnMainThread() override {
    glic::ResetClipboardEligibilityMonitorForTesting();
    enterprise_data_protection::PasteAllowedRequest::
        CleanupRequestsForTesting();
    if (reporting_profile_) {
      if (auto* reporting_client =
              enterprise_connectors::RealtimeReportingClientFactory::
                  GetForProfile(reporting_profile_.get())) {
        reporting_client->SetBrowserCloudPolicyClientForTesting(nullptr);
      }
    }
    cloud_policy_client_.reset();
    policy::SetDMTokenForTesting(policy::DMToken::CreateEmptyToken());
    data_controls::GetLastReplacedClipboardData() = {};
#if BUILDFLAG(ENTERPRISE_CLOUD_CONTENT_ANALYSIS)
    enterprise_connectors::ClipboardRequestHandler::ResetFactoryForTesting();
    enterprise_connectors::ContentAnalysisDelegate::ResetFactoryForTesting();
    enterprise_connectors::test::FakeContentAnalysisDelegate::
        ResetStaticDialogFlagsAndTotalRequestsCount();
#endif
    GlicBrowserTest::TearDownOnMainThread();
  }

 protected:
  policy::MockCloudPolicyClient* SetupRealtimeReporting() {
    reporting_profile_ = GetProfile()->GetWeakPtr();
    enterprise_connectors::RealtimeReportingClientFactory::GetInstance()
        ->SetTestingFactory(
            GetProfile(),
            base::BindRepeating(
                &enterprise_connectors::test::BuildRealtimeReportingClient));
    policy::SetDMTokenForTesting(
        policy::DMToken::CreateValidToken("fake-dm-token"));
    enterprise_connectors::test::SetOnSecurityEventReporting(
        GetProfile()->GetPrefs(), /*enabled=*/true);
    cloud_policy_client_ = std::make_unique<policy::MockCloudPolicyClient>();
    cloud_policy_client_->SetDMToken("fake-dm-token");
    enterprise_connectors::RealtimeReportingClientFactory::GetForProfile(
        GetProfile())
        ->SetBrowserCloudPolicyClientForTesting(cloud_policy_client_.get());
    return cloud_policy_client_.get();
  }

  static content::ClipboardEndpoint MakeGuestDestination(
      content::WebContents* glic_guest) {
    return content::ClipboardEndpoint::ForFrame(
        ui::DataTransferEndpoint(glic_guest->GetLastCommittedURL()),
        base::BindLambdaForTesting(
            [glic_guest] { return glic_guest->GetBrowserContext(); }),
        *glic_guest->GetPrimaryMainFrame());
  }

  static std::u16string ReadCurrentClipboardText() {
    return ui::clipboard_test_util::ReadText(
        ui::Clipboard::GetForCurrentThread(), ui::ClipboardBuffer::kCopyPaste,
        /*data_dst=*/nullptr);
  }

  static void CopyTextViaContentBrowserClient(content::RenderFrameHost* rfh,
                                              const std::u16string& text) {
    ASSERT_NE(rfh, nullptr);
    content::ClipboardEndpoint endpoint =
        content::CreateClipboardEndpoint(*rfh);
    auto dte = endpoint.data_transfer_endpoint()
                   ? std::make_unique<ui::DataTransferEndpoint>(
                         *endpoint.data_transfer_endpoint())
                   : nullptr;

    content::ClipboardPasteData data;
    data.text = text;
    ui::ClipboardMetadata metadata = {
        .size = data.text.size() * sizeof(char16_t),
        .format_type = ui::ClipboardFormatType::PlainTextType(),
    };

    content::GetContentClientForTesting()
        ->browser()
        ->IsClipboardCopyAllowedByPolicy(
            endpoint, metadata, data,
            base::BindOnce(
                [](std::unique_ptr<ui::DataTransferEndpoint> dte,
                   const ui::ClipboardFormatType&,
                   const content::ClipboardPasteData& allowed_data,
                   std::optional<std::u16string> replacement_data) {
                  if (!allowed_data.empty() || replacement_data.has_value()) {
                    ui::ScopedClipboardWriter scw(
                        ui::ClipboardBuffer::kCopyPaste, std::move(dte));
                    scw.WriteText(replacement_data ? *replacement_data
                                                   : allowed_data.text);
                  }
                },
                std::move(dte)));
  }

  content::WebContents* GetReadyGuest() {
    EXPECT_OK(WaitForGlicClient());

    content::WebContents* glic_guest =
        GetOnlyGlicInstance()->host().web_client_contents();
    EXPECT_TRUE(glic_guest != nullptr && IsGlicGuest(glic_guest));
    return glic_guest;
  }

 private:
#if BUILDFLAG(ENTERPRISE_CLOUD_CONTENT_ANALYSIS)
  base::test::ScopedFeatureList cloud_scan_feature_list_;
#endif
  base::WeakPtr<Profile> reporting_profile_;
  std::unique_ptr<policy::MockCloudPolicyClient> cloud_policy_client_;
  ScopedPageContextEligibilityForTesting scoped_eligibility_;
};

IN_PROC_BROWSER_TEST_F(
    GlicPastePolicyBrowserTest,
    PolicyOnlyCopyCheckAndAbortedCopyClearPendingEligibility) {
  const GURL eligible_url =
      embedded_test_server()->GetURL("eligible.example.com", "/title1.html");
  const GURL ineligible_url =
      embedded_test_server()->GetURL("ineligible.example.com", "/title2.html");

  tabs::TabInterface* active_tab = GetTabListInterface()->GetActiveTab();
  content::WebContents* source_contents = active_tab->GetContents();
  ASSERT_TRUE(content::NavigateToURL(source_contents, eligible_url));

  ASSERT_OK(OpenGlicForActiveTab());
  content::WebContents* glic_guest = GetReadyGuest();
  ASSERT_TRUE(glic_guest);
  content::ClipboardEndpoint destination = MakeGuestDestination(glic_guest);

  // Establish an eligible clipboard copy first.
  CopyTextViaContentBrowserClient(source_contents->GetPrimaryMainFrame(),
                                  u"eligible baseline");
  ASSERT_EQ(ReadCurrentClipboardText(), u"eligible baseline");
  const ui::ClipboardSequenceNumberToken baseline_seqno =
      ui::Clipboard::GetForCurrentThread()->GetSequenceNumber(
          ui::ClipboardBuffer::kCopyPaste);
  EXPECT_FALSE(glic::HasPendingClipboardCopyEligibilityForTesting());

  // 1. Direct policy-only check (`enterprise_data_protection::
  // IsClipboardCopyAllowedByPolicy`) does not touch Glic copy eligibility and
  // leaves `baseline_seqno` intact.
  ASSERT_TRUE(content::NavigateToURL(source_contents, ineligible_url));
  bool callback_ran = false;
  content::ClipboardPasteData check_data;
  check_data.text = u"policy check only";
  enterprise_data_protection::IsClipboardCopyAllowedByPolicy(
      content::CreateClipboardEndpoint(*source_contents->GetPrimaryMainFrame()),
      {
          .size = check_data.text.size() * sizeof(char16_t),
          .format_type = ui::ClipboardFormatType::PlainTextType(),
      },
      check_data,
      base::BindLambdaForTesting(
          [&](const ui::ClipboardFormatType&,
              const content::ClipboardPasteData&,
              std::optional<std::u16string>) { callback_ran = true; }));
  ASSERT_TRUE(callback_ran);
  EXPECT_FALSE(glic::HasPendingClipboardCopyEligibilityForTesting());

  // The earlier `baseline_seqno` copy remains eligible when pasted into Glic.
  {
    content::ClipboardEndpoint baseline_source =
        content::CreateClipboardEndpoint(
            *source_contents->GetPrimaryMainFrame());
    ASSERT_NE(baseline_source.web_contents(), nullptr);
    ui::ClipboardMetadata metadata = {
        .size = 17 * sizeof(char16_t),
        .format_type = ui::ClipboardFormatType::PlainTextType(),
        .seqno = baseline_seqno,
    };
    content::ClipboardPasteData paste_data;
    paste_data.text = u"eligible baseline";
    base::test::TestFuture<std::optional<content::ClipboardPasteData>> future;
    content::GetContentClientForTesting()
        ->browser()
        ->IsClipboardPasteAllowedByPolicy(baseline_source, destination,
                                          metadata, std::move(paste_data),
                                          future.GetCallback());
    ASSERT_TRUE(future.Get().has_value());
  }

  // 2. Aborted copy via `ChromeContentBrowserClient` where the source subframe
  // is removed before `kClipboardCopyWarn` is cancelled must leave zero pending
  // eligibility armed (bypassing after subframe removal is tracked separately
  // in crbug.com/578631802).
  data_controls::SetDataControls(GetProfile()->GetPrefs(), {R"({
        "sources": {
          "urls": ["*"]
        },
        "destinations": {
          "os_clipboard": true
        },
        "restrictions": [
          {"class": "CLIPBOARD", "level": "WARN"}
        ]
      })"});
  ASSERT_TRUE(content::NavigateToURL(source_contents, eligible_url));
  ASSERT_TRUE(content::ExecJs(
      source_contents,
      "const f = document.createElement('iframe'); f.id = 'sub'; "
      "f.src = '/title2.html'; document.body.appendChild(f);"));
  ASSERT_TRUE(content::WaitForLoadStop(source_contents));
  content::RenderFrameHost* child_rfh =
      content::ChildFrameAt(source_contents->GetPrimaryMainFrame(), 0);
  ASSERT_NE(child_rfh, nullptr);

  {
    data_controls::DesktopDataControlsDialogTestHelper copy_warn_dialog(
        data_controls::DataControlsDialog::Type::kClipboardCopyWarn);
    CopyTextViaContentBrowserClient(child_rfh, u"detached subframe copy");
    copy_warn_dialog.WaitForDialogToInitialize();
    EXPECT_FALSE(glic::HasPendingClipboardCopyEligibilityForTesting());

    content::RenderFrameDeletedObserver child_deleted(child_rfh);
    ASSERT_TRUE(content::ExecJs(source_contents,
                                "document.getElementById('sub').remove();"));
    child_deleted.WaitUntilDeleted();

    copy_warn_dialog.CloseDialogWithoutBypass();
    copy_warn_dialog.WaitForDialogToClose();
    EXPECT_FALSE(glic::HasPendingClipboardCopyEligibilityForTesting());
  }

  // 3. Cancelled inner callback and end-of-task staging must not leak
  // `pending_eligibility_ = true` onto a subsequent unrelated clipboard write.
  data_controls::SetDataControls(GetProfile()->GetPrefs(), {});
  ASSERT_TRUE(content::NavigateToURL(source_contents, eligible_url));
  {
    struct CancelledReceiver {
      void OnAllowed(const ui::ClipboardFormatType&,
                     const content::ClipboardPasteData&,
                     std::optional<std::u16string>) {}
      base::WeakPtrFactory<CancelledReceiver> weak_factory{this};
    } receiver;
    auto cancelled_callback = base::BindOnce(
        &CancelledReceiver::OnAllowed, receiver.weak_factory.GetWeakPtr());
    receiver.weak_factory.InvalidateWeakPtrs();

    content::GetContentClientForTesting()
        ->browser()
        ->IsClipboardCopyAllowedByPolicy(
            content::CreateClipboardEndpoint(
                *source_contents->GetPrimaryMainFrame()),
            {
                .size = check_data.text.size() * sizeof(char16_t),
                .format_type = ui::ClipboardFormatType::PlainTextType(),
            },
            check_data, std::move(cancelled_callback));
    EXPECT_FALSE(glic::HasPendingClipboardCopyEligibilityForTesting());
  }

  glic::StageClipboardCopyEligibility(true, /*clear_at_end_of_task=*/true);
  EXPECT_TRUE(glic::HasPendingClipboardCopyEligibilityForTesting());
  EXPECT_TRUE(base::test::RunUntil(
      []() { return !glic::HasPendingClipboardCopyEligibilityForTesting(); }));

  {
    ui::ScopedClipboardWriter writer(ui::ClipboardBuffer::kCopyPaste);
    writer.WriteText(u"unrelated write");
  }
  const ui::ClipboardSequenceNumberToken unrelated_seqno =
      ui::Clipboard::GetForCurrentThread()->GetSequenceNumber(
          ui::ClipboardBuffer::kCopyPaste);
  ASSERT_TRUE(content::NavigateToURL(source_contents, ineligible_url));
  {
    content::ClipboardEndpoint ineligible_source =
        content::CreateClipboardEndpoint(
            *source_contents->GetPrimaryMainFrame());
    ui::ClipboardMetadata metadata = {
        .size = 15 * sizeof(char16_t),
        .format_type = ui::ClipboardFormatType::PlainTextType(),
        .seqno = unrelated_seqno,
    };
    content::ClipboardPasteData paste_data;
    paste_data.text = u"unrelated write";
    base::test::TestFuture<std::optional<content::ClipboardPasteData>> future;
    content::GetContentClientForTesting()
        ->browser()
        ->IsClipboardPasteAllowedByPolicy(ineligible_source, destination,
                                          metadata, std::move(paste_data),
                                          future.GetCallback());
    EXPECT_EQ(future.Get(), std::nullopt);
  }
}

IN_PROC_BROWSER_TEST_F(
    GlicPastePolicyBrowserTest,
    EligibleTabPasteWithDataControlsBlockShowsModalAndReportsAudit) {
  policy::MockCloudPolicyClient* cloud_policy_client = SetupRealtimeReporting();

  data_controls::SetDataControls(GetProfile()->GetPrefs(), {R"({
        "name": "block_eligible_tab_to_glic",
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
  ASSERT_TRUE(content::NavigateToURL(active_tab->GetContents(), url));

  ASSERT_OK(OpenGlicForActiveTab());
  content::WebContents* source_contents = active_tab->GetContents();
  content::WebContents* glic_guest = GetReadyGuest();
  ASSERT_TRUE(glic_guest);

  CopyTextViaContentBrowserClient(source_contents->GetPrimaryMainFrame(),
                                  u"secret");
  ASSERT_EQ(ReadCurrentClipboardText(), u"secret");

  content::ClipboardEndpoint source =
      content::CreateClipboardEndpoint(*source_contents->GetPrimaryMainFrame());
  ui::ClipboardMetadata metadata = {
      .size = 6 * sizeof(char16_t),
      .format_type = ui::ClipboardFormatType::PlainTextType(),
      .seqno = ui::Clipboard::GetForCurrentThread()->GetSequenceNumber(
          ui::ClipboardBuffer::kCopyPaste),
  };
  content::ClipboardEndpoint destination = MakeGuestDestination(glic_guest);

  bool report_uploaded = false;
  base::RunLoop report_loop;
  EXPECT_CALL(*cloud_policy_client, UploadSecurityEvent)
      .WillOnce(
          [&](bool include_device_info,
              ::chrome::cros::reporting::proto::UploadEventsRequest request,
              policy::CloudPolicyClient::ResultCallback callback) {
            absl::Cleanup finish = [&] {
              std::move(callback).Run(
                  policy::CloudPolicyClient::Result(policy::DM_STATUS_SUCCESS));
              report_loop.Quit();
            };
            EXPECT_EQ(request.events_size(), 1);
            if (request.events_size() != 1) {
              return;
            }
            EXPECT_TRUE(request.events(0).has_sensitive_data_event());
            if (!request.events(0).has_sensitive_data_event()) {
              return;
            }
            const auto& event = request.events(0).sensitive_data_event();
            EXPECT_EQ(event.source(), url.spec());
            EXPECT_EQ(event.destination(),
                      glic_guest->GetLastCommittedURL().spec());
            EXPECT_EQ(event.event_result(),
                      chrome::cros::reporting::proto::EVENT_RESULT_BLOCKED);
            EXPECT_EQ(event.triggered_rule_info_size(), 1);
            if (event.triggered_rule_info_size() == 1) {
              EXPECT_EQ(event.triggered_rule_info(0).rule_id(), 123);
              EXPECT_EQ(event.triggered_rule_info(0).rule_name(),
                        "block_eligible_tab_to_glic");
            }
            report_uploaded = true;
          });

  data_controls::DesktopDataControlsDialogTestHelper dialog_helper(
      data_controls::DataControlsDialog::Type::kClipboardPasteBlock);
  base::HistogramTester histogram_tester;

  content::ClipboardPasteData paste_data;
  paste_data.text = u"secret";
  base::test::TestFuture<std::optional<content::ClipboardPasteData>> future;
  content::GetContentClientForTesting()
      ->browser()
      ->IsClipboardPasteAllowedByPolicy(source, destination, metadata,
                                        std::move(paste_data),
                                        future.GetCallback());

  dialog_helper.WaitForDialogToInitialize();
  EXPECT_EQ(future.Get(), std::nullopt);
  report_loop.Run();
  EXPECT_TRUE(report_uploaded);
  histogram_tester.ExpectTotalCount("Glic.Paste.FailedEligibilityReason.Web",
                                    0);

  dialog_helper.CloseDialogWithoutBypass();
  dialog_helper.WaitForDialogToClose();
}

IN_PROC_BROWSER_TEST_F(GlicPastePolicyBrowserTest,
                       WarnPasteRuleBypassAndCancelOnEligibleCopy) {
  policy::MockCloudPolicyClient* cloud_policy_client = SetupRealtimeReporting();

  data_controls::SetDataControls(GetProfile()->GetPrefs(), {R"({
        "name": "warn_eligible_tab_to_glic",
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
  ASSERT_TRUE(content::NavigateToURL(active_tab->GetContents(), url));

  ASSERT_OK(OpenGlicForActiveTab());
  content::WebContents* source_contents = active_tab->GetContents();
  content::WebContents* glic_guest = GetReadyGuest();
  ASSERT_TRUE(glic_guest);

  CopyTextViaContentBrowserClient(source_contents->GetPrimaryMainFrame(),
                                  u"warned secret");
  ASSERT_EQ(ReadCurrentClipboardText(), u"warned secret");

  content::ClipboardEndpoint source =
      content::CreateClipboardEndpoint(*source_contents->GetPrimaryMainFrame());
  ui::ClipboardMetadata metadata = {
      .size = 13 * sizeof(char16_t),
      .format_type = ui::ClipboardFormatType::PlainTextType(),
      .seqno = ui::Clipboard::GetForCurrentThread()->GetSequenceNumber(
          ui::ClipboardBuffer::kCopyPaste),
  };
  content::ClipboardEndpoint destination = MakeGuestDestination(glic_guest);

  // 1. User cancels the warning dialog: paste is blocked and only WARNED is
  // reported.
  {
    base::RunLoop warn_report_loop;
    EXPECT_CALL(*cloud_policy_client, UploadSecurityEvent)
        .WillOnce(
            [&](bool include_device_info,
                ::chrome::cros::reporting::proto::UploadEventsRequest request,
                policy::CloudPolicyClient::ResultCallback callback) {
              absl::Cleanup finish = [&] {
                std::move(callback).Run(policy::CloudPolicyClient::Result(
                    policy::DM_STATUS_SUCCESS));
                warn_report_loop.Quit();
              };
              EXPECT_EQ(request.events_size(), 1);
              if (request.events_size() == 1 &&
                  request.events(0).has_sensitive_data_event()) {
                const auto& event = request.events(0).sensitive_data_event();
                EXPECT_EQ(event.source(), url.spec());
                EXPECT_EQ(event.destination(),
                          glic_guest->GetLastCommittedURL().spec());
                EXPECT_EQ(event.event_result(),
                          chrome::cros::reporting::proto::EVENT_RESULT_WARNED);
              }
            });

    data_controls::DesktopDataControlsDialogTestHelper warn_dialog(
        data_controls::DataControlsDialog::Type::kClipboardPasteWarn);
    content::ClipboardPasteData paste_data;
    paste_data.text = u"warned secret";
    base::test::TestFuture<std::optional<content::ClipboardPasteData>> future;
    content::GetContentClientForTesting()
        ->browser()
        ->IsClipboardPasteAllowedByPolicy(source, destination, metadata,
                                          std::move(paste_data),
                                          future.GetCallback());

    warn_dialog.WaitForDialogToInitialize();
    warn_report_loop.Run();
    warn_dialog.CloseDialogWithoutBypass();
    warn_dialog.WaitForDialogToClose();
    EXPECT_EQ(future.Get(), std::nullopt);
    testing::Mock::VerifyAndClearExpectations(cloud_policy_client);
  }

  // Clear the 5s per-(rfh_id, seqno) multi-format paste dedup cache before
  // simulating a second independent user paste attempt.
  enterprise_data_protection::PasteAllowedRequest::CleanupRequestsForTesting();

  // 2. User bypasses the warning dialog: paste succeeds and both WARNED and
  // BYPASSED are reported with the Glic guest destination.
  {
    std::vector<chrome::cros::reporting::proto::EventResult> results;
    base::RunLoop bypass_report_loop;
    EXPECT_CALL(*cloud_policy_client, UploadSecurityEvent)
        .Times(2)
        .WillRepeatedly(
            [&](bool include_device_info,
                ::chrome::cros::reporting::proto::UploadEventsRequest request,
                policy::CloudPolicyClient::ResultCallback callback) {
              absl::Cleanup finish = [&] {
                std::move(callback).Run(policy::CloudPolicyClient::Result(
                    policy::DM_STATUS_SUCCESS));
                if (results.size() == 2) {
                  bypass_report_loop.Quit();
                }
              };
              EXPECT_EQ(request.events_size(), 1);
              if (request.events_size() == 1 &&
                  request.events(0).has_sensitive_data_event()) {
                const auto& event = request.events(0).sensitive_data_event();
                EXPECT_EQ(event.source(), url.spec());
                EXPECT_EQ(event.destination(),
                          glic_guest->GetLastCommittedURL().spec());
                results.push_back(event.event_result());
              }
            });

    data_controls::DesktopDataControlsDialogTestHelper warn_dialog(
        data_controls::DataControlsDialog::Type::kClipboardPasteWarn);
    content::ClipboardPasteData paste_data;
    paste_data.text = u"warned secret";
    base::test::TestFuture<std::optional<content::ClipboardPasteData>> future;
    content::GetContentClientForTesting()
        ->browser()
        ->IsClipboardPasteAllowedByPolicy(source, destination, metadata,
                                          std::move(paste_data),
                                          future.GetCallback());

    warn_dialog.WaitForDialogToInitialize();
    warn_dialog.BypassWarning();
    warn_dialog.WaitForDialogToClose();

    std::optional<content::ClipboardPasteData> allowed = future.Get();
    ASSERT_TRUE(allowed.has_value());
    EXPECT_EQ(allowed->text, u"warned secret");

    bypass_report_loop.Run();
    EXPECT_EQ(results,
              (std::vector<chrome::cros::reporting::proto::EventResult>{
                  chrome::cros::reporting::proto::EVENT_RESULT_WARNED,
                  chrome::cros::reporting::proto::EVENT_RESULT_BYPASSED,
              }));
  }
}

IN_PROC_BROWSER_TEST_F(
    GlicPastePolicyBrowserTest,
    KeptInManagedChromeRestoresOpenTabCopyAndBlocksAfterSourceTabCloses) {
  data_controls::SetDataControls(GetProfile()->GetPrefs(), {R"({
        "sources": {
          "urls": ["eligible.example.com"]
        },
        "destinations": {
          "os_clipboard": true
        },
        "restrictions": [
          {"class": "CLIPBOARD", "level": "BLOCK"}
        ]
      })"});

  tabs::TabInterface* glic_tab = GetTabListInterface()->GetActiveTab();
  ASSERT_TRUE(glic_tab);

  const GURL eligible_url =
      embedded_test_server()->GetURL("eligible.example.com", "/title1.html");
  tabs::TabInterface* source_tab = CreateAndActivateTab(eligible_url);
  ASSERT_TRUE(source_tab);
  content::WebContents* source_contents = source_tab->GetContents();

  // Real copy writes OS replacement text while caching original text under
  // CopyRestrictionLevel::kKeptInManagedChrome.
  CopyTextViaContentBrowserClient(source_contents->GetPrimaryMainFrame(),
                                  u"corp internal secret");
  const std::u16string os_clipboard_text = ReadCurrentClipboardText();
  EXPECT_NE(os_clipboard_text, u"corp internal secret");
  EXPECT_FALSE(os_clipboard_text.empty());
  const ui::ClipboardSequenceNumberToken copy_seqno =
      ui::Clipboard::GetForCurrentThread()->GetSequenceNumber(
          ui::ClipboardBuffer::kCopyPaste);
  ASSERT_EQ(data_controls::GetLastReplacedClipboardData().seqno, copy_seqno);
  ASSERT_EQ(data_controls::GetLastReplacedClipboardData().restriction_level,
            data_controls::CopyRestrictionLevel::kKeptInManagedChrome);

  // Switch back to `glic_tab` and open Glic while `source_tab` remains open in
  // the background, so closing `source_tab` later does not background
  // `glic_tab`'s side panel or detach its modal dialog manager.
  ActivateTab(glic_tab);
  ASSERT_OK(OpenGlicForActiveTab());
  content::WebContents* glic_guest = GetReadyGuest();
  ASSERT_TRUE(glic_guest);

  content::ClipboardEndpoint source_endpoint =
      content::CreateClipboardEndpoint(*source_contents->GetPrimaryMainFrame());
  content::ClipboardEndpoint destination = MakeGuestDestination(glic_guest);
  ui::ClipboardMetadata metadata = {
      .size = os_clipboard_text.size() * sizeof(char16_t),
      .format_type = ui::ClipboardFormatType::PlainTextType(),
      .seqno = copy_seqno,
  };

  // Same-profile open-tab paste into Glic restores the original copied text.
  {
    content::ClipboardPasteData paste_data;
    paste_data.text = os_clipboard_text;
    base::test::TestFuture<std::optional<content::ClipboardPasteData>> future;
    content::GetContentClientForTesting()
        ->browser()
        ->IsClipboardPasteAllowedByPolicy(source_endpoint, destination,
                                          metadata, std::move(paste_data),
                                          future.GetCallback());
    std::optional<content::ClipboardPasteData> restored = future.Get();
    ASSERT_TRUE(restored.has_value());
    EXPECT_EQ(restored->text, u"corp internal secret");
  }

  // Close the source tab so `source_endpoint.web_contents()` and
  // `source_endpoint.browser_context()` both become null.
  content::WebContentsDestroyedWatcher destroyed_watcher(source_contents);
  GetTabListInterface()->CloseTab(source_tab->GetHandle());
  destroyed_watcher.Wait();
  ASSERT_EQ(source_endpoint.web_contents(), nullptr);
  ASSERT_EQ(source_endpoint.browser_context(), nullptr);

  // Closed-source-tab paste with CopyRestrictionLevel::kKeptInManagedChrome
  // blocks in ChromeContentBrowserClient::IsClipboardPasteAllowedByPolicy and
  // shows kClipboardPasteBlock instead of leaking the replacement warning
  // string into Glic.
  enterprise_data_protection::PasteAllowedRequest::CleanupRequestsForTesting();
  {
    data_controls::DesktopDataControlsDialogTestHelper block_dialog(
        data_controls::DataControlsDialog::Type::kClipboardPasteBlock);
    content::ClipboardPasteData paste_data;
    paste_data.text = os_clipboard_text;
    base::test::TestFuture<std::optional<content::ClipboardPasteData>> future;
    content::GetContentClientForTesting()
        ->browser()
        ->IsClipboardPasteAllowedByPolicy(source_endpoint, destination,
                                          metadata, std::move(paste_data),
                                          future.GetCallback());

    block_dialog.WaitForDialogToInitialize();
    EXPECT_EQ(future.Get(), std::nullopt);
    block_dialog.CloseDialogWithoutBypass();
    block_dialog.WaitForDialogToClose();
  }

  // Closed-source-tab paste with CopyRestrictionLevel::kBlocked also blocks and
  // shows kClipboardPasteBlock.
  enterprise_data_protection::PasteAllowedRequest::CleanupRequestsForTesting();
  data_controls::GetLastReplacedClipboardData().restriction_level =
      data_controls::CopyRestrictionLevel::kBlocked;
  {
    data_controls::DesktopDataControlsDialogTestHelper block_dialog(
        data_controls::DataControlsDialog::Type::kClipboardPasteBlock);
    content::ClipboardPasteData paste_data;
    paste_data.text = os_clipboard_text;
    base::test::TestFuture<std::optional<content::ClipboardPasteData>> future;
    content::GetContentClientForTesting()
        ->browser()
        ->IsClipboardPasteAllowedByPolicy(source_endpoint, destination,
                                          metadata, std::move(paste_data),
                                          future.GetCallback());

    block_dialog.WaitForDialogToInitialize();
    EXPECT_EQ(future.Get(), std::nullopt);
    block_dialog.CloseDialogWithoutBypass();
    block_dialog.WaitForDialogToClose();
  }
}

IN_PROC_BROWSER_TEST_F(
    GlicPastePolicyBrowserTest,
    DetachedFloatyPasteBlockModalAnchorsToFloatyAndClosesWithGlic) {
  data_controls::SetDataControls(GetProfile()->GetPrefs(), {R"({
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

  const GURL eligible_url =
      embedded_test_server()->GetURL("eligible.example.com", "/title1.html");
  tabs::TabInterface* active_tab = GetTabListInterface()->GetActiveTab();
  ASSERT_TRUE(content::NavigateToURL(active_tab->GetContents(), eligible_url));

  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * instance,
                       OpenGlicForActiveTabAndDetach());
  ASSERT_TRUE(instance->IsDetached());
  content::WebContents* glic_guest = GetReadyGuest();
  ASSERT_TRUE(glic_guest);

  content::WebContents* source_contents = active_tab->GetContents();
  CopyTextViaContentBrowserClient(source_contents->GetPrimaryMainFrame(),
                                  u"floaty blocked secret");
  ASSERT_EQ(ReadCurrentClipboardText(), u"floaty blocked secret");

  content::ClipboardEndpoint source =
      content::CreateClipboardEndpoint(*source_contents->GetPrimaryMainFrame());
  ui::ClipboardMetadata metadata = {
      .size = 21 * sizeof(char16_t),
      .format_type = ui::ClipboardFormatType::PlainTextType(),
      .seqno = ui::Clipboard::GetForCurrentThread()->GetSequenceNumber(
          ui::ClipboardBuffer::kCopyPaste),
  };
  content::ClipboardEndpoint destination = MakeGuestDestination(glic_guest);

  data_controls::DesktopDataControlsDialogTestHelper block_dialog(
      data_controls::DataControlsDialog::Type::kClipboardPasteBlock);
  content::ClipboardPasteData paste_data;
  paste_data.text = u"floaty blocked secret";
  base::test::TestFuture<std::optional<content::ClipboardPasteData>> future;
  content::GetContentClientForTesting()
      ->browser()
      ->IsClipboardPasteAllowedByPolicy(source, destination, metadata,
                                        std::move(paste_data),
                                        future.GetCallback());

  block_dialog.WaitForDialogToInitialize();
  EXPECT_EQ(future.Get(), std::nullopt);

  ASSERT_NE(block_dialog.dialog_delegate(), nullptr);
  views::Widget* dialog_widget = block_dialog.dialog_delegate()->GetWidget();
  ASSERT_NE(dialog_widget, nullptr);
  ASSERT_NE(instance->host().webui_contents(), nullptr);
  views::Widget* floaty_widget = views::Widget::GetTopLevelWidgetForNativeView(
      instance->host().webui_contents()->GetNativeView());
  views::Widget* browser_widget = views::Widget::GetWidgetForNativeWindow(
      GetBrowser()->GetWindow()->GetNativeWindow());
  ASSERT_NE(floaty_widget, nullptr);
  EXPECT_NE(floaty_widget, browser_widget);
  EXPECT_EQ(dialog_widget->parent(), floaty_widget);

  // Closing the detached Glic floaty must tear down the hosted modal dialog.
  ASSERT_OK(CloseAllEmbeddersAndWait(instance));
  block_dialog.WaitForDialogToClose();
}

IN_PROC_BROWSER_TEST_F(
    GlicPastePolicyBrowserTest,
    KeptInManagedChromeCopyPreservesEligibleAndBlocksIneligible) {
  data_controls::SetDataControls(GetProfile()->GetPrefs(), {R"({
        "sources": {
          "urls": ["*"]
        },
        "destinations": {
          "os_clipboard": true
        },
        "restrictions": [
          {"class": "CLIPBOARD", "level": "BLOCK"}
        ]
      })"});

  const GURL eligible_url =
      embedded_test_server()->GetURL("eligible.example.com", "/title1.html");
  tabs::TabInterface* active_tab = GetTabListInterface()->GetActiveTab();
  content::WebContents* source_contents = active_tab->GetContents();
  ASSERT_TRUE(content::NavigateToURL(source_contents, eligible_url));

  ASSERT_OK(OpenGlicForActiveTab());
  content::WebContents* glic_guest = GetReadyGuest();
  ASSERT_TRUE(glic_guest);
  content::ClipboardEndpoint destination = MakeGuestDestination(glic_guest);

  // 1. Eligible tab KeptInManagedChrome copy restores original text in Glic.
  {
    CopyTextViaContentBrowserClient(source_contents->GetPrimaryMainFrame(),
                                    u"kept in chrome secret");
    const std::u16string replacement = ReadCurrentClipboardText();
    ASSERT_FALSE(replacement.empty());
    ASSERT_NE(replacement, u"kept in chrome secret");

    const ui::ClipboardSequenceNumberToken copy_seqno =
        ui::Clipboard::GetForCurrentThread()->GetSequenceNumber(
            ui::ClipboardBuffer::kCopyPaste);
    ASSERT_EQ(data_controls::GetLastReplacedClipboardData().seqno, copy_seqno);
    ASSERT_EQ(data_controls::GetLastReplacedClipboardData().restriction_level,
              data_controls::CopyRestrictionLevel::kKeptInManagedChrome);

    content::ClipboardEndpoint source = content::CreateClipboardEndpoint(
        *source_contents->GetPrimaryMainFrame());
    ui::ClipboardMetadata metadata = {
        .size = replacement.size() * sizeof(char16_t),
        .format_type = ui::ClipboardFormatType::PlainTextType(),
        .seqno = copy_seqno,
    };
    content::ClipboardPasteData paste_data;
    paste_data.text = replacement;
    base::test::TestFuture<std::optional<content::ClipboardPasteData>> future;
    content::GetContentClientForTesting()
        ->browser()
        ->IsClipboardPasteAllowedByPolicy(source, destination, metadata,
                                          std::move(paste_data),
                                          future.GetCallback());
    std::optional<content::ClipboardPasteData> restored = future.Get();
    ASSERT_TRUE(restored.has_value());
    EXPECT_EQ(restored->text, u"kept in chrome secret");
  }

  // 2. Ineligible tab KeptInManagedChrome copy overwrites any stale pending
  // eligibility and blocks paste into Glic with kPageContextIneligible.
  enterprise_data_protection::PasteAllowedRequest::CleanupRequestsForTesting();
  const GURL ineligible_url =
      embedded_test_server()->GetURL("ineligible.example.com", "/title1.html");
  ASSERT_TRUE(content::NavigateToURL(source_contents, ineligible_url));
  {
    glic::StageClipboardCopyEligibility(true);

    CopyTextViaContentBrowserClient(source_contents->GetPrimaryMainFrame(),
                                    u"ineligible kept in chrome secret");
    const std::u16string replacement = ReadCurrentClipboardText();
    ASSERT_FALSE(replacement.empty());
    ASSERT_NE(replacement, u"ineligible kept in chrome secret");

    const ui::ClipboardSequenceNumberToken copy_seqno =
        ui::Clipboard::GetForCurrentThread()->GetSequenceNumber(
            ui::ClipboardBuffer::kCopyPaste);
    ASSERT_EQ(data_controls::GetLastReplacedClipboardData().seqno, copy_seqno);

    content::ClipboardEndpoint source = content::CreateClipboardEndpoint(
        *source_contents->GetPrimaryMainFrame());
    ui::ClipboardMetadata metadata = {
        .size = replacement.size() * sizeof(char16_t),
        .format_type = ui::ClipboardFormatType::PlainTextType(),
        .seqno = copy_seqno,
    };
    base::HistogramTester histogram_tester;
    content::ClipboardPasteData paste_data;
    paste_data.text = replacement;
    base::test::TestFuture<std::optional<content::ClipboardPasteData>> future;
    content::GetContentClientForTesting()
        ->browser()
        ->IsClipboardPasteAllowedByPolicy(source, destination, metadata,
                                          std::move(paste_data),
                                          future.GetCallback());
    EXPECT_EQ(future.Get(), std::nullopt);
    histogram_tester.ExpectUniqueSample(
        "Glic.Paste.FailedEligibilityReason.Web",
        0 /* kPageContextIneligible */, 1);
  }
}

#if BUILDFLAG(ENTERPRISE_CLOUD_CONTENT_ANALYSIS)
IN_PROC_BROWSER_TEST_F(
    GlicPastePolicyBrowserTest,
    IncognitoPasteShortCircuitsBeforeDataControlsWarnAndCloudScan) {
  policy::MockCloudPolicyClient* cloud_policy_client = SetupRealtimeReporting();
  EXPECT_CALL(*cloud_policy_client, UploadSecurityEvent).Times(0);

  enterprise_connectors::test::SetAnalysisConnector(
      GetProfile()->GetPrefs(), enterprise_connectors::BULK_DATA_ENTRY,
      R"({
        "service_provider": "google",
        "enable": [{"url_list": ["*"], "tags": ["dlp"]}],
        "block_until_verdict": 1,
        "minimum_data_size": 1
      })");
  enterprise_connectors::ContentAnalysisDelegate::SetFactoryForTesting(
      base::BindRepeating(
          &enterprise_connectors::test::FakeContentAnalysisDelegate::Create,
          base::DoNothing(),
          base::BindRepeating([](const std::string&, const base::FilePath&) {
            return enterprise_connectors::test::FakeContentAnalysisDelegate::
                SuccessfulResponse({"dlp"});
          }),
          "fake-dm-token"));

  data_controls::SetDataControls(GetProfile()->GetPrefs(), {R"({
        "sources": {
          "incognito": true
        },
        "destinations": {
          "gemini_in_chrome": true
        },
        "restrictions": [
          {"class": "CLIPBOARD", "level": "WARN"}
        ]
      })"});

  ASSERT_OK(OpenGlicForActiveTab());
  content::WebContents* glic_guest = GetReadyGuest();
  ASSERT_TRUE(glic_guest);

  BrowserWindowInterface* incognito_browser =
      InProcessBrowserTest::CreateIncognitoBrowser();
  content::WebContents* source_contents =
      incognito_browser->GetTabStripModel()->GetActiveWebContents();
  const GURL url =
      embedded_test_server()->GetURL("eligible.example.com", "/title1.html");
  ASSERT_TRUE(content::NavigateToURL(source_contents, url));

  CopyTextViaContentBrowserClient(source_contents->GetPrimaryMainFrame(),
                                  u"secret");
  ASSERT_EQ(ReadCurrentClipboardText(), u"secret");

  content::ClipboardEndpoint source =
      content::CreateClipboardEndpoint(*source_contents->GetPrimaryMainFrame());
  ui::ClipboardMetadata metadata = {
      .size = 6 * sizeof(char16_t),
      .format_type = ui::ClipboardFormatType::PlainTextType(),
      .seqno = ui::Clipboard::GetForCurrentThread()->GetSequenceNumber(
          ui::ClipboardBuffer::kCopyPaste),
  };
  content::ClipboardEndpoint destination = MakeGuestDestination(glic_guest);

  data_controls::DesktopDataControlsDialogTestHelper dialog_helper(
      data_controls::DataControlsDialog::Type::kClipboardPasteWarn);
  base::HistogramTester histogram_tester;

  content::ClipboardPasteData paste_data;
  paste_data.text = u"secret";
  base::test::TestFuture<std::optional<content::ClipboardPasteData>> future;
  content::GetContentClientForTesting()
      ->browser()
      ->IsClipboardPasteAllowedByPolicy(source, destination, metadata,
                                        std::move(paste_data),
                                        future.GetCallback());

  EXPECT_EQ(future.Get(), std::nullopt);
  EXPECT_EQ(dialog_helper.dialog(), nullptr);
  histogram_tester.ExpectUniqueSample("Glic.Paste.FailedEligibilityReason.Web",
                                      2 /* kCrossProfile */, 1);
  EXPECT_EQ(enterprise_connectors::test::FakeContentAnalysisDelegate::
                GetTotalAnalysisRequestsCount(),
            0);
}

IN_PROC_BROWSER_TEST_F(GlicPastePolicyBrowserTest,
                       IneligibleTabPasteNeverUploadsToCloudContentAnalysis) {
  policy::SetDMTokenForTesting(
      policy::DMToken::CreateValidToken("fake-dm-token"));
  enterprise_connectors::test::SetAnalysisConnector(
      GetProfile()->GetPrefs(), enterprise_connectors::BULK_DATA_ENTRY,
      R"({
        "service_provider": "google",
        "enable": [{"url_list": ["*"], "tags": ["dlp"]}],
        "block_until_verdict": 1,
        "minimum_data_size": 1
      })");
  enterprise_connectors::ContentAnalysisDelegate::SetFactoryForTesting(
      base::BindRepeating(
          &enterprise_connectors::test::FakeContentAnalysisDelegate::Create,
          base::DoNothing(),
          base::BindRepeating([](const std::string&, const base::FilePath&) {
            return enterprise_connectors::test::FakeContentAnalysisDelegate::
                SuccessfulResponse({"dlp"});
          }),
          "fake-dm-token"));

  const GURL url =
      embedded_test_server()->GetURL("ineligible.example.com", "/title1.html");
  tabs::TabInterface* active_tab = GetTabListInterface()->GetActiveTab();
  ASSERT_TRUE(content::NavigateToURL(active_tab->GetContents(), url));

  ASSERT_OK(OpenGlicForActiveTab());
  content::WebContents* source_contents = active_tab->GetContents();
  content::WebContents* glic_guest = GetReadyGuest();
  ASSERT_TRUE(glic_guest);

  CopyTextViaContentBrowserClient(source_contents->GetPrimaryMainFrame(),
                                  u"sensitive cse text");
  ASSERT_EQ(ReadCurrentClipboardText(), u"sensitive cse text");

  content::ClipboardEndpoint source =
      content::CreateClipboardEndpoint(*source_contents->GetPrimaryMainFrame());
  ui::ClipboardMetadata metadata = {
      .size = 18 * sizeof(char16_t),
      .format_type = ui::ClipboardFormatType::PlainTextType(),
      .seqno = ui::Clipboard::GetForCurrentThread()->GetSequenceNumber(
          ui::ClipboardBuffer::kCopyPaste),
  };
  content::ClipboardEndpoint destination = MakeGuestDestination(glic_guest);

  base::HistogramTester histogram_tester;
  content::ClipboardPasteData paste_data;
  paste_data.text = u"sensitive cse text";
  base::test::TestFuture<std::optional<content::ClipboardPasteData>> future;
  content::GetContentClientForTesting()
      ->browser()
      ->IsClipboardPasteAllowedByPolicy(source, destination, metadata,
                                        std::move(paste_data),
                                        future.GetCallback());

  EXPECT_EQ(future.Get(), std::nullopt);
  histogram_tester.ExpectUniqueSample("Glic.Paste.FailedEligibilityReason.Web",
                                      0 /* kPageContextIneligible */, 1);
  EXPECT_EQ(enterprise_connectors::test::FakeContentAnalysisDelegate::
                GetTotalAnalysisRequestsCount(),
            0);
}

IN_PROC_BROWSER_TEST_F(
    GlicPastePolicyBrowserTest,
    AsyncCopyScanOngoingThenBlockedShowsPasteBlockModalInGlic) {
  policy::SetDMTokenForTesting(
      policy::DMToken::CreateValidToken("fake-dm-token"));
  enterprise_connectors::test::SetAnalysisConnector(
      GetProfile()->GetPrefs(), enterprise_connectors::DATA_COPIED,
      R"({
        "service_provider": "google",
        "enable": [{"url_list": ["*"], "tags": ["dlp"]}],
        "block_until_verdict": 1,
        "minimum_data_size": 1
      })");

  PendingClipboardScanController scan_controller;
  base::RunLoop scan_deleted_loop;
  enterprise_connectors::ContentAnalysisDelegate::SetFactoryForTesting(
      base::BindRepeating(
          &ControllableClipboardAnalysisDelegate::Create, &scan_controller,
          scan_deleted_loop.QuitClosure(),
          base::BindRepeating([](const std::string&, const base::FilePath&) {
            return enterprise_connectors::test::FakeContentAnalysisDelegate::
                DlpResponse(enterprise_connectors::ContentAnalysisResponse::
                                Result::SUCCESS,
                            "dlp_copy_block",
                            enterprise_connectors::TriggeredRule::BLOCK);
          }),
          "fake-dm-token"));

  const GURL eligible_url =
      embedded_test_server()->GetURL("eligible.example.com", "/title1.html");
  tabs::TabInterface* active_tab = GetTabListInterface()->GetActiveTab();
  ASSERT_TRUE(content::NavigateToURL(active_tab->GetContents(), eligible_url));

  ASSERT_OK(OpenGlicForActiveTab());
  content::WebContents* source_contents = active_tab->GetContents();
  content::WebContents* glic_guest = GetReadyGuest();
  ASSERT_TRUE(glic_guest);

  // Trigger a real copy that enters CopyIfAllowedByContentAnalysis and pauses
  // while the cloud scan is in flight.
  CopyTextViaContentBrowserClient(source_contents->GetPrimaryMainFrame(),
                                  u"deep scanned secret");
  scan_controller.WaitForUploadStart();

  const std::u16string scanning_placeholder = l10n_util::GetStringUTF16(
      IDS_ENTERPRISE_CONTENT_ANALYSIS_COPY_SCANNING_MESSAGE);
  EXPECT_EQ(ReadCurrentClipboardText(), scanning_placeholder);
  const ui::ClipboardSequenceNumberToken ongoing_seqno =
      ui::Clipboard::GetForCurrentThread()->GetSequenceNumber(
          ui::ClipboardBuffer::kCopyPaste);
  ASSERT_EQ(data_controls::GetLastReplacedClipboardData().seqno, ongoing_seqno);
  ASSERT_EQ(data_controls::GetLastReplacedClipboardData().restriction_level,
            data_controls::CopyRestrictionLevel::kOngoingScan);

  content::ClipboardEndpoint source =
      content::CreateClipboardEndpoint(*source_contents->GetPrimaryMainFrame());
  content::ClipboardEndpoint destination = MakeGuestDestination(glic_guest);

  // 1. Pasting into Glic while kOngoingScan is active is silently blocked
  // without showing kClipboardPasteBlock.
  {
    ui::ClipboardMetadata ongoing_metadata = {
        .size = scanning_placeholder.size() * sizeof(char16_t),
        .format_type = ui::ClipboardFormatType::PlainTextType(),
        .seqno = ongoing_seqno,
    };
    data_controls::DesktopDataControlsDialogTestHelper block_dialog(
        data_controls::DataControlsDialog::Type::kClipboardPasteBlock);
    base::HistogramTester histogram_tester;

    content::ClipboardPasteData paste_data;
    paste_data.text = scanning_placeholder;
    base::test::TestFuture<std::optional<content::ClipboardPasteData>> future;
    content::GetContentClientForTesting()
        ->browser()
        ->IsClipboardPasteAllowedByPolicy(source, destination, ongoing_metadata,
                                          std::move(paste_data),
                                          future.GetCallback());

    EXPECT_EQ(future.Get(), std::nullopt);
    EXPECT_EQ(block_dialog.dialog(), nullptr);
    histogram_tester.ExpectTotalCount("Glic.Paste.FailedEligibilityReason.Web",
                                      0);
  }

  // 2. Complete the cloud copy scan with BLOCK: OnCopyDeepScanComplete writes
  // the blocked message to a new clipboard sequence number and binds copy-time
  // eligibility to that new sequence number.
  scan_controller.CompletePendingScan();
  scan_deleted_loop.Run();

  const std::u16string blocked_message = l10n_util::GetStringUTF16(
      IDS_ENTERPRISE_CONTENT_ANALYSIS_COPY_BLOCKED_MESSAGE);
  EXPECT_EQ(ReadCurrentClipboardText(), blocked_message);
  const ui::ClipboardSequenceNumberToken blocked_seqno =
      ui::Clipboard::GetForCurrentThread()->GetSequenceNumber(
          ui::ClipboardBuffer::kCopyPaste);
  EXPECT_NE(blocked_seqno, ongoing_seqno);
  ASSERT_EQ(data_controls::GetLastReplacedClipboardData().seqno, blocked_seqno);
  ASSERT_EQ(data_controls::GetLastReplacedClipboardData().restriction_level,
            data_controls::CopyRestrictionLevel::kBlocked);

  // Pasting into Glic now surfaces kClipboardPasteBlock on the final seqno.
  {
    ui::ClipboardMetadata blocked_metadata = {
        .size = blocked_message.size() * sizeof(char16_t),
        .format_type = ui::ClipboardFormatType::PlainTextType(),
        .seqno = blocked_seqno,
    };
    data_controls::DesktopDataControlsDialogTestHelper block_dialog(
        data_controls::DataControlsDialog::Type::kClipboardPasteBlock);
    base::HistogramTester histogram_tester;

    content::ClipboardPasteData paste_data;
    paste_data.text = blocked_message;
    base::test::TestFuture<std::optional<content::ClipboardPasteData>> future;
    content::GetContentClientForTesting()
        ->browser()
        ->IsClipboardPasteAllowedByPolicy(source, destination, blocked_metadata,
                                          std::move(paste_data),
                                          future.GetCallback());

    block_dialog.WaitForDialogToInitialize();
    EXPECT_EQ(future.Get(), std::nullopt);
    histogram_tester.ExpectTotalCount("Glic.Paste.FailedEligibilityReason.Web",
                                      0);
    block_dialog.CloseDialogWithoutBypass();
    block_dialog.WaitForDialogToClose();
  }
}

IN_PROC_BROWSER_TEST_F(
    GlicPastePolicyBrowserTest,
    AsyncCopyScanAllowedSucceedsAndCapturesCopyTimeEligibilityAcrossNavigation) {
  policy::SetDMTokenForTesting(
      policy::DMToken::CreateValidToken("fake-dm-token"));
  enterprise_connectors::test::SetAnalysisConnector(
      GetProfile()->GetPrefs(), enterprise_connectors::DATA_COPIED,
      R"({
        "service_provider": "google",
        "enable": [{"url_list": ["*"], "tags": ["dlp"]}],
        "block_until_verdict": 1,
        "minimum_data_size": 1
      })");

  PendingClipboardScanController scan_controller;
  base::RepeatingClosure on_scan_deleted = base::DoNothing();
  enterprise_connectors::ContentAnalysisDelegate::SetFactoryForTesting(
      base::BindLambdaForTesting(
          [&](content::WebContents* web_contents,
              enterprise_connectors::ContentAnalysisDelegate::Data data,
              enterprise_connectors::ContentAnalysisDelegate::CompletionCallback
                  callback,
              enterprise_connectors::DeepScanAccessPoint access_point) {
            return ControllableClipboardAnalysisDelegate::Create(
                &scan_controller, on_scan_deleted,
                base::BindRepeating([](const std::string&,
                                       const base::FilePath&) {
                  return enterprise_connectors::test::
                      FakeContentAnalysisDelegate::SuccessfulResponse({"dlp"});
                }),
                "fake-dm-token", web_contents, std::move(data),
                std::move(callback), access_point);
          }));

  const GURL eligible_url =
      embedded_test_server()->GetURL("eligible.example.com", "/title1.html");
  const GURL ineligible_url =
      embedded_test_server()->GetURL("ineligible.example.com", "/title2.html");
  tabs::TabInterface* active_tab = GetTabListInterface()->GetActiveTab();
  ASSERT_TRUE(content::NavigateToURL(active_tab->GetContents(), eligible_url));

  ASSERT_OK(OpenGlicForActiveTab());
  content::WebContents* source_contents = active_tab->GetContents();
  content::WebContents* glic_guest = GetReadyGuest();
  ASSERT_TRUE(glic_guest);
  content::ClipboardEndpoint destination = MakeGuestDestination(glic_guest);

  // 1. Open eligible tab async copy scan allowed -> writes original payload to
  // clipboard and succeeds when pasted into Glic.
  {
    base::RunLoop scan_done;
    on_scan_deleted = scan_done.QuitClosure();

    CopyTextViaContentBrowserClient(source_contents->GetPrimaryMainFrame(),
                                    u"allowed scanned payload");
    scan_controller.WaitForUploadStart();
    scan_controller.CompletePendingScan();
    scan_done.Run();

    ASSERT_EQ(ReadCurrentClipboardText(), u"allowed scanned payload");
    content::ClipboardEndpoint source = content::CreateClipboardEndpoint(
        *source_contents->GetPrimaryMainFrame());
    ui::ClipboardMetadata metadata = {
        .size = 23 * sizeof(char16_t),
        .format_type = ui::ClipboardFormatType::PlainTextType(),
        .seqno = ui::Clipboard::GetForCurrentThread()->GetSequenceNumber(
            ui::ClipboardBuffer::kCopyPaste),
    };
    content::ClipboardPasteData paste_data;
    paste_data.text = u"allowed scanned payload";
    base::test::TestFuture<std::optional<content::ClipboardPasteData>> future;
    content::GetContentClientForTesting()
        ->browser()
        ->IsClipboardPasteAllowedByPolicy(source, destination, metadata,
                                          std::move(paste_data),
                                          future.GetCallback());
    std::optional<content::ClipboardPasteData> allowed = future.Get();
    ASSERT_TRUE(allowed.has_value());
    EXPECT_EQ(allowed->text, u"allowed scanned payload");
  }

  // 2. Copy initiated on an ineligible tab whose tab navigates to an eligible
  // URL while the cloud scan is in flight must preserve copy-time ineligibility
  // on the final clipboard write.
  ASSERT_TRUE(content::NavigateToURL(source_contents, ineligible_url));
  {
    base::RunLoop scan_done;
    on_scan_deleted = scan_done.QuitClosure();

    CopyTextViaContentBrowserClient(source_contents->GetPrimaryMainFrame(),
                                    u"ineligible midscan payload");
    scan_controller.WaitForUploadStart();

    // Navigate the source tab to an eligible URL before the scan completes!
    ASSERT_TRUE(content::NavigateToURL(source_contents, eligible_url));

    scan_controller.CompletePendingScan();
    scan_done.Run();

    ASSERT_EQ(ReadCurrentClipboardText(), u"ineligible midscan payload");
    content::ClipboardEndpoint current_tab_source =
        content::CreateClipboardEndpoint(
            *source_contents->GetPrimaryMainFrame());
    ui::ClipboardMetadata metadata = {
        .size = 26 * sizeof(char16_t),
        .format_type = ui::ClipboardFormatType::PlainTextType(),
        .seqno = ui::Clipboard::GetForCurrentThread()->GetSequenceNumber(
            ui::ClipboardBuffer::kCopyPaste),
    };

    base::HistogramTester histogram_tester;
    content::ClipboardPasteData paste_data;
    paste_data.text = u"ineligible midscan payload";
    base::test::TestFuture<std::optional<content::ClipboardPasteData>> future;
    content::GetContentClientForTesting()
        ->browser()
        ->IsClipboardPasteAllowedByPolicy(current_tab_source, destination,
                                          metadata, std::move(paste_data),
                                          future.GetCallback());

    EXPECT_EQ(future.Get(), std::nullopt);
    histogram_tester.ExpectUniqueSample(
        "Glic.Paste.FailedEligibilityReason.Web",
        0 /* kPageContextIneligible */, 1);
  }
}

IN_PROC_BROWSER_TEST_F(
    GlicPastePolicyBrowserTest,
    WarnPasteRuleWithBlockedCopyShowsBlockModalWithoutWarnOrBypassReport) {
  policy::MockCloudPolicyClient* cloud_policy_client = SetupRealtimeReporting();

  enterprise_connectors::test::SetAnalysisConnector(
      GetProfile()->GetPrefs(), enterprise_connectors::DATA_COPIED,
      R"({
        "service_provider": "google",
        "enable": [{"url_list": ["*"], "tags": ["dlp"]}],
        "block_until_verdict": 1,
        "minimum_data_size": 1
      })");
  base::RunLoop copy_scan_done;
  enterprise_connectors::ContentAnalysisDelegate::SetFactoryForTesting(
      base::BindRepeating(
          &enterprise_connectors::test::FakeContentAnalysisDelegate::Create,
          copy_scan_done.QuitClosure(),
          base::BindRepeating([](const std::string&, const base::FilePath&) {
            return enterprise_connectors::test::FakeContentAnalysisDelegate::
                DlpResponse(enterprise_connectors::ContentAnalysisResponse::
                                Result::SUCCESS,
                            "dlp_copy_block",
                            enterprise_connectors::TriggeredRule::BLOCK);
          }),
          "fake-dm-token"));

  data_controls::SetDataControls(GetProfile()->GetPrefs(), {R"({
        "name": "warn_paste_to_glic",
        "rule_id": "789",
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

  const GURL eligible_url =
      embedded_test_server()->GetURL("eligible.example.com", "/title1.html");
  tabs::TabInterface* active_tab = GetTabListInterface()->GetActiveTab();
  ASSERT_TRUE(content::NavigateToURL(active_tab->GetContents(), eligible_url));

  ASSERT_OK(OpenGlicForActiveTab());
  content::WebContents* source_contents = active_tab->GetContents();
  content::WebContents* glic_guest = GetReadyGuest();
  ASSERT_TRUE(glic_guest);

  // Allow the copy-stage DLP report during CopyTextViaContentBrowserClient,
  // then require ZERO security event reports during the paste attempt.
  EXPECT_CALL(*cloud_policy_client, UploadSecurityEvent)
      .WillRepeatedly([](bool,
                         ::chrome::cros::reporting::proto::UploadEventsRequest,
                         policy::CloudPolicyClient::ResultCallback callback) {
        std::move(callback).Run(
            policy::CloudPolicyClient::Result(policy::DM_STATUS_SUCCESS));
      });

  CopyTextViaContentBrowserClient(source_contents->GetPrimaryMainFrame(),
                                  u"blocked copy under warn rule");
  copy_scan_done.Run();
  testing::Mock::VerifyAndClearExpectations(cloud_policy_client);

  const std::u16string blocked_message = l10n_util::GetStringUTF16(
      IDS_ENTERPRISE_CONTENT_ANALYSIS_COPY_BLOCKED_MESSAGE);
  ASSERT_EQ(ReadCurrentClipboardText(), blocked_message);
  const ui::ClipboardSequenceNumberToken blocked_seqno =
      ui::Clipboard::GetForCurrentThread()->GetSequenceNumber(
          ui::ClipboardBuffer::kCopyPaste);
  ASSERT_EQ(data_controls::GetLastReplacedClipboardData().seqno, blocked_seqno);
  ASSERT_EQ(data_controls::GetLastReplacedClipboardData().restriction_level,
            data_controls::CopyRestrictionLevel::kBlocked);

  // Zero paste WARNED or BYPASSED reports may be emitted when pasting a
  // kBlocked copy into Glic, and kClipboardPasteBlock must appear directly
  // without showing kClipboardPasteWarn.
  EXPECT_CALL(*cloud_policy_client, UploadSecurityEvent).Times(0);

  content::ClipboardEndpoint source =
      content::CreateClipboardEndpoint(*source_contents->GetPrimaryMainFrame());
  content::ClipboardEndpoint destination = MakeGuestDestination(glic_guest);
  ui::ClipboardMetadata metadata = {
      .size = blocked_message.size() * sizeof(char16_t),
      .format_type = ui::ClipboardFormatType::PlainTextType(),
      .seqno = blocked_seqno,
  };

  // DesktopDataControlsDialogTestHelper::OnConstructed asserts that only a
  // single DesktopDataControlsDialog is constructed and that its type equals
  // kClipboardPasteBlock (never kClipboardPasteWarn).
  data_controls::DesktopDataControlsDialogTestHelper block_dialog(
      data_controls::DataControlsDialog::Type::kClipboardPasteBlock);

  content::ClipboardPasteData paste_data;
  paste_data.text = blocked_message;
  base::test::TestFuture<std::optional<content::ClipboardPasteData>> future;
  content::GetContentClientForTesting()
      ->browser()
      ->IsClipboardPasteAllowedByPolicy(source, destination, metadata,
                                        std::move(paste_data),
                                        future.GetCallback());

  block_dialog.WaitForDialogToInitialize();
  EXPECT_EQ(future.Get(), std::nullopt);

  block_dialog.CloseDialogWithoutBypass();
  block_dialog.WaitForDialogToClose();
}
#endif  // BUILDFLAG(ENTERPRISE_CLOUD_CONTENT_ANALYSIS)

}  // namespace
}  // namespace glic
