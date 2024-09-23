// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/dlp/data_transfer_dlp_controller.h"

#include <memory>
#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/strings/strcat.h"
#include "base/task/thread_pool.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/types/optional_util.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_files_controller.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager.h"
#include "chrome/browser/chromeos/policy/dlp/test/mock_dlp_rules_manager.h"
#include "chrome/browser/enterprise/data_controls/dlp_reporting_manager.h"
#include "chrome/browser/enterprise/data_controls/dlp_reporting_manager_test_helper.h"
#include "chrome/browser/policy/messaging_layer/public/report_client.h"
#include "chrome/browser/policy/messaging_layer/public/report_client_test_util.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/account_id/account_id.h"
#include "components/enterprise/data_controls/core/browser/dlp_histogram_helper.h"
#include "components/enterprise/data_controls/core/browser/dlp_policy_event.pb.h"
#include "components/reporting/client/mock_report_queue.h"
#include "components/reporting/storage/test_storage_module.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "extensions/common/constants.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "ui/base/data_transfer_policy/data_transfer_endpoint.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/lacros/lacros_service.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

namespace policy {

namespace {

constexpr char kExample1Url[] = "https://www.example1.com";
constexpr char kExample2Url[] = "https://www.example2.com";

constexpr size_t kNonEmptyPastedContentSize = 99u;

class MockDlpController : public DataTransferDlpController {
 public:
  explicit MockDlpController(const DlpRulesManager& dlp_rules_manager)
      : DataTransferDlpController(dlp_rules_manager) {}

  MOCK_METHOD2(
      NotifyBlockedPaste,
      void(base::optional_ref<const ui::DataTransferEndpoint> data_src,
           base::optional_ref<const ui::DataTransferEndpoint> data_dst));

  MOCK_METHOD2(
      NotifyBlockedDrop,
      void(base::optional_ref<const ui::DataTransferEndpoint> data_src,
           base::optional_ref<const ui::DataTransferEndpoint> data_dst));

  MOCK_METHOD3(WarnOnPaste,
               void(base::optional_ref<const ui::DataTransferEndpoint> data_src,
                    base::optional_ref<const ui::DataTransferEndpoint> data_dst,
                    base::OnceClosure reporting_cb));

  MOCK_METHOD4(WarnOnBlinkPaste,
               void(base::optional_ref<const ui::DataTransferEndpoint> data_src,
                    base::optional_ref<const ui::DataTransferEndpoint> data_dst,
                    content::WebContents* web_contents,
                    base::OnceCallback<void(bool)> paste_cb));

  MOCK_METHOD1(
      ShouldPasteOnWarn,
      bool(base::optional_ref<const ui::DataTransferEndpoint> data_dst));

  MOCK_METHOD1(
      ShouldCancelOnWarn,
      bool(base::optional_ref<const ui::DataTransferEndpoint> data_dst));

  MOCK_METHOD3(WarnOnDrop,
               void(base::optional_ref<const ui::DataTransferEndpoint> data_src,
                    base::optional_ref<const ui::DataTransferEndpoint> data_dst,
                    base::OnceClosure drop_cb));

 protected:
  base::TimeDelta GetSkipReportingTimeout() override {
    // Override with a very high value to ensure that tests are passing on slow
    // debug builds.
    return base::Milliseconds(1000);
  }
};

std::optional<ui::DataTransferEndpoint> CreateEndpoint(
    ui::EndpointType* type,
    bool notify_if_restricted) {
  if (type && *type == ui::EndpointType::kUrl) {
    return ui::DataTransferEndpoint(
        GURL(kExample2Url), {.notify_if_restricted = notify_if_restricted});
  } else if (type) {
    return ui::DataTransferEndpoint(
        *type, {.notify_if_restricted = notify_if_restricted});
  }
  return std::nullopt;
}

std::unique_ptr<content::WebContents> CreateTestWebContents(
    content::BrowserContext* browser_context) {
  auto site_instance = content::SiteInstance::Create(browser_context);
  return content::WebContentsTester::CreateTestWebContents(
      browser_context, std::move(site_instance));
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
data_controls::Component GetComponent(ui::EndpointType endpoint_type) {
  switch (endpoint_type) {
    case ui::EndpointType::kArc:
      return data_controls::Component::kArc;
    case ui::EndpointType::kCrostini:
      return data_controls::Component::kCrostini;
    case ui::EndpointType::kPluginVm:
      return data_controls::Component::kPluginVm;
    default:
      return data_controls::Component::kUnknownComponent;
  }
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace

class DataTransferDlpControllerTest
    : public ::testing::TestWithParam<
          std::tuple<std::optional<ui::EndpointType>, bool>> {
 protected:
  DataTransferDlpControllerTest() {}

  ~DataTransferDlpControllerTest() override = default;

  void SetUp() override {
    // Initialize `testing_profile_` and dependant class members here as it
    // depends on Lacros being properly initialized.
    testing_profile_ = TestingProfile::Builder().Build();
    test_reporting_ =
        ::reporting::ReportingClient::TestEnvironment::CreateWithStorageModule(
            base::MakeRefCounted<::reporting::test::TestStorageModule>());
    rules_manager_ = std::make_unique<::testing::NiceMock<MockDlpRulesManager>>(
        testing_profile_.get());
    dlp_controller_ =
        std::make_unique<::testing::StrictMock<MockDlpController>>(
            *rules_manager_);

    // In tests Manager can only be created after TestEnvironment.
    reporting_manager_ = std::make_unique<data_controls::DlpReportingManager>();
    data_controls::SetReportQueueForReportingManager(
        reporting_manager_.get(), events_,
        base::ThreadPool::CreateSequencedTaskRunner({}));
    ON_CALL(*rules_manager_, GetReportingManager)
        .WillByDefault(::testing::Return(reporting_manager_.get()));
  }

  content::BrowserTaskEnvironment task_environment_;
  content::RenderViewHostTestEnabler rvh_test_enabler_;
  std::unique_ptr<TestingProfile> testing_profile_;
  std::unique_ptr<::reporting::ReportingClient::TestEnvironment>
      test_reporting_;
  std::unique_ptr<::testing::NiceMock<MockDlpRulesManager>> rules_manager_;
  std::unique_ptr<::testing::StrictMock<MockDlpController>> dlp_controller_;
  base::HistogramTester histogram_tester_;
  std::unique_ptr<data_controls::DlpReportingManager> reporting_manager_;
  std::vector<DlpPolicyEvent> events_;
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  chromeos::LacrosService lacros_service_;
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
};

TEST_F(DataTransferDlpControllerTest, NullSrc) {
  EXPECT_EQ(true, dlp_controller_->IsClipboardReadAllowed(
                      std::nullopt, std::nullopt, std::nullopt));

  ::testing::StrictMock<base::MockOnceClosure> callback;
  EXPECT_CALL(callback, Run());

  dlp_controller_->DropIfAllowed(/*data_src=*/std::nullopt,
                                 /*data_dst=*/std::nullopt,
                                 /*filenames=*/std::nullopt, callback.Get());

  histogram_tester_.ExpectUniqueSample(
      data_controls::GetDlpHistogramPrefix() +
          data_controls::dlp::kClipboardReadBlockedUMA,
      false, 1);
  histogram_tester_.ExpectUniqueSample(
      data_controls::GetDlpHistogramPrefix() +
          data_controls::dlp::kDragDropBlockedUMA,
      false, 1);
}

TEST_F(DataTransferDlpControllerTest, ClipboardHistoryDst) {
  ui::DataTransferEndpoint data_src((GURL(kExample1Url)));
  ui::DataTransferEndpoint data_dst(ui::EndpointType::kClipboardHistory);
  EXPECT_EQ(true, dlp_controller_->IsClipboardReadAllowed(data_src, data_dst,
                                                          std::nullopt));
  histogram_tester_.ExpectUniqueSample(
      data_controls::GetDlpHistogramPrefix() +
          data_controls::dlp::kClipboardReadBlockedUMA,
      false, 1);
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(DataTransferDlpControllerTest, LacrosDst) {
  ui::DataTransferEndpoint data_src((GURL(kExample1Url)));
  ui::DataTransferEndpoint data_dst(ui::EndpointType::kLacros);
  EXPECT_EQ(true, dlp_controller_->IsClipboardReadAllowed(data_src, data_dst,
                                                          std::nullopt));
  histogram_tester_.ExpectUniqueSample(
      data_controls::GetDlpHistogramPrefix() +
          data_controls::dlp::kClipboardReadBlockedUMA,
      false, 1);
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

TEST_F(DataTransferDlpControllerTest, PasteIfAllowed_Allow) {
  ui::DataTransferEndpoint data_src((GURL(kExample1Url)));
  ui::DataTransferEndpoint data_dst((GURL(kExample2Url)));

  // IsClipboardReadAllowed
  EXPECT_CALL(*rules_manager_, IsRestrictedDestination)
      .WillOnce(testing::Return(DlpRulesManager::Level::kAllow));

  ::testing::StrictMock<base::MockOnceCallback<void(bool)>> callback;
  EXPECT_CALL(callback, Run(true));

  absl::variant<size_t, std::vector<base::FilePath>> pasted_content =
      kNonEmptyPastedContentSize;
  auto web_contents = CreateTestWebContents(testing_profile_.get());
  dlp_controller_->PasteIfAllowed(
      &data_src, &data_dst, std::move(pasted_content),
      web_contents->GetPrimaryMainFrame(), callback.Get());
}

TEST_F(DataTransferDlpControllerTest, PasteIfAllowed_NullWebContents) {
  ui::DataTransferEndpoint data_src((GURL(kExample1Url)));
  ui::DataTransferEndpoint data_dst((GURL(kExample2Url)));

  ::testing::StrictMock<base::MockOnceCallback<void(bool)>> callback;
  EXPECT_CALL(callback, Run(false));

  absl::variant<size_t, std::vector<base::FilePath>> pasted_content =
      kNonEmptyPastedContentSize;
  dlp_controller_->PasteIfAllowed(
      &data_src, &data_dst, std::move(pasted_content), nullptr, callback.Get());
}

TEST_F(DataTransferDlpControllerTest, PasteIfAllowed_WarnDst) {
  ui::DataTransferEndpoint data_src((GURL(kExample1Url)));
  ui::DataTransferEndpoint data_dst((GURL(kExample2Url)));

  auto web_contents = CreateTestWebContents(testing_profile_.get());

  ::testing::StrictMock<base::MockOnceCallback<void(bool)>> callback;

  // ShouldPasteOnWarn returns false.
  EXPECT_CALL(*rules_manager_, IsRestrictedDestination)
      .WillOnce(testing::Return(DlpRulesManager::Level::kWarn));
  EXPECT_CALL(*dlp_controller_, ShouldPasteOnWarn)
      .WillRepeatedly(testing::Return(false));
  EXPECT_CALL(*dlp_controller_, ShouldCancelOnWarn)
      .WillRepeatedly(testing::Return(false));
  EXPECT_CALL(*dlp_controller_, WarnOnBlinkPaste);

  absl::variant<size_t, std::vector<base::FilePath>> pasted_content =
      kNonEmptyPastedContentSize;
  dlp_controller_->PasteIfAllowed(
      &data_src, &data_dst, std::move(pasted_content),
      web_contents->GetPrimaryMainFrame(), callback.Get());
  // We are not expecting warning proceeded event here. Warning proceeded event
  // is sent after a user accept the warn dialogue.
  // However, DataTransferDlpController::WarnOnBlinkPaste method is mocked
  // and consequently the dialog is not displayed.
  EXPECT_EQ(events_.size(), 1u);
  EXPECT_THAT(
      events_[0],
      data_controls::IsDlpPolicyEvent(data_controls::CreateDlpPolicyEvent(
          GURL(kExample1Url).spec(), GURL(kExample2Url).spec(),
          DlpRulesManager::Restriction::kClipboard, "", "",
          DlpRulesManager::Level::kWarn)));
}

TEST_F(DataTransferDlpControllerTest, PasteIfAllowed_ProceedDst) {
  ui::DataTransferEndpoint data_src((GURL(kExample1Url)));
  ui::DataTransferEndpoint data_dst((GURL(kExample2Url)));

  auto web_contents = CreateTestWebContents(testing_profile_.get());

  ::testing::StrictMock<base::MockOnceCallback<void(bool)>> callback;

  // ShouldPasteOnWarn returns true.
  EXPECT_CALL(*rules_manager_, IsRestrictedDestination)
      .WillOnce(testing::Return(DlpRulesManager::Level::kWarn));
  EXPECT_CALL(*dlp_controller_, ShouldPasteOnWarn)
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(*dlp_controller_, ShouldCancelOnWarn)
      .WillRepeatedly(testing::Return(false));

  EXPECT_CALL(callback, Run(true));
  absl::variant<size_t, std::vector<base::FilePath>> pasted_content =
      kNonEmptyPastedContentSize;
  dlp_controller_->PasteIfAllowed(
      &data_src, &data_dst, std::move(pasted_content),
      web_contents->GetPrimaryMainFrame(), callback.Get());
  EXPECT_EQ(events_.size(), 1u);
  EXPECT_THAT(events_[0],
              data_controls::IsDlpPolicyEvent(
                  data_controls::CreateDlpPolicyWarningProceededEvent(
                      GURL(kExample1Url).spec(), GURL(kExample2Url).spec(),
                      DlpRulesManager::Restriction::kClipboard, "", "")));
}

TEST_F(DataTransferDlpControllerTest, PasteIfAllowed_CancelDst) {
  ui::DataTransferEndpoint data_src((GURL(kExample1Url)));
  ui::DataTransferEndpoint data_dst((GURL(kExample2Url)));

  auto web_contents = CreateTestWebContents(testing_profile_.get());

  ::testing::StrictMock<base::MockOnceCallback<void(bool)>> callback;

  // ShouldCancelOnWarn returns true.
  EXPECT_CALL(*rules_manager_, IsRestrictedDestination)
      .WillOnce(testing::Return(DlpRulesManager::Level::kWarn));
  EXPECT_CALL(*dlp_controller_, ShouldPasteOnWarn)
      .WillRepeatedly(testing::Return(false));
  EXPECT_CALL(*dlp_controller_, ShouldCancelOnWarn)
      .WillRepeatedly(testing::Return(true));

  EXPECT_CALL(callback, Run(false));
  absl::variant<size_t, std::vector<base::FilePath>> pasted_content =
      kNonEmptyPastedContentSize;
  dlp_controller_->PasteIfAllowed(
      &data_src, &data_dst, std::move(pasted_content),
      web_contents->GetPrimaryMainFrame(), callback.Get());
  EXPECT_TRUE(events_.empty());
}

class MockFilesController : public policy::DlpFilesController {
 public:
  explicit MockFilesController(const policy::DlpRulesManager& rules_manager)
      : DlpFilesController(rules_manager) {}
  ~MockFilesController() override = default;

  MOCK_METHOD(void,
              CheckIfPasteOrDropIsAllowed,
              (const std::vector<base::FilePath>& files,
               const ui::DataTransferEndpoint* data_dst,
               CheckIfDlpAllowedCallback result_callback),
              (override));

  MOCK_METHOD(std::optional<data_controls::Component>,
              MapFilePathToPolicyComponent,
              (Profile * profile, const base::FilePath& file_path),
              (override));

  MOCK_METHOD(bool,
              IsInLocalFileSystem,
              (const base::FilePath& file_path),
              (override));

  MOCK_METHOD(void,
              ShowDlpBlockedFiles,
              (std::optional<uint64_t> task_id,
               std::vector<base::FilePath> blocked_files,
               dlp::FileAction action),
              (override));
};

TEST_F(DataTransferDlpControllerTest, DropFile_Blocked) {
  const base::FilePath path("file1.txt");

  auto drag_data = ui::OSExchangeData();
  drag_data.SetFilename(path);
  drag_data.SetSource(std::make_unique<ui::DataTransferEndpoint>(
      GURL(base::StrCat({extensions::kExtensionScheme, "://",
                         extension_misc::kFilesManagerAppId}))));
  ui::DataTransferEndpoint data_dst((GURL(kExample1Url)));

  MockFilesController files_controller(*rules_manager_);
  std::optional<std::vector<ui::FileInfo>> file_names =
      drag_data.GetFilenames();
  ASSERT_TRUE(file_names.has_value());

  EXPECT_CALL(*rules_manager_, GetDlpFilesController)
      .WillOnce(testing::Return(&files_controller));
  EXPECT_CALL(files_controller,
              CheckIfPasteOrDropIsAllowed(std::vector<base::FilePath>{path},
                                          testing::NotNull(),
                                          base::test::IsNotNullCallback()))
      .WillOnce(base::test::RunOnceCallback<2>(false));

  ::testing::StrictMock<base::MockOnceClosure> drop_callback;
  dlp_controller_->DropIfAllowed({*drag_data.GetSource()}, {data_dst},
                                 drag_data.GetFilenames(), drop_callback.Get());
}

TEST_F(DataTransferDlpControllerTest, DropFile_Allowed) {
  const base::FilePath path("file1.txt");

  auto drag_data = ui::OSExchangeData();
  drag_data.SetFilename(path);
  drag_data.SetSource(std::make_unique<ui::DataTransferEndpoint>(
      GURL(base::StrCat({extensions::kExtensionScheme, "://",
                         extension_misc::kFilesManagerAppId}))));
  ui::DataTransferEndpoint data_dst((GURL(kExample1Url)));

  MockFilesController files_controller(*rules_manager_);
  std::optional<std::vector<ui::FileInfo>> file_names =
      drag_data.GetFilenames();
  ASSERT_TRUE(file_names.has_value());

  EXPECT_CALL(*rules_manager_, GetDlpFilesController)
      .WillOnce(testing::Return(&files_controller));
  EXPECT_CALL(files_controller,
              CheckIfPasteOrDropIsAllowed(std::vector<base::FilePath>{path},
                                          testing::NotNull(),
                                          base::test::IsNotNullCallback()))
      .WillOnce(base::test::RunOnceCallback<2>(true));

  ::testing::StrictMock<base::MockOnceClosure> drop_callback;
  EXPECT_CALL(drop_callback, Run);
  dlp_controller_->DropIfAllowed({*drag_data.GetSource()}, {data_dst},
                                 drag_data.GetFilenames(), drop_callback.Get());
}

TEST_F(DataTransferDlpControllerTest, PasteFile_Blocked) {
  ui::DataTransferEndpoint* data_src = nullptr;

  auto path = base::FilePath("file1.txt");
  ui::DataTransferEndpoint data_dst((GURL(kExample1Url)));

  MockFilesController files_controller(*rules_manager_);

  EXPECT_CALL(*rules_manager_, GetDlpFilesController)
      .WillOnce(testing::Return(&files_controller));
  EXPECT_CALL(files_controller, CheckIfPasteOrDropIsAllowed(
                                    std::vector<base::FilePath>{path},
                                    &data_dst, base::test::IsNotNullCallback()))
      .WillOnce(base::test::RunOnceCallback<2>(false));

  auto web_contents = CreateTestWebContents(testing_profile_.get());

  ::testing::StrictMock<base::MockOnceCallback<void(bool)>> paste_callback;
  EXPECT_CALL(paste_callback, Run(false));

  absl::variant<size_t, std::vector<base::FilePath>> pasted_content =
      std::vector<base::FilePath>{path};
  dlp_controller_->PasteIfAllowed(data_src, data_dst, std::move(pasted_content),
                                  web_contents->GetPrimaryMainFrame(),
                                  paste_callback.Get());
}

TEST_F(DataTransferDlpControllerTest, PasteFile_Allowed) {
  ui::DataTransferEndpoint* data_src = nullptr;

  auto path = base::FilePath("file1.txt");
  ui::DataTransferEndpoint data_dst((GURL(kExample1Url)));

  MockFilesController files_controller(*rules_manager_);

  EXPECT_CALL(*rules_manager_, GetDlpFilesController)
      .WillOnce(testing::Return(&files_controller));
  EXPECT_CALL(files_controller, CheckIfPasteOrDropIsAllowed(
                                    std::vector<base::FilePath>{path},
                                    &data_dst, base::test::IsNotNullCallback()))
      .WillOnce(base::test::RunOnceCallback<2>(true));

  auto web_contents = CreateTestWebContents(testing_profile_.get());

  ::testing::StrictMock<base::MockOnceCallback<void(bool)>> paste_callback;
  EXPECT_CALL(paste_callback, Run(true));

  absl::variant<size_t, std::vector<base::FilePath>> pasted_content =
      std::vector<base::FilePath>{path};
  dlp_controller_->PasteIfAllowed(data_src, data_dst, std::move(pasted_content),
                                  web_contents->GetPrimaryMainFrame(),
                                  paste_callback.Get());
}

// Create a version of the test class for parameterized testing.
class DlpControllerTest : public DataTransferDlpControllerTest {
 protected:
  void SetUp() override {
    DataTransferDlpControllerTest::SetUp();
    data_src_ = ui::DataTransferEndpoint((GURL(kExample1Url)));
    drag_data_.SetSource(std::make_unique<ui::DataTransferEndpoint>(data_src_));
    std::optional<ui::EndpointType> endpoint_type;
    std::tie(endpoint_type, do_notify_) = GetParam();
    data_dst_ = CreateEndpoint(base::OptionalToPtr(endpoint_type), do_notify_);
  }

  ui::DataTransferEndpoint data_src_{ui::EndpointType::kDefault};
  bool do_notify_;
  std::optional<ui::DataTransferEndpoint> data_dst_;
  ui::OSExchangeData drag_data_;
};

INSTANTIATE_TEST_SUITE_P(
    DlpClipboard,
    DlpControllerTest,
    ::testing::Combine(::testing::Values(std::nullopt,
                                         ui::EndpointType::kDefault,
#if BUILDFLAG(IS_CHROMEOS_ASH)
                                         ui::EndpointType::kUnknownVm,
                                         ui::EndpointType::kBorealis,
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
                                         ui::EndpointType::kUrl),
                       testing::Bool()));

TEST_P(DlpControllerTest, Allow) {
  // IsClipboardReadAllowed
  EXPECT_CALL(*rules_manager_, IsRestrictedDestination)
      .WillOnce(testing::Return(DlpRulesManager::Level::kAllow));

  EXPECT_EQ(true, dlp_controller_->IsClipboardReadAllowed(data_src_, data_dst_,
                                                          std::nullopt));
  testing::Mock::VerifyAndClearExpectations(&dlp_controller_);

  // DropIfAllowed
  EXPECT_CALL(*rules_manager_, IsRestrictedDestination)
      .WillOnce(testing::Return(DlpRulesManager::Level::kAllow));
  ::testing::StrictMock<base::MockOnceClosure> callback;
  EXPECT_CALL(callback, Run());

  dlp_controller_->DropIfAllowed({*drag_data_.GetSource()}, {data_dst_},
                                 drag_data_.GetFilenames(), callback.Get());
  testing::Mock::VerifyAndClearExpectations(&dlp_controller_);

  histogram_tester_.ExpectUniqueSample(
      data_controls::GetDlpHistogramPrefix() +
          data_controls::dlp::kClipboardReadBlockedUMA,
      false, 1);
  histogram_tester_.ExpectUniqueSample(
      data_controls::GetDlpHistogramPrefix() +
          data_controls::dlp::kDragDropBlockedUMA,
      false, 1);
}

TEST_P(DlpControllerTest, Block_IsClipboardReadAllowed) {
  EXPECT_CALL(*rules_manager_, IsRestrictedDestination)
      .WillOnce(testing::Return(DlpRulesManager::Level::kBlock));
  if (do_notify_ || !data_dst_.has_value()) {
    EXPECT_CALL(*dlp_controller_, NotifyBlockedPaste);
  }

  EXPECT_EQ(false, dlp_controller_->IsClipboardReadAllowed(data_src_, data_dst_,
                                                           std::nullopt));
  testing::Mock::VerifyAndClearExpectations(&dlp_controller_);

  if (!data_dst_ || do_notify_) {
    std::string dst_url = data_dst_.has_value() && data_dst_->IsUrlType()
                              ? data_dst_->GetURL()->spec()
                              : "";
    EXPECT_EQ(events_.size(), 1u);
    EXPECT_THAT(events_[0], data_controls::IsDlpPolicyEvent(
                                data_controls::CreateDlpPolicyEvent(
                                    data_src_.GetURL()->spec(), dst_url,
                                    DlpRulesManager::Restriction::kClipboard,
                                    "", "", DlpRulesManager::Level::kBlock)));
  } else {
    EXPECT_TRUE(events_.empty());
  }

  histogram_tester_.ExpectUniqueSample(
      data_controls::GetDlpHistogramPrefix() +
          data_controls::dlp::kClipboardReadBlockedUMA,
      true, 1);
}

TEST_P(DlpControllerTest, Block_DropIfAllowed) {
  EXPECT_CALL(*rules_manager_, IsRestrictedDestination)
      .WillOnce(testing::Return(DlpRulesManager::Level::kBlock));
  EXPECT_CALL(*dlp_controller_, NotifyBlockedDrop);
  ::testing::StrictMock<base::MockOnceClosure> callback;

  dlp_controller_->DropIfAllowed({*drag_data_.GetSource()}, {data_dst_},
                                 drag_data_.GetFilenames(), callback.Get());
  testing::Mock::VerifyAndClearExpectations(&dlp_controller_);

  std::string dst_url = data_dst_.has_value() && data_dst_->IsUrlType()
                            ? data_dst_->GetURL()->spec()
                            : "";
  EXPECT_EQ(events_.size(), 1u);
  EXPECT_THAT(events_[0], data_controls::IsDlpPolicyEvent(
                              data_controls::CreateDlpPolicyEvent(
                                  data_src_.GetURL()->spec(), dst_url,
                                  DlpRulesManager::Restriction::kClipboard, "",
                                  "", DlpRulesManager::Level::kBlock)));

  histogram_tester_.ExpectUniqueSample(
      data_controls::GetDlpHistogramPrefix() +
          data_controls::dlp::kDragDropBlockedUMA,
      true, 1);
}

TEST_P(DlpControllerTest, Report_IsClipboardReadAllowed) {
  EXPECT_CALL(*rules_manager_, IsRestrictedDestination)
      .WillOnce(testing::Return(DlpRulesManager::Level::kReport));

  EXPECT_EQ(true, dlp_controller_->IsClipboardReadAllowed(data_src_, data_dst_,
                                                          std::nullopt));
  testing::Mock::VerifyAndClearExpectations(&dlp_controller_);

  if (!data_dst_ || do_notify_) {
    std::string dst_url = data_dst_.has_value() && data_dst_->IsUrlType()
                              ? data_dst_->GetURL()->spec()
                              : "";
    EXPECT_EQ(events_.size(), 1u);
    EXPECT_THAT(events_[0], data_controls::IsDlpPolicyEvent(
                                data_controls::CreateDlpPolicyEvent(
                                    data_src_.GetURL()->spec(), dst_url,
                                    DlpRulesManager::Restriction::kClipboard,
                                    "", "", DlpRulesManager::Level::kReport)));
  } else {
    EXPECT_TRUE(events_.empty());
  }
}

TEST_P(DlpControllerTest, Report_DropIfAllowed) {
  EXPECT_CALL(*rules_manager_, IsRestrictedDestination)
      .WillOnce(testing::Return(DlpRulesManager::Level::kReport));
  ::testing::StrictMock<base::MockOnceClosure> callback;
  EXPECT_CALL(callback, Run());

  dlp_controller_->DropIfAllowed({*drag_data_.GetSource()}, {data_dst_},
                                 drag_data_.GetFilenames(), callback.Get());
  testing::Mock::VerifyAndClearExpectations(&dlp_controller_);

  std::string dst_url = data_dst_.has_value() && data_dst_->IsUrlType()
                            ? data_dst_->GetURL()->spec()
                            : "";
  EXPECT_EQ(events_.size(), 1u);
  EXPECT_THAT(events_[0], data_controls::IsDlpPolicyEvent(
                              data_controls::CreateDlpPolicyEvent(
                                  data_src_.GetURL()->spec(), dst_url,
                                  DlpRulesManager::Restriction::kClipboard, "",
                                  "", DlpRulesManager::Level::kReport)));
}

TEST_P(DlpControllerTest, Warn_IsClipboardReadAllowed) {
  // ShouldPasteOnWarn returns false.
  EXPECT_CALL(*rules_manager_, IsRestrictedDestination)
      .WillOnce(testing::Return(DlpRulesManager::Level::kWarn));
  EXPECT_CALL(*dlp_controller_, ShouldPasteOnWarn)
      .WillRepeatedly(testing::Return(false));
  EXPECT_CALL(*dlp_controller_, ShouldCancelOnWarn)
      .WillRepeatedly(testing::Return(false));
  bool show_warning =
      data_dst_.has_value() ? (do_notify_ && !data_dst_->IsUrlType()) : true;
  if (show_warning) {
    EXPECT_CALL(*dlp_controller_, WarnOnPaste);
  }

  EXPECT_EQ(!show_warning, dlp_controller_->IsClipboardReadAllowed(
                               data_src_, data_dst_, std::nullopt));
  if (show_warning) {
    std::string dst_url = data_dst_.has_value() && data_dst_->IsUrlType()
                              ? data_dst_->GetURL()->spec()
                              : "";
    EXPECT_EQ(events_.size(), 1u);
    EXPECT_THAT(events_[0], data_controls::IsDlpPolicyEvent(
                                data_controls::CreateDlpPolicyEvent(
                                    data_src_.GetURL()->spec(), dst_url,
                                    DlpRulesManager::Restriction::kClipboard,
                                    "", "", DlpRulesManager::Level::kWarn)));
  }
  testing::Mock::VerifyAndClearExpectations(&dlp_controller_);

  // ShouldPasteOnWarn returns true.
  EXPECT_CALL(*rules_manager_, IsRestrictedDestination)
      .WillOnce(testing::Return(DlpRulesManager::Level::kWarn));
  EXPECT_CALL(*dlp_controller_, ShouldPasteOnWarn)
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(*dlp_controller_, ShouldCancelOnWarn)
      .WillRepeatedly(testing::Return(false));
  EXPECT_EQ(true, dlp_controller_->IsClipboardReadAllowed(data_src_, data_dst_,
                                                          std::nullopt));
  EXPECT_EQ(events_.size(), show_warning ? 1u : 0u);
  testing::Mock::VerifyAndClearExpectations(&dlp_controller_);

  histogram_tester_.ExpectBucketCount(
      data_controls::GetDlpHistogramPrefix() +
          data_controls::dlp::kClipboardReadBlockedUMA,
      false, show_warning ? 1 : 2);
  histogram_tester_.ExpectBucketCount(
      data_controls::GetDlpHistogramPrefix() +
          data_controls::dlp::kClipboardReadBlockedUMA,
      true, show_warning ? 1 : 0);
}

TEST_P(DlpControllerTest, Warn_ShouldCancelOnWarn) {
  // ShouldCancelOnWarn returns true.
  EXPECT_CALL(*rules_manager_, IsRestrictedDestination)
      .WillOnce(testing::Return(DlpRulesManager::Level::kWarn));
  EXPECT_CALL(*dlp_controller_, ShouldCancelOnWarn)
      .WillRepeatedly(testing::Return(true));

  EXPECT_EQ(false, dlp_controller_->IsClipboardReadAllowed(data_src_, data_dst_,
                                                           std::nullopt));
  testing::Mock::VerifyAndClearExpectations(&dlp_controller_);
}

TEST_P(DlpControllerTest, Warn_DropIfAllowed) {
  EXPECT_CALL(*rules_manager_, IsRestrictedDestination)
      .WillOnce(testing::Return(DlpRulesManager::Level::kWarn));
  EXPECT_CALL(*dlp_controller_, WarnOnDrop);

  ::testing::StrictMock<base::MockOnceClosure> callback;

  dlp_controller_->DropIfAllowed({*drag_data_.GetSource()}, {data_dst_},
                                 drag_data_.GetFilenames(), callback.Get());
  testing::Mock::VerifyAndClearExpectations(&dlp_controller_);

  histogram_tester_.ExpectUniqueSample(
      data_controls::GetDlpHistogramPrefix() +
          data_controls::dlp::kDragDropBlockedUMA,
      true, 1);
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Create a version of the test class for parameterized testing.
class DlpControllerVMsTest : public DataTransferDlpControllerTest {
 protected:
  void SetUp() override {
    DataTransferDlpControllerTest::SetUp();
    data_src_ = ui::DataTransferEndpoint((GURL(kExample1Url)));
    drag_data_.SetSource(std::make_unique<ui::DataTransferEndpoint>(data_src_));
    std::tie(endpoint_type_, do_notify_) = GetParam();
    ASSERT_TRUE(endpoint_type_.has_value());
    data_dst_ = ui::DataTransferEndpoint(endpoint_type_.value(),
                                         {.notify_if_restricted = do_notify_});
  }

  ui::DataTransferEndpoint data_src_{ui::EndpointType::kDefault};
  ui::OSExchangeData drag_data_;
  std::optional<ui::EndpointType> endpoint_type_;
  bool do_notify_;
  ui::DataTransferEndpoint data_dst_{ui::EndpointType::kDefault};
};

INSTANTIATE_TEST_SUITE_P(
    DlpClipboard,
    DlpControllerVMsTest,
    ::testing::Combine(::testing::Values(ui::EndpointType::kArc,
                                         ui::EndpointType::kCrostini,
                                         ui::EndpointType::kPluginVm),
                       testing::Bool()));

TEST_P(DlpControllerVMsTest, Allow) {
  ui::DataTransferEndpoint data_src((GURL(kExample1Url)));
  auto [endpoint_type, do_notify] = GetParam();
  ASSERT_TRUE(endpoint_type.has_value());
  ui::DataTransferEndpoint data_dst(endpoint_type.value(),
                                    {.notify_if_restricted = do_notify});

  // IsClipboardReadAllowed
  EXPECT_CALL(*rules_manager_, IsRestrictedComponent)
      .WillOnce(testing::Return(DlpRulesManager::Level::kAllow));

  EXPECT_EQ(true, dlp_controller_->IsClipboardReadAllowed(data_src, data_dst,
                                                          std::nullopt));
  testing::Mock::VerifyAndClearExpectations(&dlp_controller_);

  // DropIfAllowed
  EXPECT_CALL(*rules_manager_, IsRestrictedComponent)
      .WillOnce(testing::Return(DlpRulesManager::Level::kAllow));
  ::testing::StrictMock<base::MockOnceClosure> callback;
  EXPECT_CALL(callback, Run());

  dlp_controller_->DropIfAllowed({*drag_data_.GetSource()}, {data_dst_},
                                 drag_data_.GetFilenames(), callback.Get());
  testing::Mock::VerifyAndClearExpectations(&dlp_controller_);

  histogram_tester_.ExpectUniqueSample(
      data_controls::GetDlpHistogramPrefix() +
          data_controls::dlp::kClipboardReadBlockedUMA,
      false, 1);
  histogram_tester_.ExpectUniqueSample(
      data_controls::GetDlpHistogramPrefix() +
          data_controls::dlp::kDragDropBlockedUMA,
      false, 1);
}

TEST_P(DlpControllerVMsTest, Block_IsClipboardReadAllowed) {
  EXPECT_CALL(*rules_manager_, IsRestrictedComponent)
      .WillOnce(testing::Return(DlpRulesManager::Level::kBlock));
  if (do_notify_) {
    EXPECT_CALL(*dlp_controller_, NotifyBlockedPaste);
  }

  EXPECT_EQ(false, dlp_controller_->IsClipboardReadAllowed(data_src_, data_dst_,
                                                           std::nullopt));
  testing::Mock::VerifyAndClearExpectations(&dlp_controller_);

  if (do_notify_) {
    EXPECT_EQ(events_.size(), 1u);
    EXPECT_THAT(
        events_[0],
        data_controls::IsDlpPolicyEvent(data_controls::CreateDlpPolicyEvent(
            data_src_.GetURL()->spec(), GetComponent(endpoint_type_.value()),
            DlpRulesManager::Restriction::kClipboard, "", "",
            DlpRulesManager::Level::kBlock)));
  } else {
    EXPECT_TRUE(events_.empty());
  }

  histogram_tester_.ExpectUniqueSample(
      data_controls::GetDlpHistogramPrefix() +
          data_controls::dlp::kClipboardReadBlockedUMA,
      true, 1);
}

TEST_P(DlpControllerVMsTest, Block_DropIfAllowed) {
  EXPECT_CALL(*rules_manager_, IsRestrictedComponent)
      .WillOnce(testing::Return(DlpRulesManager::Level::kBlock));
  EXPECT_CALL(*dlp_controller_, NotifyBlockedDrop);
  ::testing::StrictMock<base::MockOnceClosure> callback;

  dlp_controller_->DropIfAllowed({*drag_data_.GetSource()}, {data_dst_},
                                 drag_data_.GetFilenames(), callback.Get());
  testing::Mock::VerifyAndClearExpectations(&dlp_controller_);

  ASSERT_EQ(events_.size(), 1u);
  EXPECT_THAT(
      events_[0],
      data_controls::IsDlpPolicyEvent(data_controls::CreateDlpPolicyEvent(
          data_src_.GetURL()->spec(), GetComponent(endpoint_type_.value()),
          DlpRulesManager::Restriction::kClipboard, "", "",
          DlpRulesManager::Level::kBlock)));

  histogram_tester_.ExpectUniqueSample(
      data_controls::GetDlpHistogramPrefix() +
          data_controls::dlp::kDragDropBlockedUMA,
      true, 1);
}

TEST_P(DlpControllerVMsTest, Report_IsClipboardReadAllowed) {
  EXPECT_CALL(*rules_manager_, IsRestrictedComponent)
      .WillOnce(testing::Return(DlpRulesManager::Level::kReport));

  EXPECT_EQ(true, dlp_controller_->IsClipboardReadAllowed(data_src_, data_dst_,
                                                          std::nullopt));
  testing::Mock::VerifyAndClearExpectations(&dlp_controller_);

  if (do_notify_) {
    EXPECT_EQ(events_.size(), 1u);
    EXPECT_THAT(
        events_[0],
        data_controls::IsDlpPolicyEvent(data_controls::CreateDlpPolicyEvent(
            data_src_.GetURL()->spec(), GetComponent(endpoint_type_.value()),
            DlpRulesManager::Restriction::kClipboard, "", "",
            DlpRulesManager::Level::kReport)));
  } else {
    EXPECT_TRUE(events_.empty());
  }
}

TEST_P(DlpControllerVMsTest, Report_DropIfAllowed) {
  EXPECT_CALL(*rules_manager_, IsRestrictedComponent)
      .WillOnce(testing::Return(DlpRulesManager::Level::kReport));
  ::testing::StrictMock<base::MockOnceClosure> callback;
  EXPECT_CALL(callback, Run());

  dlp_controller_->DropIfAllowed({*drag_data_.GetSource()}, {data_dst_},
                                 drag_data_.GetFilenames(), callback.Get());
  testing::Mock::VerifyAndClearExpectations(&dlp_controller_);

  ASSERT_EQ(events_.size(), 1u);
  EXPECT_THAT(
      events_[0],
      data_controls::IsDlpPolicyEvent(data_controls::CreateDlpPolicyEvent(
          data_src_.GetURL()->spec(), GetComponent(endpoint_type_.value()),
          DlpRulesManager::Restriction::kClipboard, "", "",
          DlpRulesManager::Level::kReport)));
}

TEST_P(DlpControllerVMsTest, Warn_IsClipboardReadAllowed) {
  ui::DataTransferEndpoint data_src((GURL(kExample1Url)));
  auto [endpoint_type, do_notify] = GetParam();
  ASSERT_TRUE(endpoint_type.has_value());
  ui::DataTransferEndpoint data_dst(endpoint_type.value(),
                                    {.notify_if_restricted = do_notify});

  // IsClipboardReadAllowed
  EXPECT_CALL(*rules_manager_, IsRestrictedComponent)
      .WillOnce(testing::Return(DlpRulesManager::Level::kWarn));
  if (do_notify) {
    EXPECT_CALL(*dlp_controller_, WarnOnPaste);
  }

  EXPECT_EQ(true, dlp_controller_->IsClipboardReadAllowed(data_src, data_dst,
                                                          std::nullopt));
  if (do_notify) {
    EXPECT_EQ(events_.size(), 1u);
    EXPECT_THAT(
        events_[0],
        data_controls::IsDlpPolicyEvent(data_controls::CreateDlpPolicyEvent(
            data_src_.GetURL()->spec(), GetComponent(endpoint_type.value()),
            DlpRulesManager::Restriction::kClipboard, "", "",
            DlpRulesManager::Level::kWarn)));
  }
  testing::Mock::VerifyAndClearExpectations(&dlp_controller_);
  histogram_tester_.ExpectUniqueSample(
      data_controls::GetDlpHistogramPrefix() +
          data_controls::dlp::kClipboardReadBlockedUMA,
      false, 1);
}

TEST_P(DlpControllerVMsTest, Warn_DropIfAllowed) {
  EXPECT_CALL(*rules_manager_, IsRestrictedComponent)
      .WillOnce(testing::Return(DlpRulesManager::Level::kWarn));
  EXPECT_CALL(*dlp_controller_, WarnOnDrop);
  ::testing::StrictMock<base::MockOnceClosure> callback;

  dlp_controller_->DropIfAllowed({*drag_data_.GetSource()}, {data_dst_},
                                 drag_data_.GetFilenames(), callback.Get());

  testing::Mock::VerifyAndClearExpectations(&dlp_controller_);
  histogram_tester_.ExpectUniqueSample(
      data_controls::GetDlpHistogramPrefix() +
          data_controls::dlp::kDragDropBlockedUMA,
      true, 1);
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace policy
