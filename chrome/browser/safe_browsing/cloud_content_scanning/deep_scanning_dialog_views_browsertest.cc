// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/utf_string_conversions.h"
#include "base/test/bind_test_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/deep_scanning_browsertest_base.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/deep_scanning_dialog_views.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/deep_scanning_test_utils.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/deep_scanning_utils.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/fake_deep_scanning_dialog_delegate.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/enterprise/common/proto/connectors.pb.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/test/browser_test.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/throbber.h"
#include "ui/views/test/ax_event_counter.h"

namespace safe_browsing {

namespace {

constexpr base::TimeDelta kNoDelay = base::TimeDelta::FromSeconds(0);
constexpr base::TimeDelta kSmallDelay = base::TimeDelta::FromMilliseconds(300);
constexpr base::TimeDelta kNormalDelay = base::TimeDelta::FromMilliseconds(500);

base::string16 text() {
  return base::UTF8ToUTF16(std::string(100, 'a'));
}

// Tests the behavior of the dialog in the following ways:
// - It shows the appropriate buttons depending on its state.
// - It transitions from states in the correct order.
// - It respects time constraints (minimum shown time, initial delay, timeout)
// - It is always destroyed, therefore |quit_closure_| is called in the dtor
//   observer.
// - It sends accessibility events correctly.
class DeepScanningDialogViewsBehaviorBrowserTest
    : public DeepScanningBrowserTestBase,
      public DeepScanningDialogViews::TestObserver,
      public testing::WithParamInterface<
          std::tuple<bool, bool, base::TimeDelta>> {
 public:
  DeepScanningDialogViewsBehaviorBrowserTest()
      : ax_event_counter_(views::AXEventManager::Get()) {
    DeepScanningDialogViews::SetObserverForTesting(this);

    expected_scan_result_ = dlp_success() && malware_success();
  }

  void ConstructorCalled(DeepScanningDialogViews* views,
                         base::TimeTicks timestamp) override {
    ctor_called_timestamp_ = timestamp;
    dialog_ = views;

    // The scan should be pending when constructed.
    EXPECT_TRUE(dialog_->is_pending());

    // The dialog should only be constructed once.
    EXPECT_FALSE(ctor_called_);
    ctor_called_ = true;
  }

  void ViewsFirstShown(DeepScanningDialogViews* views,
                       base::TimeTicks timestamp) override {
    DCHECK_EQ(views, dialog_);
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
    EXPECT_EQ(dialog_->GetDialogButtons(), ui::DIALOG_BUTTON_CANCEL);

    // Record the number of AX events until now to check if the text update adds
    // one later.
    ax_events_count_when_first_shown_ =
        ax_event_counter_.GetCount(ax::mojom::Event::kAlert);

    // The dialog should only be shown once some time after being constructed.
    EXPECT_TRUE(ctor_called_);
    EXPECT_FALSE(views_first_shown_);
    views_first_shown_ = true;
  }

  void DialogUpdated(
      DeepScanningDialogViews* views,
      DeepScanningDialogDelegate::DeepScanningFinalResult result) override {
    DCHECK_EQ(views, dialog_);
    dialog_updated_timestamp_ = base::TimeTicks::Now();

    // The dialog should not be updated if the failure was shown immediately.
    EXPECT_TRUE(pending_shown_);

    // The dialog should only be updated after an initial delay.
    base::TimeDelta delay = dialog_updated_timestamp_ - first_shown_timestamp_;
    EXPECT_GE(delay, DeepScanningDialogViews::GetMinimumPendingDialogTime());

    // The dialog can only be updated to the success or failure case.
    EXPECT_TRUE(dialog_->is_result());
    bool is_success =
        result == DeepScanningDialogDelegate::DeepScanningFinalResult::SUCCESS;
    EXPECT_EQ(dialog_->is_success(), is_success);
    EXPECT_EQ(dialog_->is_success(), expected_scan_result_);

    // The dialog's buttons should be Cancel in the fail case and nothing in the
    // success case.
    ui::DialogButton expected_buttons = dialog_->is_success()
                                            ? ui::DIALOG_BUTTON_NONE
                                            : ui::DIALOG_BUTTON_CANCEL;
    EXPECT_EQ(expected_buttons, dialog_->GetDialogButtons());

    // The dialog should only be updated once some time after being shown.
    EXPECT_TRUE(views_first_shown_);
    EXPECT_FALSE(dialog_updated_);

    // TODO(crbug/1131565): Re-enable this for Mac.
#if !defined(OS_MAC)
    // The dialog being updated implies an accessibility alert is sent.
    EXPECT_EQ(ax_events_count_when_first_shown_ + 1,
              ax_event_counter_.GetCount(ax::mojom::Event::kAlert));
#endif

    dialog_updated_ = true;
  }

  void DestructorCalled(DeepScanningDialogViews* views) override {
    dtor_called_timestamp_ = base::TimeTicks::Now();

    EXPECT_TRUE(views);
    EXPECT_EQ(dialog_, views);
    EXPECT_EQ(dialog_->is_success(), expected_scan_result_);

    if (views_first_shown_) {
      // Ensure the dialog update only occurred if the pending state was shown.
      EXPECT_EQ(pending_shown_, dialog_updated_);

      // Ensure the success UI timed out properly.
      EXPECT_TRUE(dialog_->is_result());
      if (dialog_->is_success()) {
        // The success dialog should stay open for some time.
        base::TimeDelta delay =
            dtor_called_timestamp_ - dialog_updated_timestamp_;
        EXPECT_GE(delay, DeepScanningDialogViews::GetSuccessDialogTimeout());

        EXPECT_EQ(ui::DIALOG_BUTTON_NONE, dialog_->GetDialogButtons());
      } else {
        EXPECT_EQ(ui::DIALOG_BUTTON_CANCEL, dialog_->GetDialogButtons());
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
  DeepScanningDialogViews* dialog_;

  base::TimeTicks ctor_called_timestamp_;
  base::TimeTicks first_shown_timestamp_;
  base::TimeTicks dialog_updated_timestamp_;
  base::TimeTicks dtor_called_timestamp_;

  bool pending_shown_ = false;
  bool ctor_called_ = false;
  bool views_first_shown_ = false;
  bool dialog_updated_ = false;

  bool expected_scan_result_;

  int ax_events_count_when_first_shown_ = 0;
  views::test::AXEventCounter ax_event_counter_;
};

// Tests the behavior of the dialog in the following ways:
// - It closes when the "Cancel" button is clicked.
// - It returns a negative verdict on the scanned content.
// - The "CancelledByUser" metrics are recorded.
class DeepScanningDialogViewsCancelPendingScanBrowserTest
    : public DeepScanningBrowserTestBase,
      public DeepScanningDialogViews::TestObserver {
 public:
  DeepScanningDialogViewsCancelPendingScanBrowserTest() {
    DeepScanningDialogViews::SetObserverForTesting(this);
  }

  void ViewsFirstShown(DeepScanningDialogViews* views,
                       base::TimeTicks timestamp) override {
    // Simulate the user clicking "Cancel" after the dialog is first shown.
    views->CancelDialog();
  }

  void DestructorCalled(DeepScanningDialogViews* views) override {
    // The test is over once the views are destroyed.
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
class DeepScanningDialogViewsWarningBrowserTest
    : public DeepScanningBrowserTestBase,
      public DeepScanningDialogViews::TestObserver,
      public testing::WithParamInterface<bool> {
 public:
  DeepScanningDialogViewsWarningBrowserTest() {
    DeepScanningDialogViews::SetObserverForTesting(this);
  }

  void ViewsFirstShown(DeepScanningDialogViews* views,
                       base::TimeTicks timestamp) override {
    // The dialog is first shown in the pending state.
    ASSERT_TRUE(views->is_pending());

    ASSERT_EQ(views->GetDialogButtons(), ui::DIALOG_BUTTON_CANCEL);
  }

  void DialogUpdated(
      DeepScanningDialogViews* views,
      DeepScanningDialogDelegate::DeepScanningFinalResult result) override {
    ASSERT_TRUE(views->is_warning());

    // The dialog's buttons should be Ok and Cancel.
    ASSERT_EQ(ui::DIALOG_BUTTON_OK | ui::DIALOG_BUTTON_CANCEL,
              views->GetDialogButtons());

    SimulateClickAndEndTest(views);
  }

  void SimulateClickAndEndTest(DeepScanningDialogViews* views) {
    if (user_bypasses_warning())
      views->AcceptDialog();
    else
      views->CancelDialog();

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
class DeepScanningDialogViewsAppearanceBrowserTest
    : public DeepScanningBrowserTestBase,
      public DeepScanningDialogViews::TestObserver,
      public testing::WithParamInterface<
          std::tuple<bool, bool, DeepScanAccessPoint>> {
 public:
  DeepScanningDialogViewsAppearanceBrowserTest() {
    DeepScanningDialogViews::SetObserverForTesting(this);
  }

  void ViewsFirstShown(DeepScanningDialogViews* views,
                       base::TimeTicks timestamp) override {
    // The dialog initially shows the pending message for the appropriate access
    // point and scan type.
    base::string16 pending_message = views->GetMessageForTesting()->GetText();
    base::string16 expected_message = l10n_util::GetPluralStringFUTF16(
        IDS_DEEP_SCANNING_DIALOG_UPLOAD_PENDING_MESSAGE, file_scan() ? 1 : 0);
    ASSERT_EQ(pending_message, expected_message);

    // The top image is the pending one corresponding to the access point.
    const gfx::ImageSkia& actual_image =
        views->GetTopImageForTesting()->GetImage();
    int expected_image_id = 0;
    switch (access_point()) {
      case DeepScanAccessPoint::DRAG_AND_DROP:
        expected_image_id =
            file_scan() ? IDR_UPLOAD_SCANNING : IDR_PASTE_SCANNING;
        break;
      case DeepScanAccessPoint::UPLOAD:
        expected_image_id = IDR_UPLOAD_SCANNING;
        break;
      case DeepScanAccessPoint::PASTE:
        expected_image_id = IDR_PASTE_SCANNING;
        break;
      case DeepScanAccessPoint::DOWNLOAD:
        NOTREACHED();
    }
    gfx::ImageSkia* expected_image =
        ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
            expected_image_id);
    ASSERT_TRUE(expected_image->BackedBySameObjectAs(actual_image));

    // The spinner should be present.
    ASSERT_TRUE(views->GetSideIconSpinnerForTesting());
  }

  void DialogUpdated(
      DeepScanningDialogViews* views,
      DeepScanningDialogDelegate::DeepScanningFinalResult result) override {
    // The dialog shows the failure or success message for the appropriate
    // access point and scan type.
    base::string16 final_message = views->GetMessageForTesting()->GetText();
    int files_count = file_scan() ? 1 : 0;
    base::string16 expected_message =
        success()
            ? l10n_util::GetPluralStringFUTF16(
                  IDS_DEEP_SCANNING_DIALOG_SUCCESS_MESSAGE, files_count)
            : l10n_util::GetPluralStringFUTF16(
                  IDS_DEEP_SCANNING_DIALOG_UPLOAD_FAILURE_MESSAGE, files_count);
    ASSERT_EQ(final_message, expected_message);

    // The top image is the failure/success one corresponding to the access
    // point and scan type.
    const gfx::ImageSkia& actual_image =
        views->GetTopImageForTesting()->GetImage();
    int expected_image_id = 0;
    switch (access_point()) {
      case DeepScanAccessPoint::DRAG_AND_DROP:
        expected_image_id =
            file_scan() ? success() ? IDR_UPLOAD_SUCCESS : IDR_UPLOAD_VIOLATION
                        : success() ? IDR_PASTE_SUCCESS : IDR_PASTE_VIOLATION;
        break;
      case DeepScanAccessPoint::UPLOAD:
        expected_image_id =
            success() ? IDR_UPLOAD_SUCCESS : IDR_UPLOAD_VIOLATION;
        break;
      case DeepScanAccessPoint::PASTE:
        expected_image_id = success() ? IDR_PASTE_SUCCESS : IDR_PASTE_VIOLATION;
        break;
      case DeepScanAccessPoint::DOWNLOAD:
        NOTREACHED();
    }
    gfx::ImageSkia* expected_image =
        ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
            expected_image_id);
    ASSERT_TRUE(expected_image->BackedBySameObjectAs(actual_image));

    // The spinner should not be present in the final result since nothing is
    // pending.
    ASSERT_FALSE(views->GetSideIconSpinnerForTesting());
  }

  void DestructorCalled(DeepScanningDialogViews* views) override {
    // End the test once the dialog gets destroyed.
    CallQuitClosure();
  }

  bool file_scan() const { return std::get<0>(GetParam()); }

  bool success() const { return std::get<1>(GetParam()); }

  DeepScanAccessPoint access_point() const { return std::get<2>(GetParam()); }
};

constexpr char kTestUrl[] = "https://google.com";

}  // namespace

IN_PROC_BROWSER_TEST_P(DeepScanningDialogViewsBehaviorBrowserTest, Test) {
  base::ScopedAllowBlockingForTesting allow_blocking;

  // Setup policies to enable deep scanning, its UI and the responses to be
  // simulated.
  SetDlpPolicyForConnectors(CHECK_UPLOADS_AND_DOWNLOADS);
  SetMalwarePolicyForConnectors(SEND_UPLOADS_AND_DOWNLOADS);
  AddUrlsToCheckForMalwareOfUploadsForConnectors({"*"});
  SetStatusCallbackResponse(SimpleContentAnalysisResponseForTesting(
      dlp_success(), malware_success()));

  // Always set this policy so the UI is shown.
  SetDelayDeliveryUntilVerdictPolicyForConnectors(DELAY_UPLOADS);

  // Set up delegate test values.
  FakeDeepScanningDialogDelegate::SetResponseDelay(response_delay());
  SetUpDelegate();

  bool called = false;
  base::RunLoop run_loop;
  SetQuitClosure(run_loop.QuitClosure());

  DeepScanningDialogDelegate::Data data;
  CreateFilesForTest({"foo.doc"}, {"content"}, &data);
  ASSERT_TRUE(DeepScanningDialogDelegate::IsEnabled(
      browser()->profile(), GURL(kTestUrl), &data,
      enterprise_connectors::AnalysisConnector::FILE_ATTACHED));

  DeepScanningDialogDelegate::ShowForWebContents(
      browser()->tab_strip_model()->GetActiveWebContents(), std::move(data),
      base::BindOnce(
          [](bool* called, const DeepScanningDialogDelegate::Data& data,
             const DeepScanningDialogDelegate::Result& result) {
            *called = true;
          },
          &called),
      DeepScanAccessPoint::UPLOAD);
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
    DeepScanningDialogViewsBehaviorBrowserTest,
    testing::Combine(
        /*dlp_success*/ testing::Bool(),
        /*malware_success*/ testing::Bool(),
        /*response_delay*/
        testing::Values(kNoDelay, kSmallDelay, kNormalDelay)));

IN_PROC_BROWSER_TEST_F(DeepScanningDialogViewsCancelPendingScanBrowserTest,
                       Test) {
  base::ScopedAllowBlockingForTesting allow_blocking;

  // Setup policies to enable deep scanning, its UI and the responses to be
  // simulated.
  SetDlpPolicyForConnectors(CHECK_UPLOADS);
  SetStatusCallbackResponse(SimpleContentAnalysisResponseForTesting(
      /*dlp=*/true, /*malware=*/base::nullopt));

  // Always set this policy so the UI is shown.
  SetDelayDeliveryUntilVerdictPolicyForConnectors(DELAY_UPLOADS);

  // Set up delegate test values. An unresponsive delegate is set up to avoid
  // a race between the file responses and the "Cancel" button being clicked.
  FakeDeepScanningDialogDelegate::SetResponseDelay(kSmallDelay);
  SetUpUnresponsiveDelegate();

  bool called = false;
  base::RunLoop run_loop;
  SetQuitClosure(run_loop.QuitClosure());

  DeepScanningDialogDelegate::Data data;
  CreateFilesForTest({"foo.doc", "bar.doc", "baz.doc"},
                     {"random", "file", "contents"}, &data);
  ASSERT_TRUE(DeepScanningDialogDelegate::IsEnabled(
      browser()->profile(), GURL(kTestUrl), &data,
      enterprise_connectors::AnalysisConnector::FILE_ATTACHED));

  DeepScanningDialogDelegate::ShowForWebContents(
      browser()->tab_strip_model()->GetActiveWebContents(), std::move(data),
      base::BindOnce(
          [](bool* called, const DeepScanningDialogDelegate::Data& data,
             const DeepScanningDialogDelegate::Result& result) {
            for (bool result : result.paths_results)
              ASSERT_FALSE(result);
            *called = true;
          },
          &called),
      DeepScanAccessPoint::UPLOAD);
  run_loop.Run();
  EXPECT_TRUE(called);

  ValidateMetrics();
}

IN_PROC_BROWSER_TEST_P(DeepScanningDialogViewsWarningBrowserTest, Test) {
  base::ScopedAllowBlockingForTesting allow_blocking;

  // Setup policies.
  SetDlpPolicyForConnectors(CHECK_UPLOADS);
  SetDelayDeliveryUntilVerdictPolicyForConnectors(DELAY_UPLOADS);

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

  DeepScanningDialogDelegate::Data data;
  data.text.emplace_back(text());
  data.text.emplace_back(text());
  CreateFilesForTest({"foo.doc", "bar.doc"}, {"file", "content"}, &data);
  ASSERT_TRUE(DeepScanningDialogDelegate::IsEnabled(
      browser()->profile(), GURL(kTestUrl), &data,
      enterprise_connectors::AnalysisConnector::FILE_ATTACHED));

  DeepScanningDialogDelegate::ShowForWebContents(
      browser()->tab_strip_model()->GetActiveWebContents(), std::move(data),
      base::BindOnce(
          [](bool* called, bool user_bypasses_warning,
             const DeepScanningDialogDelegate::Data& data,
             const DeepScanningDialogDelegate::Result& result) {
            ASSERT_EQ(result.text_results.size(), 2u);
            ASSERT_EQ(result.text_results[0], user_bypasses_warning);
            ASSERT_EQ(result.text_results[1], user_bypasses_warning);
            ASSERT_EQ(result.paths_results.size(), 2u);
            ASSERT_EQ(result.paths_results[0], user_bypasses_warning);
            ASSERT_EQ(result.paths_results[1], user_bypasses_warning);
            *called = true;
          },
          &called, user_bypasses_warning()),
      DeepScanAccessPoint::UPLOAD);
  run_loop.Run();
  EXPECT_TRUE(called);
}

INSTANTIATE_TEST_SUITE_P(,
                         DeepScanningDialogViewsWarningBrowserTest,
                         /*user_bypasses_warning=*/testing::Bool());

IN_PROC_BROWSER_TEST_P(DeepScanningDialogViewsAppearanceBrowserTest, Test) {
  base::ScopedAllowBlockingForTesting allow_blocking;

  // Setup policies to enable deep scanning, its UI and the responses to be
  // simulated.
  SetDlpPolicyForConnectors(CHECK_UPLOADS);
  SetMalwarePolicyForConnectors(SEND_UPLOADS);

  SetStatusCallbackResponse(
      SimpleContentAnalysisResponseForTesting(success(), success()));

  // Always set this policy so the UI is shown.
  SetDelayDeliveryUntilVerdictPolicyForConnectors(DELAY_UPLOADS);

  // Set up delegate test values.
  FakeDeepScanningDialogDelegate::SetResponseDelay(kSmallDelay);
  SetUpDelegate();

  bool called = false;
  base::RunLoop run_loop;
  SetQuitClosure(run_loop.QuitClosure());

  DeepScanningDialogDelegate::Data data;

  // Use a file path or text to validate the appearance of the dialog for both
  // types of scans.
  if (file_scan())
    CreateFilesForTest({"foo.doc"}, {"content"}, &data);
  else
    data.text.emplace_back(text());
  ASSERT_TRUE(DeepScanningDialogDelegate::IsEnabled(
      browser()->profile(), GURL(kTestUrl), &data,
      enterprise_connectors::AnalysisConnector::FILE_ATTACHED));

  DeepScanningDialogDelegate::ShowForWebContents(
      browser()->tab_strip_model()->GetActiveWebContents(), std::move(data),
      base::BindLambdaForTesting(
          [this, &called](const DeepScanningDialogDelegate::Data& data,
                          const DeepScanningDialogDelegate::Result& result) {
            for (bool result : result.paths_results)
              ASSERT_EQ(result, success());
            called = true;
          }),
      access_point());
  run_loop.Run();
  EXPECT_TRUE(called);
}

INSTANTIATE_TEST_SUITE_P(,
                         DeepScanningDialogViewsAppearanceBrowserTest,
                         testing::Combine(
                             /*file_scan=*/testing::Bool(),
                             /*success=*/testing::Bool(),
                             /*access_point=*/
                             testing::Values(DeepScanAccessPoint::UPLOAD,
                                             DeepScanAccessPoint::DRAG_AND_DROP,
                                             DeepScanAccessPoint::PASTE)));

}  // namespace safe_browsing
