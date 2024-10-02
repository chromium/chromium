// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/analysis/content_analysis_dialog.h"

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/connectors/analysis/content_analysis_delegate.h"
#include "chrome/browser/enterprise/connectors/analysis/content_analysis_downloads_delegate.h"
#include "chrome/browser/enterprise/connectors/analysis/content_analysis_features.h"
#include "chrome/browser/enterprise/connectors/test/deep_scanning_browsertest_base.h"
#include "chrome/browser/enterprise/connectors/test/deep_scanning_test_utils.h"
#include "chrome/browser/enterprise/connectors/test/fake_content_analysis_delegate.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/deep_scanning_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/download/public/common/mock_download_item.h"
#include "components/enterprise/common/proto/connectors.pb.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/textarea/textarea.h"
#include "ui/views/controls/throbber.h"
#include "ui/views/test/ax_event_counter.h"
#include "ui/views/test/views_test_utils.h"

namespace enterprise_connectors {

namespace {

constexpr base::TimeDelta kNoDelay = base::Seconds(0);
constexpr base::TimeDelta kSmallDelay = base::Milliseconds(300);
constexpr base::TimeDelta kNormalDelay = base::Milliseconds(500);

constexpr char kBlockingScansForDlpAndMalware[] = R"(
{
  "service_provider": "google",
  "enable": [
    {
      "url_list": ["*"],
      "tags": ["dlp", "malware"]
    }
  ],
  "block_until_verdict": 1
})";

constexpr char kBlockingScansForDlp[] = R"(
{
  "service_provider": "google",
  "enable": [
    {
      "url_list": ["*"],
      "tags": ["dlp"]
    }
  ],
  "block_until_verdict": 1
})";

constexpr char kBlockingScansForDlpAndMalwareWithCustomMessage[] = R"(
{
  "service_provider": "google",
  "enable": [
    {
      "url_list": ["*"],
      "tags": ["dlp", "malware"]
    }
  ],
  "block_until_verdict": 1,
  "custom_messages": [{
    "message": "Custom message",
    "learn_more_url": "http://www.example.com/",
    "tag": "dlp"
  }]
})";

std::string text() {
  return std::string(100, 'a');
}

// Tests the behavior of the dialog in the following ways:
// - It shows the appropriate buttons depending on its state.
// - It transitions from states in the correct order.
// - It respects time constraints (minimum shown time, initial delay, timeout)
// - It is always destroyed, therefore |quit_closure_| is called in the dtor
//   observer.
// - It sends accessibility events correctly.
class ContentAnalysisDialogBehaviorBrowserTest
    : public test::DeepScanningBrowserTestBase,
      public ContentAnalysisDialog::TestObserver,
      public testing::WithParamInterface<
          std::tuple<bool, bool, base::TimeDelta>> {
 public:
  ContentAnalysisDialogBehaviorBrowserTest()
      : ax_event_counter_(views::AXEventManager::Get()) {
    ContentAnalysisDialog::SetObserverForTesting(this);

    expected_scan_result_ = dlp_success() && malware_success();
  }

  void ConstructorCalled(ContentAnalysisDialog* dialog,
                         base::TimeTicks timestamp) override {
    ctor_called_timestamp_ = timestamp;
    dialog_ = dialog;

    // The scan should be pending when constructed.
    EXPECT_TRUE(dialog_->is_pending());

    // The dialog should only be constructed once.
    EXPECT_FALSE(ctor_called_);
    ctor_called_ = true;
  }

  void ViewsFirstShown(ContentAnalysisDialog* dialog,
                       base::TimeTicks timestamp) override {
    DCHECK_EQ(dialog, dialog_);
    first_shown_timestamp_ = timestamp;

    // The dialog can only be first shown in the pending or failure case.
    EXPECT_TRUE(dialog_->is_pending() || dialog_->is_failure());

    // If the failure dialog was shown immediately, ensure that was expected and
    // set |pending_shown_| for future assertions.
    if (dialog_->is_failure()) {
      EXPECT_FALSE(expected_scan_result_);
      pending_shown_ = false;
    } else {
      pending_shown_ = true;
    }

    // The dialog's buttons should be Cancel in the pending and fail case.
    EXPECT_EQ(dialog_->buttons(),
              static_cast<int>(ui::mojom::DialogButton::kCancel));

    // Record the number of AX events until now to check if the text update adds
    // one later.
    ax_events_count_when_first_shown_ =
        ax_event_counter_.GetCount(ax::mojom::Event::kAlert);

    // The dialog should only be shown once some time after being constructed.
    EXPECT_TRUE(ctor_called_);
    EXPECT_FALSE(dialog_first_shown_);

    // The initial dialog should only have the top views that are present on
    // every state initialized, and everything else should be null.
    EXPECT_TRUE(dialog->GetTopImageForTesting());
    EXPECT_TRUE(dialog->GetSideIconSpinnerForTesting());
    EXPECT_TRUE(dialog->GetMessageForTesting());
    EXPECT_FALSE(dialog->GetLearnMoreLinkForTesting());
    EXPECT_FALSE(dialog->GetBypassJustificationLabelForTesting());
    EXPECT_FALSE(dialog->GetBypassJustificationTextareaForTesting());
    EXPECT_FALSE(dialog->GetJustificationTextLengthForTesting());

    dialog_first_shown_ = true;
  }

  void DialogUpdated(ContentAnalysisDialog* dialog,
                     FinalContentAnalysisResult result) override {
    DCHECK_EQ(dialog, dialog_);
    dialog_updated_timestamp_ = base::TimeTicks::Now();

    // The dialog should not be updated if the failure was shown immediately.
    EXPECT_TRUE(pending_shown_);

    // The dialog should only be updated after an initial delay.
    base::TimeDelta delay = dialog_updated_timestamp_ - first_shown_timestamp_;
    EXPECT_GE(delay, ContentAnalysisDialog::GetMinimumPendingDialogTime());

    // The dialog can only be updated to the success or failure case.
    EXPECT_TRUE(dialog_->is_result());
    bool is_success = result == FinalContentAnalysisResult::SUCCESS;
    EXPECT_EQ(dialog_->is_success(), is_success);
    EXPECT_EQ(dialog_->is_success(), expected_scan_result_);

    // The dialog's buttons should be Cancel in the fail case and nothing in the
    // success case.
    ui::mojom::DialogButton expected_buttons =
        dialog_->is_success() ? ui::mojom::DialogButton::kNone
                              : ui::mojom::DialogButton::kCancel;
    EXPECT_EQ(static_cast<int>(expected_buttons), dialog_->buttons());

    // The dialog should only be updated once some time after being shown.
    EXPECT_TRUE(dialog_first_shown_);
    EXPECT_FALSE(dialog_updated_);

    // TODO(crbug.com/40150258): Re-enable this for Mac.
#if !BUILDFLAG(IS_MAC)
    // The dialog being updated implies an accessibility alert is sent.
    EXPECT_EQ(ax_events_count_when_first_shown_ + 1,
              ax_event_counter_.GetCount(ax::mojom::Event::kAlert));
#endif

    // The updated dialog should have every relevant view initialized.
    EXPECT_TRUE(dialog->GetTopImageForTesting());
    EXPECT_FALSE(dialog->GetSideIconSpinnerForTesting());
    EXPECT_TRUE(dialog->GetMessageForTesting());
    EXPECT_EQ(!!dialog->GetLearnMoreLinkForTesting(),
              dialog->has_learn_more_url());
    EXPECT_EQ(!!dialog->GetBypassJustificationLabelForTesting(),
              dialog->bypass_requires_justification());
    EXPECT_EQ(!!dialog->GetBypassJustificationTextareaForTesting(),
              dialog->bypass_requires_justification());
    EXPECT_EQ(!!dialog->GetJustificationTextLengthForTesting(),
              dialog->bypass_requires_justification());

    dialog_updated_ = true;
  }

  void CancelDialogAndDeleteCalled(ContentAnalysisDialog* dialog,
                                   FinalContentAnalysisResult result) override {
    EXPECT_EQ(dialog_, dialog);
    EXPECT_NE(result, FinalContentAnalysisResult::FAIL_CLOSED);

    if (dialog_->is_cloud()) {
      EXPECT_FALSE(dialog_->is_failure());
      EXPECT_FALSE(dialog_->is_warning());
    }
  }

  void DestructorCalled(ContentAnalysisDialog* dialog) override {
    dtor_called_timestamp_ = base::TimeTicks::Now();

    EXPECT_TRUE(dialog);
    EXPECT_EQ(dialog_, dialog);
    EXPECT_EQ(dialog_->is_success(), expected_scan_result_);

    if (dialog_first_shown_) {
      // Ensure the dialog update only occurred if the pending state was shown.
      EXPECT_EQ(pending_shown_, dialog_updated_);

      // Ensure the success UI timed out properly.
      EXPECT_TRUE(dialog_->is_result());
      if (dialog_->is_success()) {
        // The success dialog should stay open for some time.
        base::TimeDelta delay =
            dtor_called_timestamp_ - dialog_updated_timestamp_;
        EXPECT_GE(delay, ContentAnalysisDialog::GetSuccessDialogTimeout());

        EXPECT_EQ(static_cast<int>(ui::mojom::DialogButton::kNone),
                  dialog_->buttons());
      } else {
        EXPECT_EQ(static_cast<int>(ui::mojom::DialogButton::kCancel),
                  dialog_->buttons());
      }
    } else {
      // Ensure the dialog update didn't occur if no dialog was shown.
      EXPECT_FALSE(dialog_updated_);
    }
    EXPECT_TRUE(ctor_called_);

    // The test is over once the views are destroyed.
    CallQuitClosure();
  }

  bool dlp_success() const { return std::get<0>(GetParam()); }

  bool malware_success() const { return std::get<1>(GetParam()); }

  base::TimeDelta response_delay() const { return std::get<2>(GetParam()); }

 private:
  raw_ptr<ContentAnalysisDialog, DanglingUntriaged> dialog_;

  base::TimeTicks ctor_called_timestamp_;
  base::TimeTicks first_shown_timestamp_;
  base::TimeTicks dialog_updated_timestamp_;
  base::TimeTicks dtor_called_timestamp_;

  bool pending_shown_ = false;
  bool ctor_called_ = false;
  bool dialog_first_shown_ = false;
  bool dialog_updated_ = false;

  bool expected_scan_result_;

  int ax_events_count_when_first_shown_ = 0;
  views::test::AXEventCounter ax_event_counter_;
};

// Tests the behavior of the dialog in the following ways:
// - It closes when the "Cancel" button is clicked.
// - It returns a negative verdict on the scanned content.
// - The "CancelledByUser" metrics are recorded.
class ContentAnalysisDialogCancelPendingScanBrowserTest
    : public test::DeepScanningBrowserTestBase,
      public ContentAnalysisDialog::TestObserver {
 public:
  ContentAnalysisDialogCancelPendingScanBrowserTest() {
    ContentAnalysisDialog::SetObserverForTesting(this);
  }

  void ViewsFirstShown(ContentAnalysisDialog* dialog,
                       base::TimeTicks timestamp) override {
    // Simulate the user clicking "Cancel" after the dialog is first shown.
    dialog->CancelDialog();
  }

  void DestructorCalled(ContentAnalysisDialog* dialog) override {
    // The test is over once the dialog is destroyed.
    CallQuitClosure();
  }

  void ValidateMetrics() const {
    ASSERT_EQ(
        2u,
        histograms_.GetTotalCountsForPrefix("SafeBrowsing.DeepScan.").size());
    ASSERT_EQ(1u, histograms_
                      .GetTotalCountsForPrefix(
                          "SafeBrowsing.DeepScan.Upload.Duration")
                      .size());
    ASSERT_EQ(1u,
              histograms_
                  .GetTotalCountsForPrefix(
                      "SafeBrowsing.DeepScan.Upload.CancelledByUser.Duration")
                  .size());
  }

 private:
  base::HistogramTester histograms_;
};

// Tests the behavior of the dialog in the following ways:
// - It shows the appropriate buttons depending when showing a warning.
// - It calls the appropriate methods when the user bypasses/respects the
//   warning.
class ContentAnalysisDialogWarningBrowserTest
    : public test::DeepScanningBrowserTestBase,
      public ContentAnalysisDialog::TestObserver,
      public testing::WithParamInterface<bool> {
 public:
  ContentAnalysisDialogWarningBrowserTest() {
    ContentAnalysisDialog::SetObserverForTesting(this);
  }

  void ViewsFirstShown(ContentAnalysisDialog* dialog,
                       base::TimeTicks timestamp) override {
    // The dialog is first shown in the pending state.
    ASSERT_TRUE(dialog->is_pending());

    ASSERT_EQ(dialog->buttons(),
              static_cast<int>(ui::mojom::DialogButton::kCancel));
  }

  void DialogUpdated(ContentAnalysisDialog* dialog,
                     FinalContentAnalysisResult result) override {
    ASSERT_TRUE(dialog->is_warning());

    // The dialog's buttons should be Ok and Cancel.
    ASSERT_EQ(static_cast<int>(ui::mojom::DialogButton::kOk) |
                  static_cast<int>(ui::mojom::DialogButton::kCancel),
              dialog->buttons());

    SimulateClickAndEndTest(dialog);
  }

  void SimulateClickAndEndTest(ContentAnalysisDialog* dialog) {
    if (user_bypasses_warning())
      dialog->AcceptDialog();
    else
      dialog->CancelDialog();

    CallQuitClosure();
  }

  bool user_bypasses_warning() { return GetParam(); }
};

// Tests the behavior of the dialog in the following ways:
// - It shows the appropriate message depending on its access point and scan
//   type (file or text).
// - It shows the appropriate top image depending on its access point and scan
//   type.
// - It shows the appropriate spinner depending on its state.
class ContentAnalysisDialogAppearanceBrowserTest
    : public test::DeepScanningBrowserTestBase,
      public ContentAnalysisDialog::TestObserver,
      public testing::WithParamInterface<
          std::tuple<bool,
                     bool,
                     safe_browsing::DeepScanAccessPoint,
                     bool>> {
 public:
  ContentAnalysisDialogAppearanceBrowserTest() {
    ContentAnalysisDialog::SetObserverForTesting(this);
  }

  void ViewsFirstShown(ContentAnalysisDialog* dialog,
                       base::TimeTicks timestamp) override {
    // The dialog initially shows the pending message for the appropriate access
    // point and scan type.
    std::u16string pending_message = dialog->GetMessageForTesting()->GetText();
    std::u16string expected_message;
    if (access_point() == safe_browsing::DeepScanAccessPoint::PRINT) {
      expected_message = l10n_util::GetStringUTF16(
          IDS_DEEP_SCANNING_DIALOG_PRINT_PENDING_MESSAGE);
    } else {
      expected_message = l10n_util::GetPluralStringFUTF16(
          IDS_DEEP_SCANNING_DIALOG_UPLOAD_PENDING_MESSAGE, file_scan() ? 1 : 0);
    }
    ASSERT_EQ(pending_message, expected_message);

    // The top image is the pending one corresponding to the access point.
    const gfx::ImageSkia& actual_image =
        dialog->GetTopImageForTesting()->GetImage();
    const bool use_dark = dialog->ShouldUseDarkTopImage();
    int expected_image_id =
        use_dark ? IDR_UPLOAD_SCANNING_DARK : IDR_UPLOAD_SCANNING;
    gfx::ImageSkia* expected_image =
        ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
            expected_image_id);
    ASSERT_TRUE(expected_image->BackedBySameObjectAs(actual_image));

    // The spinner should be present.
    ASSERT_TRUE(dialog->GetSideIconSpinnerForTesting());
  }

  void DialogUpdated(ContentAnalysisDialog* dialog,
                     FinalContentAnalysisResult result) override {
    // The dialog shows the failure or success message for the appropriate
    // access point and scan type.
    std::u16string final_message = dialog->GetMessageForTesting()->GetText();
    std::u16string expected_message = GetExpectedMessage();

    ASSERT_EQ(final_message, expected_message);

    // The top image is the failure/success one corresponding to the access
    // point and scan type.
    const gfx::ImageSkia& actual_image =
        dialog->GetTopImageForTesting()->GetImage();
    const bool use_dark = dialog->ShouldUseDarkTopImage();
    int expected_image_id =
        success()
            ? (use_dark ? IDR_UPLOAD_SUCCESS_DARK : IDR_UPLOAD_SUCCESS)
            : (use_dark ? IDR_UPLOAD_VIOLATION_DARK : IDR_UPLOAD_VIOLATION);
    gfx::ImageSkia* expected_image =
        ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
            expected_image_id);
    ASSERT_TRUE(expected_image->BackedBySameObjectAs(actual_image));

    // The spinner should not be present in the final result since nothing is
    // pending.
    ASSERT_FALSE(dialog->GetSideIconSpinnerForTesting());
  }

  virtual std::u16string GetExpectedMessage() {
    if (access_point() == safe_browsing::DeepScanAccessPoint::PRINT) {
      return success() ? l10n_util::GetStringUTF16(
                             IDS_DEEP_SCANNING_DIALOG_PRINT_SUCCESS_MESSAGE)
                       : l10n_util::GetStringUTF16(
                             IDS_DEEP_SCANNING_DIALOG_PRINT_WARNING_MESSAGE);
    }
    int files_count = file_scan() ? 1 : 0;
    return success()
               ? l10n_util::GetPluralStringFUTF16(
                     IDS_DEEP_SCANNING_DIALOG_SUCCESS_MESSAGE, files_count)
               : l10n_util::GetPluralStringFUTF16(
                     IDS_DEEP_SCANNING_DIALOG_UPLOAD_FAILURE_MESSAGE,
                     files_count);
  }

  void DestructorCalled(ContentAnalysisDialog* dialog) override {
    // End the test once the dialog gets destroyed.
    CallQuitClosure();
  }

  bool file_scan() const { return std::get<0>(GetParam()); }

  bool success() const { return std::get<1>(GetParam()); }

  safe_browsing::DeepScanAccessPoint access_point() const {
    return std::get<2>(GetParam());
  }

  bool has_custom_rule_message() { return std::get<3>(GetParam()); }
};

// Tests the behavior of the dialog in the same way as
// ContentAnalysisDialogAppearanceBrowserTest but with a custom
// message set by the admin.
class ContentAnalysisDialogCustomMessageBrowserTest
    : public ContentAnalysisDialogAppearanceBrowserTest {
 private:
  void DialogUpdated(ContentAnalysisDialog* dialog,
                     FinalContentAnalysisResult result) override {
    // The dialog shows the failure or success message for the appropriate
    // access point and scan type.
    views::StyledLabel* final_message = dialog->GetMessageForTesting();
    ASSERT_TRUE(final_message);

    if ((dialog->is_failure() || dialog->is_warning()) &&
        has_custom_rule_message()) {
      // Run layout to get children.
      views::test::RunScheduledLayout(final_message);
      final_message->SetBounds(0, 0, 1000, 1000);

      // Three children as we insert the custom message in the middle of
      // IDS_DEEP_SCANNING_DIALOG_CUSTOM_MESSAGE.
      ASSERT_EQ(3u, final_message->children().size());

      ASSERT_EQ(u"Custom rule message",
                final_message->GetFirstLinkForTesting()->GetText());
      ASSERT_EQ(
          gfx::Font::UNDERLINE,
          final_message->GetFirstLinkForTesting()->font_list().GetFontStyle());

      // Click on link and check if correct page opens in new tab.
      content::WebContentsAddedObserver new_tab_observer;
      final_message->ClickFirstLinkForTesting();
      ASSERT_EQ(GURL("http://example.com"),
                new_tab_observer.GetWebContents()->GetVisibleURL());
    } else {
      std::u16string expected_message = GetExpectedMessage();
      ASSERT_EQ(final_message->GetText(), expected_message);
    }
  }
  std::u16string GetExpectedMessage() override {
    if (access_point() == safe_browsing::DeepScanAccessPoint::PRINT) {
      return success() ? l10n_util::GetStringUTF16(
                             IDS_DEEP_SCANNING_DIALOG_PRINT_SUCCESS_MESSAGE)
                       : l10n_util::GetStringFUTF16(
                             IDS_DEEP_SCANNING_DIALOG_CUSTOM_MESSAGE,
                             u"Custom message");
    }
    int files_count = file_scan() ? 1 : 0;
    return success()
               ? l10n_util::GetPluralStringFUTF16(
                     IDS_DEEP_SCANNING_DIALOG_SUCCESS_MESSAGE, files_count)
               : l10n_util::GetStringFUTF16(
                     IDS_DEEP_SCANNING_DIALOG_CUSTOM_MESSAGE,
                     u"Custom message");
  }
};

constexpr char kTestUrl[] = "https://google.com";

}  // namespace

IN_PROC_BROWSER_TEST_P(ContentAnalysisDialogBehaviorBrowserTest, Test) {
  base::ScopedAllowBlockingForTesting allow_blocking;

  // Setup policies to enable deep scanning, its UI and the responses to be
  // simulated.
  enterprise_connectors::test::SetAnalysisConnector(
      browser()->profile()->GetPrefs(), FILE_ATTACHED,
      kBlockingScansForDlpAndMalware);
  SetStatusCallbackResponse(
      safe_browsing::SimpleContentAnalysisResponseForTesting(
          dlp_success(), malware_success(), /*has_custom_rule_message=*/false));

  // Set up delegate test values.
  test::FakeContentAnalysisDelegate::SetResponseDelay(response_delay());
  SetUpDelegate();

  bool called = false;
  base::RunLoop run_loop;
  SetQuitClosure(run_loop.QuitClosure());

  ContentAnalysisDelegate::Data data;
  CreateFilesForTest({"foo.doc"}, {"content"}, &data);
  ASSERT_TRUE(ContentAnalysisDelegate::IsEnabled(
      browser()->profile(), GURL(kTestUrl), &data,
      enterprise_connectors::AnalysisConnector::FILE_ATTACHED));

  ContentAnalysisDelegate::CreateForWebContents(
      browser()->tab_strip_model()->GetActiveWebContents(), std::move(data),
      base::BindOnce(
          [](bool* called, const ContentAnalysisDelegate::Data& data,
             ContentAnalysisDelegate::Result& result) { *called = true; },
          &called),
      safe_browsing::DeepScanAccessPoint::UPLOAD);
  run_loop.Run();
  EXPECT_TRUE(called);
}

// The scan type controls if DLP, malware or both are enabled via policies. The
// dialog currently behaves identically in all 3 cases, so this parameter
// ensures this assumption is not broken by new code.
//
// The DLP/Malware success parameters determine how the response is populated,
// and therefore what the dialog should show.
//
// The three different delays test three cases:
// kNoDelay: The response is as fast as possible, and therefore the pending
//           UI is not shown (kNoDelay < GetInitialUIDelay).
// kSmallDelay: The response is not fast enough to prevent the pending UI from
//              showing, but fast enough that it hasn't been show long enough
//              (GetInitialDelay < kSmallDelay < GetMinimumPendingDialogTime).
// kNormalDelay: The response is slow enough that the pending UI is shown for
//               more than its minimum duration (GetMinimumPendingDialogTime <
//               kNormalDelay).
INSTANTIATE_TEST_SUITE_P(
    ,
    ContentAnalysisDialogBehaviorBrowserTest,
    testing::Combine(
        /*dlp_success*/ testing::Bool(),
        /*malware_success*/ testing::Bool(),
        /*response_delay*/
        testing::Values(kNoDelay, kSmallDelay, kNormalDelay)));

IN_PROC_BROWSER_TEST_F(ContentAnalysisDialogCancelPendingScanBrowserTest,
                       Test) {
  base::ScopedAllowBlockingForTesting allow_blocking;

  // Setup policies to enable deep scanning, its UI and the responses to be
  // simulated.
  enterprise_connectors::test::SetAnalysisConnector(
      browser()->profile()->GetPrefs(), FILE_ATTACHED, kBlockingScansForDlp);
  SetStatusCallbackResponse(
      safe_browsing::SimpleContentAnalysisResponseForTesting(
          /*dlp=*/true, /*malware=*/std::nullopt,
          /*has_custom_rule_message=*/false));

  // Set up delegate test values. An unresponsive delegate is set up to avoid
  // a race between the file responses and the "Cancel" button being clicked.
  test::FakeContentAnalysisDelegate::SetResponseDelay(kSmallDelay);
  SetUpUnresponsiveDelegate();

  bool called = false;
  base::RunLoop run_loop;
  SetQuitClosure(run_loop.QuitClosure());

  ContentAnalysisDelegate::Data data;
  CreateFilesForTest({"foo.doc", "bar.doc", "baz.doc"},
                     {"random", "file", "contents"}, &data);
  ASSERT_TRUE(ContentAnalysisDelegate::IsEnabled(
      browser()->profile(), GURL(kTestUrl), &data,
      enterprise_connectors::AnalysisConnector::FILE_ATTACHED));

  ContentAnalysisDelegate::CreateForWebContents(
      browser()->tab_strip_model()->GetActiveWebContents(), std::move(data),
      base::BindOnce(
          [](bool* called, const ContentAnalysisDelegate::Data& data,
             ContentAnalysisDelegate::Result& result) {
            for (bool paths_result : result.paths_results)
              ASSERT_FALSE(paths_result);
            *called = true;
          },
          &called),
      safe_browsing::DeepScanAccessPoint::UPLOAD);
  run_loop.Run();
  EXPECT_TRUE(called);

  ValidateMetrics();
}

IN_PROC_BROWSER_TEST_P(ContentAnalysisDialogWarningBrowserTest, Test) {
  base::ScopedAllowBlockingForTesting allow_blocking;

  // Setup policies.
  enterprise_connectors::test::SetAnalysisConnector(
      browser()->profile()->GetPrefs(), FILE_ATTACHED, kBlockingScansForDlp);

  // Setup the DLP warning response.
  enterprise_connectors::ContentAnalysisResponse response;
  auto* result = response.add_results();
  result->set_tag("dlp");
  result->set_status(
      enterprise_connectors::ContentAnalysisResponse::Result::SUCCESS);
  auto* rule = result->add_triggered_rules();
  rule->set_rule_name("warning_rule_name");
  rule->set_action(enterprise_connectors::TriggeredRule::WARN);
  SetStatusCallbackResponse(response);

  // Set up delegate.
  SetUpDelegate();

  bool called = false;
  base::RunLoop run_loop;
  SetQuitClosure(run_loop.QuitClosure());

  ContentAnalysisDelegate::Data data;
  data.text.emplace_back(text());
  data.text.emplace_back(text());
  CreateFilesForTest({"foo.doc", "bar.doc"}, {"file", "content"}, &data);
  ASSERT_TRUE(ContentAnalysisDelegate::IsEnabled(
      browser()->profile(), GURL(kTestUrl), &data,
      enterprise_connectors::AnalysisConnector::FILE_ATTACHED));

  ContentAnalysisDelegate::CreateForWebContents(
      browser()->tab_strip_model()->GetActiveWebContents(), std::move(data),
      base::BindOnce(
          [](bool* called, bool user_bypasses_warning,
             const ContentAnalysisDelegate::Data& data,
             ContentAnalysisDelegate::Result& result) {
            ASSERT_EQ(result.text_results.size(), 2u);
            ASSERT_EQ(result.text_results[0], user_bypasses_warning);
            ASSERT_EQ(result.text_results[1], user_bypasses_warning);
            ASSERT_EQ(result.paths_results.size(), 2u);
            ASSERT_EQ(result.paths_results[0], user_bypasses_warning);
            ASSERT_EQ(result.paths_results[1], user_bypasses_warning);
            *called = true;
          },
          &called, user_bypasses_warning()),
      safe_browsing::DeepScanAccessPoint::UPLOAD);
  run_loop.Run();
  EXPECT_TRUE(called);
}

INSTANTIATE_TEST_SUITE_P(,
                         ContentAnalysisDialogWarningBrowserTest,
                         /*user_bypasses_warning=*/testing::Bool());

IN_PROC_BROWSER_TEST_P(ContentAnalysisDialogAppearanceBrowserTest, Test) {
  base::ScopedAllowBlockingForTesting allow_blocking;

  // Setup policies to enable deep scanning, its UI and the responses to be
  // simulated.
  enterprise_connectors::test::SetAnalysisConnector(
      browser()->profile()->GetPrefs(), FILE_ATTACHED,
      kBlockingScansForDlpAndMalware);

  SetStatusCallbackResponse(
      safe_browsing::SimpleContentAnalysisResponseForTesting(
          success(), success(), /*has_custom_rule_message=*/false));

  // Set up delegate test values.
  test::FakeContentAnalysisDelegate::SetResponseDelay(kSmallDelay);
  SetUpDelegate();

  bool called = false;
  base::RunLoop run_loop;
  SetQuitClosure(run_loop.QuitClosure());

  ContentAnalysisDelegate::Data data;

  // Use a file path or text to validate the appearance of the dialog for both
  // types of scans.
  if (file_scan())
    CreateFilesForTest({"foo.doc"}, {"content"}, &data);
  else
    data.text.emplace_back(text());
  ASSERT_TRUE(ContentAnalysisDelegate::IsEnabled(
      browser()->profile(), GURL(kTestUrl), &data,
      enterprise_connectors::AnalysisConnector::FILE_ATTACHED));

  ContentAnalysisDelegate::CreateForWebContents(
      browser()->tab_strip_model()->GetActiveWebContents(), std::move(data),
      base::BindLambdaForTesting(
          [this, &called](const ContentAnalysisDelegate::Data& data,
                          ContentAnalysisDelegate::Result& result) {
            for (bool paths_result : result.paths_results)
              ASSERT_EQ(paths_result, success());
            called = true;
          }),
      access_point());
  run_loop.Run();
  EXPECT_TRUE(called);
}

INSTANTIATE_TEST_SUITE_P(
    ,
    ContentAnalysisDialogAppearanceBrowserTest,
    testing::Combine(
        /*file_scan=*/testing::Bool(),
        /*success=*/testing::Bool(),
        /*access_point=*/
        testing::Values(safe_browsing::DeepScanAccessPoint::UPLOAD,
                        safe_browsing::DeepScanAccessPoint::DRAG_AND_DROP,
                        safe_browsing::DeepScanAccessPoint::PASTE,
                        safe_browsing::DeepScanAccessPoint::PRINT),
        /*has_custom_rule_message=*/testing::Bool()));

IN_PROC_BROWSER_TEST_P(ContentAnalysisDialogCustomMessageBrowserTest, Test) {
  base::ScopedAllowBlockingForTesting allow_blocking;

  // Setup policies to enable deep scanning, its UI and the responses to be
  // simulated.
  enterprise_connectors::test::SetAnalysisConnector(
      browser()->profile()->GetPrefs(), FILE_ATTACHED,
      kBlockingScansForDlpAndMalwareWithCustomMessage);

  SetStatusCallbackResponse(
      safe_browsing::SimpleContentAnalysisResponseForTesting(
          success(), success(), has_custom_rule_message()));

  // Set up delegate test values.
  test::FakeContentAnalysisDelegate::SetResponseDelay(kSmallDelay);
  SetUpDelegate();

  bool called = false;
  base::RunLoop run_loop;
  SetQuitClosure(run_loop.QuitClosure());

  ContentAnalysisDelegate::Data data;

  // Use a file path or text to validate the appearance of the dialog for both
  // types of scans.
  if (file_scan()) {
    CreateFilesForTest({"foo.doc"}, {"content"}, &data);
  } else {
    data.text.emplace_back(text());
  }
  ASSERT_TRUE(ContentAnalysisDelegate::IsEnabled(
      browser()->profile(), GURL(kTestUrl), &data,
      enterprise_connectors::AnalysisConnector::FILE_ATTACHED));

  ContentAnalysisDelegate::CreateForWebContents(
      browser()->tab_strip_model()->GetActiveWebContents(), std::move(data),
      base::BindLambdaForTesting(
          [this, &called](const ContentAnalysisDelegate::Data& data,
                          ContentAnalysisDelegate::Result& result) {
            for (bool paths_result : result.paths_results) {
              ASSERT_EQ(paths_result, success());
            }
            called = true;
          }),
      access_point());
  run_loop.Run();
  EXPECT_TRUE(called);
}

INSTANTIATE_TEST_SUITE_P(
    ,
    ContentAnalysisDialogCustomMessageBrowserTest,
    testing::Combine(
        /*file_scan=*/testing::Bool(),
        /*success=*/testing::Bool(),
        /*access_point=*/
        testing::Values(safe_browsing::DeepScanAccessPoint::UPLOAD,
                        safe_browsing::DeepScanAccessPoint::DRAG_AND_DROP,
                        safe_browsing::DeepScanAccessPoint::PASTE,
                        safe_browsing::DeepScanAccessPoint::PRINT),
        /*has_custom_rule_message=*/testing::Bool()));

class ContentAnalysisDialogPlainTests : public InProcessBrowserTest {
 public:
  ContentAnalysisDialogPlainTests() {
    ContentAnalysisDialog::SetShowDialogDelayForTesting(kNoDelay);
  }

  void OpenCallback() { ++times_open_called_; }

  void DiscardCallback() { ++times_discard_called_; }

 protected:
  class MockDelegate : public ContentAnalysisDelegateBase {
   public:
    ~MockDelegate() override = default;
    void BypassWarnings(
        std::optional<std::u16string> user_justification) override {}
    void Cancel(bool warning) override {}

    std::optional<std::u16string> GetCustomMessage() const override {
      return std::nullopt;
    }

    std::optional<GURL> GetCustomLearnMoreUrl() const override {
      return std::nullopt;
    }

    std::optional<std::vector<std::pair<gfx::Range, GURL>>>
    GetCustomRuleMessageRanges() const override {
      return std::nullopt;
    }

    bool BypassRequiresJustification() const override {
      return bypass_requires_justification_;
    }

    std::u16string GetBypassJustificationLabel() const override {
      return u"MOCK_BYPASS_JUSTIFICATION_LABEL";
    }

    std::optional<std::u16string> OverrideCancelButtonText() const override {
      return std::nullopt;
    }

    void SetBypassRequiresJustification(bool value) {
      bypass_requires_justification_ = value;
    }

   private:
    bool bypass_requires_justification_ = false;
  };

  class MockCustomMessageDelegate : public ContentAnalysisDelegateBase {
   public:
    MockCustomMessageDelegate(const std::u16string& message, const GURL& url)
        : custom_message_(message), learn_more_url_(url) {}
    MockCustomMessageDelegate(
        const std::u16string& message,
        const std::vector<std::pair<gfx::Range, GURL>>& ranges)
        : custom_message_(message), custom_rule_message_ranges_(ranges) {}

    ~MockCustomMessageDelegate() override = default;

    void BypassWarnings(
        std::optional<std::u16string> user_justification) override {}
    void Cancel(bool warning) override {}

    std::optional<std::u16string> GetCustomMessage() const override {
      return custom_message_;
    }

    std::optional<GURL> GetCustomLearnMoreUrl() const override {
      return learn_more_url_;
    }

    std::optional<std::vector<std::pair<gfx::Range, GURL>>>
    GetCustomRuleMessageRanges() const override {
      return custom_rule_message_ranges_;
    }

    bool BypassRequiresJustification() const override { return false; }
    std::u16string GetBypassJustificationLabel() const override {
      return u"MOCK_BYPASS_JUSTIFICATION_LABEL";
    }

    std::optional<std::u16string> OverrideCancelButtonText() const override {
      return std::nullopt;
    }

   private:
    std::u16string custom_message_;
    GURL learn_more_url_;
    std::vector<std::pair<gfx::Range, GURL>> custom_rule_message_ranges_;
  };

  ContentAnalysisDialog* dialog() { return dialog_; }

  ContentAnalysisDialog* CreateContentAnalysisDialog(
      std::unique_ptr<ContentAnalysisDelegateBase> delegate,
      FinalContentAnalysisResult result = FinalContentAnalysisResult::SUCCESS) {
    // This ctor ends up calling into constrained_window to show itself, in a
    // way that relinquishes its ownership. Because of this, new it here and
    // let it be deleted by the constrained_window code.
    dialog_ = new ContentAnalysisDialog(
        std::move(delegate), true,
        browser()->tab_strip_model()->GetActiveWebContents(),
        safe_browsing::DeepScanAccessPoint::DOWNLOAD, 0, result);

    return dialog_;
  }

  int times_open_called_ = 0;
  int times_discard_called_ = 0;

 private:
  raw_ptr<ContentAnalysisDialog, DanglingUntriaged> dialog_;
};

IN_PROC_BROWSER_TEST_F(ContentAnalysisDialogPlainTests, TestCustomMessage) {
  enterprise_connectors::ContentAnalysisDialog::
      SetMinimumPendingDialogTimeForTesting(base::Milliseconds(0));

  std::unique_ptr<MockCustomMessageDelegate> delegate =
      std::make_unique<MockCustomMessageDelegate>(
          u"Test", GURL("https://www.example.com"));
  ContentAnalysisDialog* dialog = CreateContentAnalysisDialog(
      std::move(delegate), FinalContentAnalysisResult::SUCCESS);
  dialog->ShowResult(FinalContentAnalysisResult::WARNING);

  EXPECT_TRUE(dialog->IsDialogButtonEnabled(ui::mojom::DialogButton::kOk));
  EXPECT_EQ(dialog->GetMessageForTesting()->GetText(), u"Test");
}

IN_PROC_BROWSER_TEST_F(ContentAnalysisDialogPlainTests, TestCustomRuleMessage) {
  enterprise_connectors::ContentAnalysisDialog::
      SetMinimumPendingDialogTimeForTesting(base::Milliseconds(0));

  std::unique_ptr<MockCustomMessageDelegate> delegate =
      std::make_unique<MockCustomMessageDelegate>(
          u"Test", std::vector{std::pair{gfx::Range(0, 3),
                                         GURL("https://www.example.com")}});
  ContentAnalysisDialog* dialog = CreateContentAnalysisDialog(
      std::move(delegate), FinalContentAnalysisResult::SUCCESS);
  dialog->ShowResult(FinalContentAnalysisResult::WARNING);

  EXPECT_TRUE(dialog->IsDialogButtonEnabled(ui::mojom::DialogButton::kOk));
  EXPECT_EQ(dialog->GetMessageForTesting()->GetText(), u"Test");
}

IN_PROC_BROWSER_TEST_F(ContentAnalysisDialogPlainTests,
                       TestBypassJustification) {
  enterprise_connectors::ContentAnalysisDialog::
      SetMinimumPendingDialogTimeForTesting(base::Milliseconds(0));

  std::unique_ptr<MockDelegate> delegate = std::make_unique<MockDelegate>();
  delegate->SetBypassRequiresJustification(true);
  ContentAnalysisDialog* dialog = CreateContentAnalysisDialog(
      std::move(delegate), FinalContentAnalysisResult::SUCCESS);
  dialog->ShowResult(FinalContentAnalysisResult::WARNING);

  EXPECT_FALSE(dialog->IsDialogButtonEnabled(ui::mojom::DialogButton::kOk));
  dialog->GetBypassJustificationTextareaForTesting()->InsertOrReplaceText(
      u"test");
  EXPECT_TRUE(dialog->IsDialogButtonEnabled(ui::mojom::DialogButton::kOk));
}

IN_PROC_BROWSER_TEST_F(ContentAnalysisDialogPlainTests,
                       TestBypassJustificationTooLongDisablesBypassButton) {
  enterprise_connectors::ContentAnalysisDialog::
      SetMinimumPendingDialogTimeForTesting(base::Milliseconds(0));

  std::unique_ptr<MockDelegate> delegate = std::make_unique<MockDelegate>();
  delegate->SetBypassRequiresJustification(true);
  ContentAnalysisDialog* dialog = CreateContentAnalysisDialog(
      std::move(delegate), FinalContentAnalysisResult::SUCCESS);
  dialog->ShowResult(FinalContentAnalysisResult::WARNING);

  EXPECT_FALSE(dialog->IsDialogButtonEnabled(ui::mojom::DialogButton::kOk));
  dialog->GetBypassJustificationTextareaForTesting()->InsertOrReplaceText(
      u"This is a very long string. In fact, it is over two hundred characters "
      u"long because that is the maximum length of a bypass justification that "
      u"can be entered by a user. When the justification is this long, the "
      u"user will not be able to submit it. The maximum length just happens to "
      u"be the same as a popular bird-based service's character limit.");
  EXPECT_FALSE(dialog->IsDialogButtonEnabled(ui::mojom::DialogButton::kOk));
}

IN_PROC_BROWSER_TEST_F(ContentAnalysisDialogPlainTests,
                       TestOpenInDefaultPendingState) {
  ContentAnalysisDialog* dialog =
      CreateContentAnalysisDialog(std::make_unique<MockDelegate>());
  EXPECT_TRUE(dialog->GetSideIconSpinnerForTesting());
  EXPECT_EQ(
      dialog->GetMessageForTesting()->GetText(),
      u"Checking this data with your organization's security policies...");
}

IN_PROC_BROWSER_TEST_F(ContentAnalysisDialogPlainTests,
                       TestOpenInWarningState) {
  ContentAnalysisDialog* dialog = CreateContentAnalysisDialog(
      std::make_unique<MockDelegate>(), FinalContentAnalysisResult::WARNING);
  EXPECT_EQ(nullptr, dialog->GetSideIconSpinnerForTesting());
  EXPECT_EQ(dialog->GetMessageForTesting()->GetText(),
            u"This data or your device doesn’t meet some of your organization’s"
            u" security policies. Check with your admin on what needs to be "
            u"fixed.");
}

IN_PROC_BROWSER_TEST_F(ContentAnalysisDialogPlainTests, TestOpenInBlockState) {
  ContentAnalysisDialog* dialog = CreateContentAnalysisDialog(
      std::make_unique<MockDelegate>(), FinalContentAnalysisResult::FAILURE);
  EXPECT_EQ(nullptr, dialog->GetSideIconSpinnerForTesting());
  EXPECT_EQ(dialog->GetMessageForTesting()->GetText(),
            u"This data or your device doesn’t meet some of your organization’s"
            u" security policies. Check with your admin on what needs to be "
            u"fixed.");
}

IN_PROC_BROWSER_TEST_F(ContentAnalysisDialogPlainTests,
                       TestOpenInFailClosedState) {
  ContentAnalysisDialog* dialog =
      CreateContentAnalysisDialog(std::make_unique<MockDelegate>(),
                                  FinalContentAnalysisResult::FAIL_CLOSED);
  EXPECT_EQ(nullptr, dialog->GetSideIconSpinnerForTesting());
  EXPECT_EQ(dialog->GetMessageForTesting()->GetText(),
            u"Scan failed. This action is blocked by your administrator.");
}

IN_PROC_BROWSER_TEST_F(ContentAnalysisDialogPlainTests,
                       TestOpenInLargeFilesState) {
  ContentAnalysisDialog* dialog =
      CreateContentAnalysisDialog(std::make_unique<MockDelegate>(),
                                  FinalContentAnalysisResult::LARGE_FILES);
  EXPECT_EQ(nullptr, dialog->GetSideIconSpinnerForTesting());
  EXPECT_EQ(dialog->GetMessageForTesting()->GetText(),
            u"Some of these files are too big for a security check. You can "
            u"upload files up to 50 MB.");
}

IN_PROC_BROWSER_TEST_F(ContentAnalysisDialogPlainTests,
                       TestOpenInEncryptedFilesState) {
  ContentAnalysisDialog* dialog =
      CreateContentAnalysisDialog(std::make_unique<MockDelegate>(),
                                  FinalContentAnalysisResult::ENCRYPTED_FILES);
  EXPECT_EQ(nullptr, dialog->GetSideIconSpinnerForTesting());
  EXPECT_EQ(dialog->GetMessageForTesting()->GetText(),
            u"Some of these files are encrypted. Ask their owner to decrypt.");
}

IN_PROC_BROWSER_TEST_F(ContentAnalysisDialogPlainTests,
                       TestWithDownloadsDelegateBypassWarning) {
  download::MockDownloadItem mock_download_item;
  ContentAnalysisDialog* dialog = CreateContentAnalysisDialog(
      std::make_unique<ContentAnalysisDownloadsDelegate>(
          u"", u"", GURL(), true,
          base::BindOnce(&ContentAnalysisDialogPlainTests::OpenCallback,
                         base::Unretained(this)),
          base::BindOnce(&ContentAnalysisDialogPlainTests::DiscardCallback,
                         base::Unretained(this)),
          &mock_download_item,
          ContentAnalysisResponse::Result::TriggeredRule::CustomRuleMessage()),
      FinalContentAnalysisResult::WARNING);

  EXPECT_EQ(0, times_open_called_);
  EXPECT_EQ(0, times_discard_called_);

  std::u16string test_user_justification = u"user's justification for bypass";
  dialog->GetBypassJustificationTextareaForTesting()->InsertOrReplaceText(
      test_user_justification);
  dialog->AcceptDialog();
  EXPECT_EQ(1, times_open_called_);
  EXPECT_EQ(0, times_discard_called_);
  enterprise_connectors::ScanResult* stored_result =
      static_cast<enterprise_connectors::ScanResult*>(
          mock_download_item.GetUserData(
              enterprise_connectors::ScanResult::kKey));
  ASSERT_TRUE(stored_result);
  EXPECT_EQ(stored_result->user_justification, test_user_justification);
}

IN_PROC_BROWSER_TEST_F(ContentAnalysisDialogPlainTests,
                       TestWithDownloadsDelegateDiscardWarning) {
  ContentAnalysisDialog* dialog = CreateContentAnalysisDialog(
      std::make_unique<ContentAnalysisDownloadsDelegate>(
          u"", u"", GURL(), false,
          base::BindOnce(&ContentAnalysisDialogPlainTests::OpenCallback,
                         base::Unretained(this)),
          base::BindOnce(&ContentAnalysisDialogPlainTests::DiscardCallback,
                         base::Unretained(this)),
          nullptr,
          ContentAnalysisResponse::Result::TriggeredRule::CustomRuleMessage()),
      FinalContentAnalysisResult::WARNING);

  EXPECT_EQ(0, times_open_called_);
  EXPECT_EQ(0, times_discard_called_);

  dialog->CancelDialog();
  EXPECT_EQ(0, times_open_called_);
  EXPECT_EQ(1, times_discard_called_);
}

IN_PROC_BROWSER_TEST_F(ContentAnalysisDialogPlainTests,
                       TestWithDownloadsDelegateDiscardBlock) {
  ContentAnalysisDialog* dialog = CreateContentAnalysisDialog(
      std::make_unique<ContentAnalysisDownloadsDelegate>(
          u"", u"", GURL(), false,
          base::BindOnce(&ContentAnalysisDialogPlainTests::OpenCallback,
                         base::Unretained(this)),
          base::BindOnce(&ContentAnalysisDialogPlainTests::DiscardCallback,
                         base::Unretained(this)),
          nullptr,
          ContentAnalysisResponse::Result::TriggeredRule::CustomRuleMessage()),
      FinalContentAnalysisResult::FAILURE);

  EXPECT_EQ(0, times_open_called_);
  EXPECT_EQ(0, times_discard_called_);

  dialog->CancelDialog();
  EXPECT_EQ(0, times_open_called_);
  EXPECT_EQ(1, times_discard_called_);
}

IN_PROC_BROWSER_TEST_F(ContentAnalysisDialogPlainTests,
                       BypassJustificationLabelAndTextareaAccessibility) {
  enterprise_connectors::ContentAnalysisDialog::
      SetMinimumPendingDialogTimeForTesting(base::Milliseconds(0));
  std::unique_ptr<MockDelegate> delegate = std::make_unique<MockDelegate>();
  delegate->SetBypassRequiresJustification(true);
  ContentAnalysisDialog* dialog = CreateContentAnalysisDialog(
      std::move(delegate), FinalContentAnalysisResult::SUCCESS);
  dialog->ShowResult(FinalContentAnalysisResult::WARNING);

  // We need the label and its `AXNodeData` to verify that the textarea's name
  // matches the name of the label, and that the textarea's labelledby id is
  // the accessible id of the label.
  auto* label = dialog->GetBypassJustificationLabelForTesting();
  EXPECT_TRUE(label);
  ui::AXNodeData label_data;
  label->GetViewAccessibility().GetAccessibleNodeData(&label_data);

  auto* textarea = dialog->GetBypassJustificationTextareaForTesting();
  EXPECT_TRUE(textarea);
  ui::AXNodeData textarea_data;
  textarea->GetViewAccessibility().GetAccessibleNodeData(&textarea_data);
  EXPECT_EQ(textarea_data.role, ax::mojom::Role::kTextField);
  EXPECT_EQ(textarea->GetViewAccessibility().GetCachedRole(),
            ax::mojom::Role::kTextField);
  EXPECT_EQ(
      textarea_data.GetString16Attribute(ax::mojom::StringAttribute::kName),
      label->GetViewAccessibility().GetCachedName());
  EXPECT_EQ(textarea_data.GetNameFrom(), ax::mojom::NameFrom::kRelatedElement);
  EXPECT_EQ(textarea_data.GetIntListAttribute(
                ax::mojom::IntListAttribute::kLabelledbyIds)[0],
            label_data.id);
  EXPECT_TRUE(textarea_data.HasState(ax::mojom::State::kEditable));
  EXPECT_FALSE(textarea_data.HasState(ax::mojom::State::kProtected));
  EXPECT_EQ(textarea_data.GetDefaultActionVerb(),
            ax::mojom::DefaultActionVerb::kActivate);
}

class ContentAnalysisDialogUiTest
    : public DialogBrowserTest,
      public testing::WithParamInterface<std::tuple<bool, bool, bool>> {
 public:
  ContentAnalysisDialogUiTest() {
    ContentAnalysisDialog::SetShowDialogDelayForTesting(kNoDelay);
  }

  ContentAnalysisDialogUiTest(const ContentAnalysisDialogUiTest&) = delete;
  ContentAnalysisDialogUiTest& operator=(const ContentAnalysisDialogUiTest&) =
      delete;
  ~ContentAnalysisDialogUiTest() override = default;

  bool custom_message_provided() const { return std::get<0>(GetParam()); }
  bool custom_url_provided() const { return std::get<1>(GetParam()); }
  bool bypass_justification_enabled() const { return std::get<2>(GetParam()); }

  std::u16string get_custom_message() {
    return custom_message_provided() ? u"Admin comment" : u"";
  }

  GURL get_custom_url() {
    return custom_url_provided() ? GURL("http://learn-more-url.com/") : GURL();
  }

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    auto delegate = std::make_unique<ContentAnalysisDownloadsDelegate>(
        u"File Name", get_custom_message(), get_custom_url(),
        bypass_justification_enabled(), base::DoNothing(), base::DoNothing(),
        nullptr,
        ContentAnalysisResponse::Result::TriggeredRule::CustomRuleMessage());

    // This ctor ends up calling into constrained_window to show itself, in a
    // way that relinquishes its ownership. Because of this, new it here and
    // let it be deleted by the constrained_window code.
    new ContentAnalysisDialog(
        std::move(delegate), true,
        browser()->tab_strip_model()->GetActiveWebContents(),
        safe_browsing::DeepScanAccessPoint::DOWNLOAD, 1,
        FinalContentAnalysisResult::WARNING);
  }
};

IN_PROC_BROWSER_TEST_P(ContentAnalysisDialogUiTest, InvokeUi_default) {
  ShowAndVerifyUi();
}

INSTANTIATE_TEST_SUITE_P(,
                         ContentAnalysisDialogUiTest,
                         testing::Combine(
                             /*custom_message_exists*/ testing::Bool(),
                             /*custom_url_exists*/ testing::Bool(),
                             /*bypass_justification_enabled*/ testing::Bool()));

class ContentAnalysisDialogCustomRuleMessageUiTest
    : public ContentAnalysisDialogUiTest {
 public:
  ContentAnalysisDialogCustomRuleMessageUiTest() {
    ContentAnalysisDialog::SetShowDialogDelayForTesting(kNoDelay);
  }

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    ContentAnalysisResponse::Result::TriggeredRule::CustomRuleMessage
        custom_rule_message = CreateSampleCustomRuleMessage(
            u"Admin rule message", get_custom_url().GetContent());
    auto delegate = std::make_unique<ContentAnalysisDownloadsDelegate>(
        u"File Name", get_custom_message(), get_custom_url(),
        bypass_justification_enabled(), base::DoNothing(), base::DoNothing(),
        nullptr, custom_rule_message);

    // This ctor ends up calling into constrained_window to show itself, in a
    // way that relinquishes its ownership. Because of this, new it here and
    // let it be deleted by the constrained_window code.
    new ContentAnalysisDialog(
        std::move(delegate), true,
        browser()->tab_strip_model()->GetActiveWebContents(),
        safe_browsing::DeepScanAccessPoint::DOWNLOAD, 1,
        FinalContentAnalysisResult::WARNING);
  }

 private:
  base::test::ScopedFeatureList scoped_features;
};

IN_PROC_BROWSER_TEST_P(ContentAnalysisDialogCustomRuleMessageUiTest,
                       InvokeUi_default) {
  ShowAndVerifyUi();
}

INSTANTIATE_TEST_SUITE_P(,
                         ContentAnalysisDialogCustomRuleMessageUiTest,
                         testing::Combine(
                             /*custom_message_exists*/ testing::Bool(),
                             /*custom_url_exists*/ testing::Bool(),
                             /*bypass_justification_enabled*/ testing::Bool()));

class ContentAnalysisDialogDownloadObserverTest
    : public test::DeepScanningBrowserTestBase,
      public ContentAnalysisDialog::TestObserver {
 public:
  ContentAnalysisDialogDownloadObserverTest() {
    ContentAnalysisDialog::SetObserverForTesting(this);
  }

  void ConstructorCalled(ContentAnalysisDialog* dialog,
                         base::TimeTicks timestamp) override {
    ctor_called_ = true;
  }

  void ViewsFirstShown(ContentAnalysisDialog* dialog,
                       base::TimeTicks timestamp) override {
    std::move(views_first_shown_closure_).Run();
  }

  void DestructorCalled(ContentAnalysisDialog* dialog) override {
    std::move(dtor_called_closure_).Run();
  }

 protected:
  bool ctor_called_ = false;
  base::OnceClosure views_first_shown_closure_;
  base::OnceClosure dtor_called_closure_;
};

IN_PROC_BROWSER_TEST_F(ContentAnalysisDialogDownloadObserverTest,
                       DownloadOpened) {
  download::MockDownloadItem mock_download_item;
  base::RunLoop show_run_loop;
  views_first_shown_closure_ = show_run_loop.QuitClosure();

  new ContentAnalysisDialog(
      std::make_unique<ContentAnalysisDownloadsDelegate>(
          u"", u"", GURL(), true,
          /* open_file_callback */ base::DoNothing(),
          /* discard_callback */ base::DoNothing(), &mock_download_item,
          ContentAnalysisResponse::Result::TriggeredRule::CustomRuleMessage()),
      true, browser()->tab_strip_model()->GetActiveWebContents(),
      safe_browsing::DeepScanAccessPoint::DOWNLOAD, /* file_count */ 1,
      FinalContentAnalysisResult::WARNING, &mock_download_item);

  show_run_loop.Run();

  EXPECT_TRUE(ctor_called_);

  base::RunLoop dtor_run_loop;
  dtor_called_closure_ = dtor_run_loop.QuitClosure();

  mock_download_item.NotifyObserversDownloadOpened();
  dtor_run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(ContentAnalysisDialogDownloadObserverTest,
                       DownloadUpdated) {
  download::MockDownloadItem mock_download_item;
  base::RunLoop show_run_loop;
  views_first_shown_closure_ = show_run_loop.QuitClosure();

  new ContentAnalysisDialog(
      std::make_unique<ContentAnalysisDownloadsDelegate>(
          u"", u"", GURL(), true,
          /* open_file_callback */ base::DoNothing(),
          /* discard_callback */ base::DoNothing(), &mock_download_item,
          ContentAnalysisResponse::Result::TriggeredRule::CustomRuleMessage()),
      true, browser()->tab_strip_model()->GetActiveWebContents(),
      safe_browsing::DeepScanAccessPoint::DOWNLOAD, /* file_count */ 1,
      FinalContentAnalysisResult::WARNING, &mock_download_item);

  show_run_loop.Run();

  EXPECT_TRUE(ctor_called_);

  base::RunLoop dtor_run_loop;
  dtor_called_closure_ = dtor_run_loop.QuitClosure();

  // Randomly updating the download should not close the dialog.
  EXPECT_CALL(mock_download_item, GetDangerType())
      .WillOnce(testing::Return(download::DOWNLOAD_DANGER_TYPE_ASYNC_SCANNING));
  mock_download_item.NotifyObserversDownloadUpdated();
  EXPECT_FALSE(dtor_run_loop.AnyQuitCalled());

  // Updating the download with a user validation results in the dialog closing.
  EXPECT_CALL(mock_download_item, GetDangerType())
      .WillOnce(testing::Return(download::DOWNLOAD_DANGER_TYPE_USER_VALIDATED));
  mock_download_item.NotifyObserversDownloadUpdated();
  dtor_run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(ContentAnalysisDialogDownloadObserverTest,
                       DownloadDestroyed) {
  auto mock_download_item = std::make_unique<download::MockDownloadItem>();
  base::RunLoop show_run_loop;
  views_first_shown_closure_ = show_run_loop.QuitClosure();

  new ContentAnalysisDialog(
      std::make_unique<ContentAnalysisDownloadsDelegate>(
          u"", u"", GURL(), true,
          /* open_file_callback */ base::DoNothing(),
          /* discard_callback */ base::DoNothing(), mock_download_item.get(),
          ContentAnalysisResponse::Result::TriggeredRule::CustomRuleMessage()),
      true, browser()->tab_strip_model()->GetActiveWebContents(),
      safe_browsing::DeepScanAccessPoint::DOWNLOAD, /* file_count */ 1,
      FinalContentAnalysisResult::WARNING, mock_download_item.get());

  show_run_loop.Run();

  EXPECT_TRUE(ctor_called_);

  base::RunLoop dtor_run_loop;
  dtor_called_closure_ = dtor_run_loop.QuitClosure();

  mock_download_item.reset();
  dtor_run_loop.Run();
}

}  // namespace enterprise_connectors
