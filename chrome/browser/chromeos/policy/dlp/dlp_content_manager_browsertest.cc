// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/dlp/dlp_content_manager.h"

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/test_future.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager_factory.h"
#include "chrome/browser/chromeos/policy/dlp/test/dlp_content_manager_test_helper.h"
#include "chrome/browser/chromeos/policy/dlp/test/mock_dlp_rules_manager.h"
#include "chrome/browser/enterprise/data_controls/dlp_reporting_manager.h"
#include "chrome/browser/enterprise/data_controls/dlp_reporting_manager_test_helper.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/policy/messaging_layer/public/report_client_test_util.h"
#include "chrome/browser/printing/print_test_utils.h"
#include "chrome/browser/printing/print_view_manager.h"
#include "chrome/browser/printing/print_view_manager_common.h"
#include "chrome/browser/printing/test_print_preview_dialog_cloned_observer.h"
#include "chrome/browser/printing/test_print_view_manager_for_request_preview.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/enterprise/data_controls/core/browser/dlp_histogram_helper.h"
#include "components/enterprise/data_controls/core/browser/dlp_policy_event.pb.h"
#include "components/reporting/client/report_queue_impl.h"
#include "components/reporting/storage/test_storage_module.h"
#include "components/reporting/util/test_support_callbacks.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;

namespace policy {

namespace {

const DlpContentRestrictionSet kEmptyRestrictionSet;
const DlpContentRestrictionSet kScreenshotRestricted(
    DlpContentRestriction::kScreenshot,
    DlpRulesManager::Level::kBlock);
const DlpContentRestrictionSet kScreenshotWarned(
    DlpContentRestriction::kScreenshot,
    DlpRulesManager::Level::kWarn);
const DlpContentRestrictionSet kScreenshotReported(
    DlpContentRestriction::kScreenshot,
    DlpRulesManager::Level::kReport);
const DlpContentRestrictionSet kPrintAllowed(DlpContentRestriction::kPrint,
                                             DlpRulesManager::Level::kAllow);
const DlpContentRestrictionSet kPrintRestricted(DlpContentRestriction::kPrint,
                                                DlpRulesManager::Level::kBlock);
const DlpContentRestrictionSet kPrintReported(DlpContentRestriction::kPrint,
                                              DlpRulesManager::Level::kReport);
const DlpContentRestrictionSet kPrintWarned(DlpContentRestriction::kPrint,
                                            DlpRulesManager::Level::kWarn);
const DlpContentRestrictionSet kScreenShareWarned(
    DlpContentRestriction::kScreenShare,
    DlpRulesManager::Level::kWarn);

constexpr char kPrintBlockedNotificationId[] = "print_dlp_blocked";

constexpr char kExampleUrl[] = "https://example.com";
constexpr char kLabel[] = "label";
const std::u16string kApplicationTitle = u"example.com";

constexpr char kRuleName[] = "rule #1";
constexpr char kRuleId[] = "testid1";
const DlpRulesManager::RuleMetadata kRuleMetadata(kRuleName, kRuleId);
}  // namespace

class DlpContentManagerBrowserTest : public InProcessBrowserTest {
 public:
  DlpContentManagerBrowserTest() = default;
  ~DlpContentManagerBrowserTest() override = default;

  std::unique_ptr<KeyedService> SetDlpRulesManager(
      content::BrowserContext* context) {
    auto dlp_rules_manager = std::make_unique<MockDlpRulesManager>(
        Profile::FromBrowserContext(context));
    mock_rules_manager_ = dlp_rules_manager.get();
    return dlp_rules_manager;
  }

  void SetUpOnMainThread() override {
    // Instantiate |DlpContentManagerTestHelper| after main thread has been
    // set up cause |DlpReportingManager| needs a sequenced task runner handle
    // to set up the report queue.
    helper_ = std::make_unique<DlpContentManagerTestHelper>();
  }

  void TearDownOnMainThread() override { helper_.reset(); }

  // Sets up mock rules manager.
  void SetupDlpRulesManager() {
    DlpRulesManagerFactory::GetInstance()->SetTestingFactory(
        browser()->profile(),
        base::BindRepeating(&DlpContentManagerBrowserTest::SetDlpRulesManager,
                            base::Unretained(this)));
    ASSERT_TRUE(DlpRulesManagerFactory::GetForPrimaryProfile());

    EXPECT_CALL(*mock_rules_manager_, GetSourceUrlPattern)
        .WillRepeatedly(testing::DoAll(testing::SetArgPointee<3>(kRuleMetadata),
                                       testing::Return("example.com")));
    EXPECT_CALL(*mock_rules_manager_, IsRestricted)
        .WillRepeatedly(testing::Return(DlpRulesManager::Level::kAllow));
  }

  void SetupReporting() {
    SetupDlpRulesManager();
    // Set up mock report queue.
    data_controls::SetReportQueueForReportingManager(
        helper_->GetReportingManager(), events_,
        base::SequencedTaskRunner::GetCurrentDefault());
  }

  void CheckEvents(DlpRulesManager::Restriction restriction,
                   DlpRulesManager::Level level,
                   size_t count) {
    EXPECT_EQ(events_.size(), count);
    for (size_t i = 0; i < count; ++i) {
      EXPECT_THAT(events_[i], data_controls::IsDlpPolicyEvent(
                                  data_controls::CreateDlpPolicyEvent(
                                      GURL(kExampleUrl).spec(), restriction,
                                      kRuleName, kRuleId, level)));
    }
  }

 protected:
  std::unique_ptr<::reporting::ReportingClient::TestEnvironment>
      test_reporting_;
  std::unique_ptr<DlpContentManagerTestHelper> helper_;
  base::HistogramTester histogram_tester_;
  raw_ptr<MockDlpRulesManager, DanglingUntriaged> mock_rules_manager_;
  std::vector<DlpPolicyEvent> events_;
};

IN_PROC_BROWSER_TEST_F(DlpContentManagerBrowserTest, PrintingNotRestricted) {
  // Set up mock report queue and mock rules manager.
  SetupReporting();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(kExampleUrl)));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  NotificationDisplayServiceTester display_service_tester(browser()->profile());

  base::MockCallback<WarningCallback> cb;
  EXPECT_CALL(cb, Run(true)).Times(1);

  helper_->GetContentManager()->CheckPrintingRestriction(
      web_contents, web_contents->GetPrimaryMainFrame()->GetGlobalId(),
      cb.Get());

  // Start printing and check that there is no notification when printing is not
  // restricted.
  printing::test::StartPrint(web_contents);
  EXPECT_FALSE(
      display_service_tester.GetNotification(kPrintBlockedNotificationId));
  CheckEvents(DlpRulesManager::Restriction::kPrinting,
              DlpRulesManager::Level::kBlock, 0u);
}

IN_PROC_BROWSER_TEST_F(DlpContentManagerBrowserTest, ScreenshotsRestricted) {
  SetupReporting();
  DlpContentManager* manager = helper_->GetContentManager();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(kExampleUrl)));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  EXPECT_FALSE(manager->IsScreenshotApiRestricted(web_contents));
  histogram_tester_.ExpectBucketCount(
      data_controls::GetDlpHistogramPrefix() +
          data_controls::dlp::kScreenshotBlockedUMA,
      true, 0);
  histogram_tester_.ExpectBucketCount(
      data_controls::GetDlpHistogramPrefix() +
          data_controls::dlp::kScreenshotBlockedUMA,
      false, 1);
  CheckEvents(DlpRulesManager::Restriction::kScreenshot,
              DlpRulesManager::Level::kBlock, 0u);

  helper_->ChangeConfidentiality(web_contents, kScreenshotRestricted);
  EXPECT_TRUE(manager->IsScreenshotApiRestricted(web_contents));
  histogram_tester_.ExpectBucketCount(
      data_controls::GetDlpHistogramPrefix() +
          data_controls::dlp::kScreenshotBlockedUMA,
      true, 1);
  histogram_tester_.ExpectBucketCount(
      data_controls::GetDlpHistogramPrefix() +
          data_controls::dlp::kScreenshotBlockedUMA,
      false, 1);
  CheckEvents(DlpRulesManager::Restriction::kScreenshot,
              DlpRulesManager::Level::kBlock, 1u);

  web_contents->WasHidden();
  helper_->ChangeVisibility(web_contents);
  EXPECT_TRUE(manager->IsScreenshotApiRestricted(web_contents));
  histogram_tester_.ExpectBucketCount(
      data_controls::GetDlpHistogramPrefix() +
          data_controls::dlp::kScreenshotBlockedUMA,
      true, 2);
  histogram_tester_.ExpectBucketCount(
      data_controls::GetDlpHistogramPrefix() +
          data_controls::dlp::kScreenshotBlockedUMA,
      false, 1);
  CheckEvents(DlpRulesManager::Restriction::kScreenshot,
              DlpRulesManager::Level::kBlock, 2u);

  web_contents->WasShown();
  helper_->ChangeVisibility(web_contents);
  EXPECT_TRUE(manager->IsScreenshotApiRestricted(web_contents));
  histogram_tester_.ExpectBucketCount(
      data_controls::GetDlpHistogramPrefix() +
          data_controls::dlp::kScreenshotBlockedUMA,
      true, 3);
  histogram_tester_.ExpectBucketCount(
      data_controls::GetDlpHistogramPrefix() +
          data_controls::dlp::kScreenshotBlockedUMA,
      false, 1);
  CheckEvents(DlpRulesManager::Restriction::kScreenshot,
              DlpRulesManager::Level::kBlock, 3u);

  helper_->DestroyWebContents(web_contents);
  histogram_tester_.ExpectBucketCount(
      data_controls::GetDlpHistogramPrefix() +
          data_controls::dlp::kScreenshotBlockedUMA,
      true, 3);
  histogram_tester_.ExpectBucketCount(
      data_controls::GetDlpHistogramPrefix() +
          data_controls::dlp::kScreenshotBlockedUMA,
      false, 1);
  CheckEvents(DlpRulesManager::Restriction::kScreenshot,
              DlpRulesManager::Level::kBlock, 3u);
}

IN_PROC_BROWSER_TEST_F(DlpContentManagerBrowserTest, ScreenshotsWarned) {
  SetupReporting();
  DlpContentManager* manager = helper_->GetContentManager();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(kExampleUrl)));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  EXPECT_FALSE(manager->IsScreenshotApiRestricted(web_contents));
  CheckEvents(DlpRulesManager::Restriction::kScreenshot,
              DlpRulesManager::Level::kWarn, 0u);

  helper_->ChangeConfidentiality(web_contents, kScreenshotWarned);
  EXPECT_TRUE(manager->IsScreenshotApiRestricted(web_contents));
  CheckEvents(DlpRulesManager::Restriction::kScreenshot,
              DlpRulesManager::Level::kWarn, 1u);

  web_contents->WasHidden();
  helper_->ChangeVisibility(web_contents);
  EXPECT_TRUE(manager->IsScreenshotApiRestricted(web_contents));
  CheckEvents(DlpRulesManager::Restriction::kScreenshot,
              DlpRulesManager::Level::kWarn, 2u);

  web_contents->WasShown();
  helper_->ChangeVisibility(web_contents);
  EXPECT_TRUE(manager->IsScreenshotApiRestricted(web_contents));
  CheckEvents(DlpRulesManager::Restriction::kScreenshot,
              DlpRulesManager::Level::kWarn, 3u);

  helper_->DestroyWebContents(web_contents);
  CheckEvents(DlpRulesManager::Restriction::kScreenshot,
              DlpRulesManager::Level::kWarn, 3u);
}

IN_PROC_BROWSER_TEST_F(DlpContentManagerBrowserTest, ScreenshotsReported) {
  SetupReporting();
  DlpContentManager* manager = helper_->GetContentManager();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(kExampleUrl)));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  EXPECT_FALSE(manager->IsScreenshotApiRestricted(web_contents));
  CheckEvents(DlpRulesManager::Restriction::kScreenshot,
              DlpRulesManager::Level::kReport, 0u);

  helper_->ChangeConfidentiality(web_contents, kScreenshotReported);
  EXPECT_FALSE(manager->IsScreenshotApiRestricted(web_contents));
  CheckEvents(DlpRulesManager::Restriction::kScreenshot,
              DlpRulesManager::Level::kReport, 1u);

  web_contents->WasHidden();
  helper_->ChangeVisibility(web_contents);
  EXPECT_FALSE(manager->IsScreenshotApiRestricted(web_contents));
  CheckEvents(DlpRulesManager::Restriction::kScreenshot,
              DlpRulesManager::Level::kReport, 2u);

  web_contents->WasShown();
  helper_->ChangeVisibility(web_contents);
  EXPECT_FALSE(manager->IsScreenshotApiRestricted(web_contents));
  CheckEvents(DlpRulesManager::Restriction::kScreenshot,
              DlpRulesManager::Level::kReport, 3u);

  helper_->DestroyWebContents(web_contents);
  histogram_tester_.ExpectBucketCount(
      data_controls::GetDlpHistogramPrefix() +
          data_controls::dlp::kScreenshotBlockedUMA,
      true, 0);
  histogram_tester_.ExpectBucketCount(
      data_controls::GetDlpHistogramPrefix() +
          data_controls::dlp::kScreenshotBlockedUMA,
      false, 4);
  CheckEvents(DlpRulesManager::Restriction::kScreenshot,
              DlpRulesManager::Level::kReport, 3u);
}

class DlpContentManagerReportingBrowserTest
    : public DlpContentManagerBrowserTest {
 public:
  void SetUpOnMainThread() override {
    DlpContentManagerBrowserTest::SetUpOnMainThread();
    content::WebContents* first_tab =
        browser()->tab_strip_model()->GetActiveWebContents();
    ASSERT_TRUE(first_tab);

    // Open a new tab so |cloned_tab_observer_| can see it and create a
    // TestPrintViewManagerForRequestPreview for it before the real
    // PrintViewManager gets created.
    // Since TestPrintViewManagerForRequestPreview is created with
    // PrintViewManager::UserDataKey(), the real PrintViewManager is not created
    // and TestPrintViewManagerForRequestPreview gets mojo messages for the
    // purposes of this test.
    cloned_tab_observer_ =
        std::make_unique<printing::TestPrintPreviewDialogClonedObserver>(
            first_tab);
    chrome::DuplicateTab(browser());
  }

  void TearDownOnMainThread() override {
    DlpContentManagerBrowserTest::TearDownOnMainThread();
    cloned_tab_observer_.reset();
    test_reporting_.reset();
    storage_module_.reset();
  }

  // Sets up real report queue together with TestStorageModule
  void SetupReportQueue() {
    const ::reporting::Destination destination_ =
        ::reporting::Destination::UPLOAD_EVENTS;

    storage_module_ =
        base::MakeRefCounted<::reporting::test::TestStorageModule>();

    test_reporting_ =
        ::reporting::ReportingClient::TestEnvironment::CreateWithStorageModule(
            test_storage_module());

    policy_check_callback_ =
        base::BindRepeating(&testing::MockFunction<::reporting::Status()>::Call,
                            base::Unretained(&mocked_policy_check_));

    ON_CALL(mocked_policy_check_, Call())
        .WillByDefault(testing::Return(::reporting::Status::StatusOK()));

    auto config_result = ::reporting::ReportQueueConfiguration::Create(
                             {.event_type = ::reporting::EventType::kDevice,
                              .destination = destination_})
                             .SetPolicyCheckCallback(policy_check_callback_)
                             .Build();

    ASSERT_TRUE(config_result.has_value());

    // Create a report queue with the test storage module, and attach it
    // to an actual speculative report queue so we can override the one used in
    // |DlpReportingManager| by default.
    ::reporting::test::TestEvent<
        ::reporting::StatusOr<std::unique_ptr<::reporting::ReportQueue>>>
        report_queue_event;
    ::reporting::ReportQueueImpl::Create(std::move(config_result.value()),
                                         storage_module_,
                                         report_queue_event.cb());
    auto report_queue_result = report_queue_event.result();

    ASSERT_TRUE(report_queue_result.has_value());

    auto speculative_report_queue =
        ::reporting::SpeculativeReportQueueImpl::Create(
            {.destination = destination_});
    auto attach_queue_cb =
        speculative_report_queue->PrepareToAttachActualQueue();

    helper_->GetReportingManager()->SetReportQueueForTest(
        std::move(speculative_report_queue));
    std::move(attach_queue_cb).Run(std::move(report_queue_result.value()));

    // Wait until the speculative report queue is initialized with the stubbed
    // report queue posted to its internal task runner
    base::ThreadPoolInstance::Get()->FlushForTesting();
  }

  ::reporting::test::TestStorageModule* test_storage_module() const {
    ::reporting::test::TestStorageModule* test_storage_module =
        google::protobuf::down_cast<::reporting::test::TestStorageModule*>(
            storage_module_.get());
    DCHECK(test_storage_module);
    return test_storage_module;
  }

  void CheckRecord(DlpPolicyEvent expectedEvent, ::reporting::Record record) {
    DlpPolicyEvent event;
    EXPECT_TRUE(event.ParseFromString(record.data()));
    EXPECT_EQ(event.source().url(), GURL(kExampleUrl).spec());
    EXPECT_EQ(event.triggered_rule_name(), kRuleName);
    EXPECT_EQ(event.triggered_rule_id(), kRuleId);
    EXPECT_THAT(event, data_controls::IsDlpPolicyEvent(expectedEvent));
  }

  // Sets an action to execute when an event arrives to the report queue storage
  // module.
  void SetAddRecordCheck(DlpPolicyEvent expectedEvent, int times) {
    // TODO(1290312): Change to [=, this] when chrome code base is updated to
    // C++20.
    EXPECT_CALL(*test_storage_module(), AddRecord)
        .Times(times)
        .WillRepeatedly(testing::WithArgs<1, 2>(testing::Invoke(
            [=, this](::reporting::Record record,
                      base::OnceCallback<void(::reporting::Status)> callback) {
              content::GetUIThreadTaskRunner({})->PostTask(
                  FROM_HERE,
                  base::BindOnce(
                      &DlpContentManagerReportingBrowserTest::CheckRecord,
                      base::Unretained(this), std::move(expectedEvent),
                      std::move(record)));
              std::move(callback).Run(::reporting::Status::StatusOK());
            })));
  }

  // Start printing and wait for the end of
  // printing::PrintViewManager::RequestPrintPreview(). StartPrint() is an
  // asynchronous function, which initializes mojo communication with a renderer
  // process. We need to wait for the DLP restriction check in
  // RequestPrintPreview(), which happens after the renderer process
  // communicates back to the browser process.
  void StartPrint(
      printing::TestPrintViewManagerForRequestPreview* print_manager,
      content::WebContents* web_contents) {
    base::test::TestFuture<void> future;
    print_manager->set_quit_closure(future.GetCallback());
    printing::test::StartPrint(web_contents);
    EXPECT_TRUE(future.Wait());
  }

 protected:
  // Helper class to enable asserting that printing was accepted or rejected.
  class MockPrintManager
      : public printing::TestPrintViewManagerForRequestPreview {
   public:
    MOCK_METHOD(void, PrintPreviewAllowedForTesting, (), (override));
    MOCK_METHOD(void, PrintPreviewRejectedForTesting, (), (override));

    static void CreateForWebContents(content::WebContents* web_contents) {
      web_contents->SetUserData(
          PrintViewManager::UserDataKey(),
          std::make_unique<MockPrintManager>(web_contents));
    }

    static MockPrintManager* FromWebContents(
        content::WebContents* web_contents) {
      return static_cast<MockPrintManager*>(
          printing::TestPrintViewManagerForRequestPreview::FromWebContents(
              web_contents));
    }

    explicit MockPrintManager(content::WebContents* web_contents)
        : printing::TestPrintViewManagerForRequestPreview(web_contents) {}
    ~MockPrintManager() override = default;
  };

  MockPrintManager* GetPrintManager(content::WebContents* web_contents) {
    MockPrintManager::CreateForWebContents(web_contents);
    return MockPrintManager::FromWebContents(web_contents);
  }

  scoped_refptr<::reporting::StorageModuleInterface> storage_module_;
  testing::NiceMock<testing::MockFunction<::reporting::Status()>>
      mocked_policy_check_;
  ::reporting::ReportQueueConfiguration::PolicyCheckCallback
      policy_check_callback_;
  std::unique_ptr<printing::TestPrintPreviewDialogClonedObserver>
      cloned_tab_observer_;
};

IN_PROC_BROWSER_TEST_F(DlpContentManagerReportingBrowserTest,
                       PrintingRestricted) {
  // Set up mock rules manager.
  SetupDlpRulesManager();
  // Set up real report queue.
  SetupReportQueue();
  // Sets an action to execute when an event arrives to a storage module.
  SetAddRecordCheck(
      CreateDlpPolicyEvent(GURL(kExampleUrl).spec(),
                           DlpRulesManager::Restriction::kPrinting, kRuleName,
                           kRuleId, DlpRulesManager::Level::kBlock),
      /*times=*/2);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(kExampleUrl)));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  NotificationDisplayServiceTester display_service_tester(browser()->profile());

  // Set up the mocks for directly calling CheckPrintingRestriction().
  base::MockCallback<WarningCallback> cb;
  testing::InSequence s;
  EXPECT_CALL(cb, Run(true)).Times(1);
  EXPECT_CALL(cb, Run(false)).Times(1);

  content::GlobalRenderFrameHostId rfh_id =
      web_contents->GetPrimaryMainFrame()->GetGlobalId();
  // Printing should first be allowed.
  helper_->GetContentManager()->CheckPrintingRestriction(web_contents, rfh_id,
                                                         cb.Get());

  // Set up printing restriction.
  helper_->ChangeConfidentiality(web_contents, kPrintRestricted);
  helper_->GetContentManager()->CheckPrintingRestriction(web_contents, rfh_id,
                                                         cb.Get());

  // Setup the mock for the printing manager to invoke
  // CheckPrintingRestriction() indirectly.
  MockPrintManager* print_manager = GetPrintManager(web_contents);
  EXPECT_CALL(*print_manager, PrintPreviewAllowedForTesting).Times(0);
  EXPECT_CALL(*print_manager, PrintPreviewRejectedForTesting).Times(1);
  StartPrint(print_manager, web_contents);

  // Check for notification about printing restriction.
  EXPECT_TRUE(
      display_service_tester.GetNotification(kPrintBlockedNotificationId));
}

IN_PROC_BROWSER_TEST_F(DlpContentManagerReportingBrowserTest,
                       PrintingReported) {
  SetupDlpRulesManager();
  SetupReportQueue();
  SetAddRecordCheck(
      CreateDlpPolicyEvent(GURL(kExampleUrl).spec(),
                           DlpRulesManager::Restriction::kPrinting, kRuleName,
                           kRuleId, DlpRulesManager::Level::kReport),
      /*times=*/2);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(kExampleUrl)));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  NotificationDisplayServiceTester display_service_tester(browser()->profile());

  // Set up printing restriction.
  helper_->ChangeConfidentiality(web_contents, kPrintReported);
  // Printing should be reported, but still allowed whether we call
  // CheckPrintingRestriction() directly or indirectly.
  base::MockCallback<WarningCallback> cb;
  EXPECT_CALL(cb, Run(true)).Times(1);
  helper_->GetContentManager()->CheckPrintingRestriction(
      web_contents, web_contents->GetPrimaryMainFrame()->GetGlobalId(),
      cb.Get());

  MockPrintManager* print_manager = GetPrintManager(web_contents);
  EXPECT_CALL(*print_manager, PrintPreviewAllowedForTesting).Times(1);
  EXPECT_CALL(*print_manager, PrintPreviewRejectedForTesting).Times(0);
  StartPrint(print_manager, web_contents);

  EXPECT_FALSE(
      display_service_tester.GetNotification(kPrintBlockedNotificationId));
}

IN_PROC_BROWSER_TEST_F(DlpContentManagerReportingBrowserTest, PrintingWarned) {
  SetupDlpRulesManager();
  SetupReportQueue();
  NotificationDisplayServiceTester display_service_tester(browser()->profile());

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(kExampleUrl)));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Set up printing restriction.
  helper_->ChangeConfidentiality(web_contents, kPrintWarned);

  SetAddRecordCheck(
      CreateDlpPolicyEvent(GURL(kExampleUrl).spec(),
                           DlpRulesManager::Restriction::kPrinting, kRuleName,
                           kRuleId, DlpRulesManager::Level::kWarn),
      /*times=*/1);

  MockPrintManager* print_manager = GetPrintManager(web_contents);
  testing::InSequence s;
  EXPECT_CALL(*print_manager, PrintPreviewRejectedForTesting()).Times(1);

  StartPrint(print_manager, web_contents);
  EXPECT_EQ(helper_->ActiveWarningDialogsCount(), 1);
  // Hit Esc to "Cancel".
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_ESCAPE, false,
                                              false, false, false));
  EXPECT_EQ(helper_->ActiveWarningDialogsCount(), 0);
  // There should be no notification about printing restriction.
  EXPECT_FALSE(
      display_service_tester.GetNotification(kPrintBlockedNotificationId));
  EXPECT_TRUE(testing::Mock::VerifyAndClearExpectations(test_storage_module()));

  SetAddRecordCheck(
      CreateDlpPolicyEvent(GURL(kExampleUrl).spec(),
                           DlpRulesManager::Restriction::kPrinting, kRuleName,
                           kRuleId, DlpRulesManager::Level::kWarn),
      /*times=*/1);

  // Attempt to print again.
  StartPrint(print_manager, web_contents);
  EXPECT_EQ(helper_->ActiveWarningDialogsCount(), 1);
  EXPECT_TRUE(testing::Mock::VerifyAndClearExpectations(test_storage_module()));

  SetAddRecordCheck(
      CreateDlpPolicyWarningProceededEvent(
          GURL(kExampleUrl).spec(), DlpRulesManager::Restriction::kPrinting,
          kRuleName, kRuleId),
      /*times=*/1);
  EXPECT_CALL(*print_manager, PrintPreviewAllowedForTesting()).Times(1);

  // Hit Enter to "Print anyway".
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_RETURN, false,
                                              false, false, false));
  EXPECT_EQ(helper_->ActiveWarningDialogsCount(), 0);
}

IN_PROC_BROWSER_TEST_F(DlpContentManagerReportingBrowserTest,
                       TabShareWarnedDuringAllowed) {
  SetupReporting();
  NotificationDisplayServiceTester display_service_tester(browser()->profile());

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(kExampleUrl)));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  const content::DesktopMediaID media_id(
      content::DesktopMediaID::TYPE_WEB_CONTENTS,
      content::DesktopMediaID::kNullId,
      content::WebContentsMediaCaptureId(
          web_contents->GetPrimaryMainFrame()->GetProcess()->GetID(),
          web_contents->GetPrimaryMainFrame()->GetRoutingID()));

  DlpContentManager* manager = helper_->GetContentManager();
  base::MockCallback<content::MediaStreamUI::StateChangeCallback>
      state_change_cb;
  base::MockCallback<base::RepeatingClosure> stop_cb;
  base::MockCallback<content::MediaStreamUI::SourceCallback> source_cb;
  // Explicitly specify that the stop callback should never be invoked.
  EXPECT_CALL(stop_cb, Run()).Times(0);
  testing::InSequence s;
  EXPECT_CALL(state_change_cb,
              Run(testing::_, blink::mojom::MediaStreamStateChange::PAUSE))
      .Times(1);
  EXPECT_CALL(source_cb, Run).Times(1);
  // Although the share should be paused and resumed, DLP will only call
  // state_change_cb_ once to pause it. When it's supposed to be resumed, it
  // will call source_cb only first and resume only the new stream once
  // notified.
  EXPECT_CALL(state_change_cb,
              Run(testing::_, blink::mojom::MediaStreamStateChange::PLAY))
      .Times(0);

  manager->OnScreenShareStarted(kLabel, {media_id}, kApplicationTitle,
                                stop_cb.Get(), state_change_cb.Get(),
                                source_cb.Get());

  helper_->ChangeConfidentiality(web_contents, kScreenShareWarned);
  EXPECT_EQ(helper_->ActiveWarningDialogsCount(), 1);
  CheckEvents(DlpRulesManager::Restriction::kScreenShare,
              DlpRulesManager::Level::kWarn, 1u);

  // Hit Enter to "Share anyway".
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_RETURN, /*control=*/false,
      /*shift=*/false, /*alt=*/false, /*command=*/false));
  EXPECT_EQ(helper_->ActiveWarningDialogsCount(), 0);
  EXPECT_EQ(events_.size(), 2u);
  EXPECT_THAT(
      events_[1],
      data_controls::IsDlpPolicyEvent(
          data_controls::CreateDlpPolicyWarningProceededEvent(
              GURL(kExampleUrl).spec(),
              DlpRulesManager::Restriction::kScreenShare, kRuleName, kRuleId)));

  // The contents should already be cached as allowed by the user, so this
  // should not trigger a new warning.
  helper_->ChangeConfidentiality(web_contents, kScreenShareWarned);
  EXPECT_EQ(helper_->ActiveWarningDialogsCount(), 0);
  EXPECT_EQ(events_.size(), 2u);
}

}  // namespace policy
