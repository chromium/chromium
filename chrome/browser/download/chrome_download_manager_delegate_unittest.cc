// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/chrome_download_manager_delegate.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/guid.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/download/download_core_service_factory.h"
#include "chrome/browser/download/download_core_service_impl.h"
#include "chrome/browser/download/download_item_model.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/download/download_target_info.h"
#include "chrome/browser/download/insecure_download_blocking.h"
#include "chrome/browser/safe_browsing/download_protection/download_protection_util.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/net/safe_search_util.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/download/public/common/download_danger_type.h"
#include "components/download/public/common/download_features.h"
#include "components/download/public/common/download_interrupt_reasons.h"
#include "components/download/public/common/mock_download_item.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/buildflags.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/browser/download_item_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/test/mock_download_manager.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "net/base/network_change_notifier.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/origin.h"

#if BUILDFLAG(FULL_SAFE_BROWSING)
#include "chrome/browser/safe_browsing/download_protection/download_protection_service.h"
#endif

#if BUILDFLAG(ENABLE_PLUGINS)
#include "content/public/browser/plugin_service.h"
#endif

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/download/download_prompt_status.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/infobars/core/infobar.h"
#include "components/infobars/core/infobar_delegate.h"
#include "components/infobars/core/infobar_manager.h"
#endif

using download::DownloadItem;
using download::DownloadPathReservationTracker;
using download::PathValidationResult;
using ConnectionType = net::NetworkChangeNotifier::ConnectionType;
using safe_browsing::DownloadFileType;
using ::testing::_;
using ::testing::AnyNumber;
using ::testing::AtMost;
using ::testing::DoAll;
using ::testing::Invoke;
using ::testing::Ref;
using ::testing::Return;
using ::testing::ReturnArg;
using ::testing::ReturnPointee;
using ::testing::ReturnRef;
using ::testing::ReturnRefOfCopy;
using ::testing::SetArgPointee;
using ::testing::WithArg;
using url::Origin;

namespace {

class MockWebContentsDelegate : public content::WebContentsDelegate {
 public:
  ~MockWebContentsDelegate() override {}
};

ACTION_P3(ScheduleCallback2, result0, result1) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(arg0), result0, result1));
}

// Struct for holding the result of calling DetermineDownloadTarget.
struct DetermineDownloadTargetResult {
  base::FilePath target_path;
  download::DownloadItem::TargetDisposition disposition =
      download::DownloadItem::TARGET_DISPOSITION_OVERWRITE;
  download::DownloadDangerType danger_type =
      download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS;
  download::DownloadItem::InsecureDownloadStatus insecure_download_status =
      download::DownloadItem::InsecureDownloadStatus::UNKNOWN;
  base::FilePath intermediate_path;
  base::FilePath display_name;
  download::DownloadInterruptReason interrupt_reason =
      download::DOWNLOAD_INTERRUPT_REASON_NONE;
  std::string mime_type;
};

// Subclass of the ChromeDownloadManagerDelegate that replaces a few interaction
// points for ease of testing.
class TestChromeDownloadManagerDelegate : public ChromeDownloadManagerDelegate {
 public:
  explicit TestChromeDownloadManagerDelegate(Profile* profile)
      : ChromeDownloadManagerDelegate(profile) {
    ON_CALL(*this, MockCheckDownloadUrl(_, _))
        .WillByDefault(Return(download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS));
    ON_CALL(*this, GetDownloadProtectionService())
        .WillByDefault(Return(nullptr));
    ON_CALL(*this, MockReserveVirtualPath(_, _, _, _, _))
        .WillByDefault(DoAll(SetArgPointee<4>(PathValidationResult::SUCCESS),
                             ReturnArg<1>()));
  }

  ~TestChromeDownloadManagerDelegate() override {}

  // The concrete implementation talks to the ExtensionDownloadsEventRouter to
  // dispatch a OnDeterminingFilename event. While we would like to test this as
  // well in this unit test, we are currently going to rely on the extension
  // browser test to provide test coverage here. Instead we are going to mock it
  // out for unit tests.
  void NotifyExtensions(download::DownloadItem* download,
                        const base::FilePath& suggested_virtual_path,
                        NotifyExtensionsCallback callback) override {
    std::move(callback).Run(base::FilePath(),
                            DownloadPathReservationTracker::UNIQUIFY);
  }

  // DownloadPathReservationTracker talks to the underlying file system. For
  // tests we are going to mock it out so that we can test how
  // ChromeDownloadManagerDelegate reponds to various DownloadTargetDeterminer
  // results.
  void ReserveVirtualPath(
      download::DownloadItem* download,
      const base::FilePath& virtual_path,
      bool create_directory,
      DownloadPathReservationTracker::FilenameConflictAction conflict_action,
      DownloadPathReservationTracker::ReservedPathCallback callback) override {
    PathValidationResult result = PathValidationResult::SUCCESS;
    base::FilePath path_to_return = MockReserveVirtualPath(
        download, virtual_path, create_directory, conflict_action, &result);
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), result, path_to_return));
  }

#if BUILDFLAG(IS_ANDROID)
  void OnDownloadCanceled(download::DownloadItem* download,
                          bool has_no_external_storage) override {}
#endif

  MOCK_METHOD5(
      MockReserveVirtualPath,
      base::FilePath(download::DownloadItem*,
                     const base::FilePath&,
                     bool,
                     DownloadPathReservationTracker::FilenameConflictAction,
                     PathValidationResult*));

  // The concrete implementation invokes SafeBrowsing's
  // DownloadProtectionService. Now that SafeBrowsingService is testable, we
  // should migrate to using TestSafeBrowsingService instead.
  void CheckDownloadUrl(DownloadItem* download,
                        const base::FilePath& virtual_path,
                        CheckDownloadUrlCallback callback) override {
    std::move(callback).Run(MockCheckDownloadUrl(download, virtual_path));
  }

  MOCK_METHOD2(MockCheckDownloadUrl,
               download::DownloadDangerType(DownloadItem*,
                                            const base::FilePath&));

  MOCK_METHOD0(GetDownloadProtectionService,
               safe_browsing::DownloadProtectionService*());

  void RequestConfirmation(
      DownloadItem* item,
      const base::FilePath& path,
      DownloadConfirmationReason reason,
      DownloadTargetDeterminerDelegate::ConfirmationCallback cb) override {
    RequestConfirmation_(item, path, reason, cb);
  }

  // The concrete implementation on desktop just invokes a file picker. Android
  // has a non-trivial implementation. The former is tested via browser tests,
  // and the latter is exercised in this unit test.
  MOCK_METHOD4(RequestConfirmation_,
               void(DownloadItem*,
                    const base::FilePath&,
                    DownloadConfirmationReason,
                    DownloadTargetDeterminerDelegate::ConfirmationCallback&));

  // For testing the concrete implementation.
  void RequestConfirmationConcrete(
      DownloadItem* download_item,
      const base::FilePath& path,
      DownloadConfirmationReason reason,
      DownloadTargetDeterminerDelegate::ConfirmationCallback& callback) {
    ChromeDownloadManagerDelegate::RequestConfirmation(
        download_item, path, reason, std::move(callback));
  }

  void ShowFilePickerForDownload(
      DownloadItem* download,
      const base::FilePath& path,
      DownloadTargetDeterminerDelegate::ConfirmationCallback) override {}

  void DetermineLocalPath(download::DownloadItem* download,
                          const base::FilePath& virtual_path,
                          download::LocalPathCallback callback) override {
    std::move(callback).Run(virtual_path, virtual_path.BaseName());
  }

 private:
  friend class ChromeDownloadManagerDelegateTest;
};

// A DownloadCoreService that returns the TestChromeDownloadManagerDelegate.
class TestDownloadCoreService : public DownloadCoreServiceImpl {
 public:
  explicit TestDownloadCoreService(Profile* profile);
  ~TestDownloadCoreService() override;

  void set_download_manager_delegate(ChromeDownloadManagerDelegate* delegate) {
    delegate_ = delegate;
  }

  ChromeDownloadManagerDelegate* GetDownloadManagerDelegate() override;

  raw_ptr<ChromeDownloadManagerDelegate> delegate_;
};

TestDownloadCoreService::TestDownloadCoreService(Profile* profile)
    : DownloadCoreServiceImpl(profile) {}

TestDownloadCoreService::~TestDownloadCoreService() = default;

ChromeDownloadManagerDelegate*
TestDownloadCoreService::GetDownloadManagerDelegate() {
  return delegate_;
}

static std::unique_ptr<KeyedService> CreateTestDownloadCoreService(
    content::BrowserContext* browser_context) {
  return std::make_unique<TestDownloadCoreService>(
      Profile::FromBrowserContext(browser_context));
}

class ChromeDownloadManagerDelegateTest
    : public ChromeRenderViewHostTestHarness {
 public:
  // Result of calling DetermineDownloadTarget.
  ChromeDownloadManagerDelegateTest();

  // ::testing::Test
  void SetUp() override;
  void TearDown() override;

  // Verifies and clears test expectations for |delegate_| and
  // |download_manager_|.
  void VerifyAndClearExpectations();

  // Creates MockDownloadItem and sets up default expectations.
  std::unique_ptr<download::MockDownloadItem> CreateActiveDownloadItem(
      int32_t id);

  // Given the relative path |path|, returns the full path under the temporary
  // downloads directory.
  base::FilePath GetPathInDownloadDir(const char* path);

  void DetermineDownloadTarget(DownloadItem* download,
                               DetermineDownloadTargetResult* result);

  void OnConfirmationCallbackComplete(
      DownloadTargetDeterminerDelegate::ConfirmationCallback callback,
      DownloadConfirmationResult result,
      const base::FilePath& virtual_path);

  base::FilePath GetDownloadDirectory() const { return test_download_dir_; }
  TestChromeDownloadManagerDelegate* delegate();
  content::MockDownloadManager* download_manager();
  DownloadPrefs* download_prefs();
  PrefService* pref_service();

  // Creates a mock download item as used by mixed download blocking tests.
  std::unique_ptr<download::MockDownloadItem>
  PrepareDownloadItemForInsecureBlocking(
      const GURL& download_url,
      const absl::optional<url::Origin>& request_initiator,
      const absl::optional<GURL>& redirect_url);

  const std::vector<uint32_t>& download_ids() const { return download_ids_; }
  void GetNextId(uint32_t next_id) { download_ids_.emplace_back(next_id); }

  void VerifyMixedContentExtensionOverride(
      DownloadItem* download_item,
      const base::FieldTrialParams& parameters,
      InsecureDownloadExtensions extension,
      download::DownloadInterruptReason interrupt_reason,
      download::DownloadItem::InsecureDownloadStatus insecure_download_status);

 private:
  base::FilePath test_download_dir_;
  raw_ptr<sync_preferences::TestingPrefServiceSyncable> pref_service_;
  std::unique_ptr<content::MockDownloadManager> download_manager_;
  std::unique_ptr<TestChromeDownloadManagerDelegate> delegate_;
  MockWebContentsDelegate web_contents_delegate_;
  std::vector<uint32_t> download_ids_;
  TestingProfileManager testing_profile_manager_;
};

ChromeDownloadManagerDelegateTest::ChromeDownloadManagerDelegateTest()
    : ChromeRenderViewHostTestHarness(
          base::test::TaskEnvironment::TimeSource::MOCK_TIME),
      download_manager_(new ::testing::NiceMock<content::MockDownloadManager>),
      testing_profile_manager_(TestingBrowserProcess::GetGlobal()) {}

void ChromeDownloadManagerDelegateTest::SetUp() {
  ChromeRenderViewHostTestHarness::SetUp();

  CHECK(profile());

  test_download_dir_ = profile()->GetPath().AppendASCII("TestDownloadDir");
  ASSERT_TRUE(base::CreateDirectory(test_download_dir_));

  delegate_ =
      std::make_unique<::testing::NiceMock<TestChromeDownloadManagerDelegate>>(
          profile());
  DownloadCoreServiceFactory::GetInstance()->SetTestingFactory(
      profile(), base::BindRepeating(&CreateTestDownloadCoreService));
  static_cast<TestDownloadCoreService*>(
      DownloadCoreServiceFactory::GetForBrowserContext(profile()))
      ->set_download_manager_delegate(delegate_.get());
  download_prefs()->SkipSanitizeDownloadTargetPathForTesting();
  download_prefs()->SetDownloadPath(test_download_dir_);
  delegate_->SetDownloadManager(download_manager_.get());
  pref_service_ = profile()->GetTestingPrefService();
  web_contents()->SetDelegate(&web_contents_delegate_);

#if BUILDFLAG(IS_ANDROID)
  pref_service_->SetInteger(prefs::kPromptForDownloadAndroid,
                            static_cast<int>(DownloadPromptStatus::DONT_SHOW));
#endif
}

void ChromeDownloadManagerDelegateTest::TearDown() {
  base::RunLoop().RunUntilIdle();
  delegate_->Shutdown();
  ChromeRenderViewHostTestHarness::TearDown();
}

void ChromeDownloadManagerDelegateTest::VerifyAndClearExpectations() {
  ::testing::Mock::VerifyAndClearExpectations(delegate_.get());
}

std::unique_ptr<download::MockDownloadItem>
ChromeDownloadManagerDelegateTest::CreateActiveDownloadItem(int32_t id) {
  std::unique_ptr<download::MockDownloadItem> item(
      new ::testing::NiceMock<download::MockDownloadItem>());
  ON_CALL(*item, GetURL()).WillByDefault(ReturnRefOfCopy(GURL()));
  ON_CALL(*item, GetTabUrl()).WillByDefault(ReturnRefOfCopy(GURL()));
  ON_CALL(*item, GetUrlChain())
      .WillByDefault(ReturnRefOfCopy(std::vector<GURL>()));
  ON_CALL(*item, GetDangerType())
      .WillByDefault(Return(download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS));
  ON_CALL(*item, GetForcedFilePath())
      .WillByDefault(ReturnRefOfCopy(base::FilePath()));
  ON_CALL(*item, GetFullPath())
      .WillByDefault(ReturnRefOfCopy(base::FilePath()));
  ON_CALL(*item, GetHash())
      .WillByDefault(ReturnRefOfCopy(std::string()));
  ON_CALL(*item, GetId())
      .WillByDefault(Return(id));
  ON_CALL(*item, GetLastReason())
      .WillByDefault(Return(download::DOWNLOAD_INTERRUPT_REASON_NONE));
  ON_CALL(*item, GetReferrerUrl())
      .WillByDefault(ReturnRefOfCopy(GURL()));
  ON_CALL(*item, GetRequestInitiator())
      .WillByDefault(ReturnRefOfCopy(absl::optional<Origin>()));
  ON_CALL(*item, GetState())
      .WillByDefault(Return(DownloadItem::IN_PROGRESS));
  ON_CALL(*item, GetTargetFilePath())
      .WillByDefault(ReturnRefOfCopy(base::FilePath()));
  ON_CALL(*item, GetTransitionType())
      .WillByDefault(Return(ui::PAGE_TRANSITION_LINK));
  ON_CALL(*item, HasUserGesture())
      .WillByDefault(Return(false));
  ON_CALL(*item, IsDangerous())
      .WillByDefault(Return(false));
  ON_CALL(*item, IsTemporary())
      .WillByDefault(Return(false));
  std::string guid = base::GenerateGUID();
  ON_CALL(*item, GetGuid()).WillByDefault(ReturnRefOfCopy(guid));
  content::DownloadItemUtils::AttachInfoForTesting(item.get(), profile(),
                                                   web_contents());
  EXPECT_CALL(*download_manager_, GetDownload(id))
      .WillRepeatedly(Return(item.get()));
  EXPECT_CALL(*download_manager_, GetDownloadByGuid(guid))
      .WillRepeatedly(Return(item.get()));
  return item;
}

base::FilePath ChromeDownloadManagerDelegateTest::GetPathInDownloadDir(
    const char* relative_path) {
  base::FilePath full_path = GetDownloadDirectory().AppendASCII(relative_path);
  return full_path.NormalizePathSeparators();
}

void StoreDownloadTargetInfo(
    const base::RepeatingClosure& quit_runloop,
    DetermineDownloadTargetResult* result,
    const base::FilePath& target_path,
    DownloadItem::TargetDisposition target_disposition,
    download::DownloadDangerType danger_type,
    download::DownloadItem::InsecureDownloadStatus insecure_download_status,
    const base::FilePath& intermediate_path,
    const base::FilePath& display_name,
    const std::string& mime_type,
    download::DownloadInterruptReason interrupt_reason) {
  result->target_path = target_path;
  result->disposition = target_disposition;
  result->danger_type = danger_type;
  result->insecure_download_status = insecure_download_status;
  result->intermediate_path = intermediate_path;
  result->display_name = display_name;
  result->interrupt_reason = interrupt_reason;
  result->mime_type = mime_type;
  quit_runloop.Run();
}

void ChromeDownloadManagerDelegateTest::DetermineDownloadTarget(
    DownloadItem* download_item,
    DetermineDownloadTargetResult* result) {
  base::RunLoop loop_runner;
  content::DownloadTargetCallback callback = base::BindOnce(
      &StoreDownloadTargetInfo, loop_runner.QuitClosure(), result);
  EXPECT_TRUE(delegate()->DetermineDownloadTarget(download_item, &callback));
  EXPECT_FALSE(callback);  // DetermineDownloadTarget() took the callback.
  loop_runner.Run();
}

void ChromeDownloadManagerDelegateTest::OnConfirmationCallbackComplete(
    DownloadTargetDeterminerDelegate::ConfirmationCallback callback,
    DownloadConfirmationResult result,
    const base::FilePath& virtual_path) {
  delegate_->OnConfirmationCallbackComplete(std::move(callback), result,
                                            virtual_path);
}

TestChromeDownloadManagerDelegate*
    ChromeDownloadManagerDelegateTest::delegate() {
  return delegate_.get();
}

content::MockDownloadManager*
    ChromeDownloadManagerDelegateTest::download_manager() {
  return download_manager_.get();
}

DownloadPrefs* ChromeDownloadManagerDelegateTest::download_prefs() {
  return delegate_->download_prefs();
}

PrefService* ChromeDownloadManagerDelegateTest::pref_service() {
  return pref_service_;
}

std::unique_ptr<download::MockDownloadItem>
ChromeDownloadManagerDelegateTest::PrepareDownloadItemForInsecureBlocking(
    const GURL& download_url,
    const absl::optional<Origin>& request_initiator,
    const absl::optional<GURL>& redirect_url) {
  std::vector<GURL> url_chain;
  if (redirect_url.has_value())
    url_chain.push_back(redirect_url.value());
  // The redirect chain always contains the final destination at the end.
  url_chain.push_back(download_url);
  std::unique_ptr<download::MockDownloadItem> download_item =
      CreateActiveDownloadItem(0);
  ON_CALL(*download_item, GetURL())
      .WillByDefault(ReturnRefOfCopy(download_url));
  ON_CALL(*download_item, GetUrlChain())
      .WillByDefault(ReturnRefOfCopy(url_chain));
  ON_CALL(*download_item, GetRequestInitiator())
      .WillByDefault(ReturnRefOfCopy(request_initiator));
  return download_item;
}

void ExpectExtensionOnlyIn(const InsecureDownloadExtensions& ext,
                           const std::string& initiator,
                           const std::string& download,
                           base::HistogramTester& tester) {
  static const char* const initiator_types[] = {
      kInsecureDownloadExtensionInitiatorUnknown,
      kInsecureDownloadExtensionInitiatorSecure,
      kInsecureDownloadExtensionInitiatorInsecure,
      kInsecureDownloadExtensionInitiatorInferredSecure,
      kInsecureDownloadExtensionInitiatorInferredInsecure,
  };

  static const char* const download_types[] = {
      kInsecureDownloadHistogramTargetSecure,
      kInsecureDownloadHistogramTargetInsecure};

  std::vector<std::string> histograms;
  for (auto* initiator_init : initiator_types) {
    for (auto* download_init : download_types) {
      histograms.push_back(
          GetDLBlockingHistogramName(initiator_init, download_init));
    }
  }

  auto expected_histogram = GetDLBlockingHistogramName(initiator, download);

  for (auto histogram : histograms) {
    if (histogram == expected_histogram) {
      tester.ExpectUniqueSample(expected_histogram, ext, 1);
    } else {
      tester.ExpectTotalCount(histogram, 0);
    }
  }
}

// Determine download target for |download_item| after enabling active content
// download blocking with the |parameters| enabled. Verify |extension|,
// |interrupt_reason| and |insecure_download_status|. Used by
// BlockedAsActiveContent_ tests.
void ChromeDownloadManagerDelegateTest::VerifyMixedContentExtensionOverride(
    DownloadItem* download_item,
    const base::FieldTrialParams& parameters,
    InsecureDownloadExtensions extension,
    download::DownloadInterruptReason interrupt_reason,
    download::DownloadItem::InsecureDownloadStatus insecure_download_status) {
  DetermineDownloadTargetResult result;
  base::HistogramTester histograms;
  base::test::ScopedFeatureList feature_list;

  feature_list.InitAndEnableFeatureWithParameters(
      features::kTreatUnsafeDownloadsAsActive, parameters);

  DetermineDownloadTarget(download_item, &result);

  EXPECT_EQ(interrupt_reason, result.interrupt_reason);
  EXPECT_EQ(insecure_download_status, result.insecure_download_status);
  histograms.ExpectUniqueSample(
      kInsecureDownloadHistogramName,
      InsecureDownloadSecurityStatus::kInitiatorSecureFileInsecure, 1);
  ExpectExtensionOnlyIn(extension, kInsecureDownloadExtensionInitiatorSecure,
                        kInsecureDownloadHistogramTargetInsecure, histograms);
}

}  // namespace

TEST_F(ChromeDownloadManagerDelegateTest, LastSavePath) {
  GURL download_url("http://example.com/foo.txt");

  std::unique_ptr<download::MockDownloadItem> save_as_download =
      CreateActiveDownloadItem(0);
  EXPECT_CALL(*save_as_download, GetURL())
      .Times(AnyNumber())
      .WillRepeatedly(ReturnRef(download_url));
  EXPECT_CALL(*save_as_download, GetTargetDisposition())
      .Times(AnyNumber())
      .WillRepeatedly(Return(DownloadItem::TARGET_DISPOSITION_PROMPT));

  std::unique_ptr<download::MockDownloadItem> automatic_download =
      CreateActiveDownloadItem(1);
  EXPECT_CALL(*automatic_download, GetURL())
      .Times(AnyNumber())
      .WillRepeatedly(ReturnRef(download_url));
  EXPECT_CALL(*automatic_download, GetTargetDisposition())
      .Times(AnyNumber())
      .WillRepeatedly(Return(DownloadItem::TARGET_DISPOSITION_OVERWRITE));

  {
    // When the prompt is displayed for the first download, the user selects a
    // path in a different directory.
    DetermineDownloadTargetResult result;
    base::FilePath expected_prompt_path(GetPathInDownloadDir("foo.txt"));
    base::FilePath user_selected_path(GetPathInDownloadDir("bar/baz.txt"));
    EXPECT_CALL(*delegate(), RequestConfirmation_(save_as_download.get(),
                                                  expected_prompt_path, _, _))
        .WillOnce(WithArg<3>(ScheduleCallback2(
            DownloadConfirmationResult::CONFIRMED, user_selected_path)));
    DetermineDownloadTarget(save_as_download.get(), &result);
    EXPECT_EQ(user_selected_path, result.target_path);
    VerifyAndClearExpectations();
  }

  {
    // The prompt path for the second download is the user selected directory
    // from the previous download.
    DetermineDownloadTargetResult result;
    base::FilePath expected_prompt_path(GetPathInDownloadDir("bar/foo.txt"));
    EXPECT_CALL(*delegate(), RequestConfirmation_(save_as_download.get(),
                                                  expected_prompt_path, _, _))
        .WillOnce(WithArg<3>(ScheduleCallback2(
            DownloadConfirmationResult::CANCELED, base::FilePath())));
    DetermineDownloadTarget(save_as_download.get(), &result);
    VerifyAndClearExpectations();
  }

  {
    // Start an automatic download. This one should get the default download
    // path since the last download path only affects Save As downloads.
    DetermineDownloadTargetResult result;
    base::FilePath expected_path(GetPathInDownloadDir("foo.txt"));
    DetermineDownloadTarget(automatic_download.get(), &result);
    EXPECT_EQ(expected_path, result.target_path);
    VerifyAndClearExpectations();
  }

  {
    // The prompt path for the next download should be the default.
    download_prefs()->SetSaveFilePath(download_prefs()->DownloadPath());
    DetermineDownloadTargetResult result;
    base::FilePath expected_prompt_path(GetPathInDownloadDir("foo.txt"));
    EXPECT_CALL(*delegate(), RequestConfirmation_(save_as_download.get(),
                                                  expected_prompt_path, _, _))
        .WillOnce(WithArg<3>(ScheduleCallback2(
            DownloadConfirmationResult::CANCELED, base::FilePath())));
    DetermineDownloadTarget(save_as_download.get(), &result);
    VerifyAndClearExpectations();
  }
}

TEST_F(ChromeDownloadManagerDelegateTest, ConflictAction) {
  const GURL kUrl("http://example.com/foo");
  const std::string kTargetDisposition("attachment; filename=\"foo.txt\"");

  std::unique_ptr<download::MockDownloadItem> download_item =
      CreateActiveDownloadItem(0);
  EXPECT_CALL(*download_item, GetURL()).WillRepeatedly(ReturnRef(kUrl));
  EXPECT_CALL(*download_item, GetContentDisposition())
      .WillRepeatedly(Return(kTargetDisposition));

  base::FilePath kExpectedPath = GetPathInDownloadDir("bar.txt");

  DetermineDownloadTargetResult result;

  EXPECT_CALL(*delegate(), MockReserveVirtualPath(_, _, _, _, _))
      .WillOnce(DoAll(SetArgPointee<4>(PathValidationResult::CONFLICT),
                      ReturnArg<1>()));
  EXPECT_CALL(*delegate(),
              RequestConfirmation_(
                  _, _, DownloadConfirmationReason::TARGET_CONFLICT, _))
      .WillOnce(WithArg<3>(ScheduleCallback2(
          DownloadConfirmationResult::CONFIRMED, kExpectedPath)));
  DetermineDownloadTarget(download_item.get(), &result);
  EXPECT_EQ(download::DownloadItem::TARGET_DISPOSITION_PROMPT,
            result.disposition);
  EXPECT_EQ(kExpectedPath, result.target_path);

  VerifyAndClearExpectations();
}

TEST_F(ChromeDownloadManagerDelegateTest, MaybeDangerousContent) {
#if BUILDFLAG(ENABLE_PLUGINS)
  content::PluginService::GetInstance()->Init();
#endif

  GURL url("http://example.com/foo");

  std::unique_ptr<download::MockDownloadItem> download_item =
      CreateActiveDownloadItem(0);
  EXPECT_CALL(*download_item, GetURL()).WillRepeatedly(ReturnRef(url));
  EXPECT_CALL(*download_item, GetTargetDisposition())
      .WillRepeatedly(Return(DownloadItem::TARGET_DISPOSITION_OVERWRITE));
  EXPECT_CALL(*delegate(), MockCheckDownloadUrl(_, _))
      .WillRepeatedly(
          Return(download::DOWNLOAD_DANGER_TYPE_MAYBE_DANGEROUS_CONTENT));

  {
    const std::string kDangerousContentDisposition(
        "attachment; filename=\"foo.swf\"");
    EXPECT_CALL(*download_item, GetContentDisposition())
        .WillRepeatedly(Return(kDangerousContentDisposition));
    DetermineDownloadTargetResult result;
    DetermineDownloadTarget(download_item.get(), &result);

    EXPECT_EQ(DownloadFileType::ALLOW_ON_USER_GESTURE,
              DownloadItemModel(download_item.get()).GetDangerLevel());
    EXPECT_EQ(download::DOWNLOAD_DANGER_TYPE_MAYBE_DANGEROUS_CONTENT,
              result.danger_type);
  }

  {
    const std::string kSafeContentDisposition(
        "attachment; filename=\"foo.txt\"");
    EXPECT_CALL(*download_item, GetContentDisposition())
        .WillRepeatedly(Return(kSafeContentDisposition));
    DetermineDownloadTargetResult result;
    DetermineDownloadTarget(download_item.get(), &result);
    EXPECT_EQ(DownloadFileType::NOT_DANGEROUS,
              DownloadItemModel(download_item.get()).GetDangerLevel());
    EXPECT_EQ(download::DOWNLOAD_DANGER_TYPE_MAYBE_DANGEROUS_CONTENT,
              result.danger_type);
  }

  {
    const std::string kModerateContentDisposition(
        "attachment; filename=\"foo.crx\"");
    EXPECT_CALL(*download_item, GetContentDisposition())
        .WillRepeatedly(Return(kModerateContentDisposition));
    DetermineDownloadTargetResult result;
    DetermineDownloadTarget(download_item.get(), &result);
    EXPECT_EQ(DownloadFileType::ALLOW_ON_USER_GESTURE,
              DownloadItemModel(download_item.get()).GetDangerLevel());
    EXPECT_EQ(download::DOWNLOAD_DANGER_TYPE_MAYBE_DANGEROUS_CONTENT,
              result.danger_type);
  }
}

TEST_F(ChromeDownloadManagerDelegateTest, BlockedByPolicy) {
  const GURL kUrl("http://example.com/foo");
  const std::string kTargetDisposition("attachment; filename=\"foo.txt\"");

  std::unique_ptr<download::MockDownloadItem> download_item =
      CreateActiveDownloadItem(0);
  EXPECT_CALL(*download_item, GetURL()).WillRepeatedly(ReturnRef(kUrl));
  EXPECT_CALL(*download_item, GetContentDisposition())
      .WillRepeatedly(Return(kTargetDisposition));

  base::FilePath kExpectedPath = GetPathInDownloadDir("bar.txt");

  DetermineDownloadTargetResult result;

  EXPECT_CALL(*delegate(), MockReserveVirtualPath(_, _, _, _, _))
      .WillOnce(DoAll(SetArgPointee<4>(PathValidationResult::CONFLICT),
                      ReturnArg<1>()));
  EXPECT_CALL(*delegate(),
              RequestConfirmation_(
                  _, _, DownloadConfirmationReason::TARGET_CONFLICT, _))
      .WillOnce(WithArg<3>(ScheduleCallback2(
          DownloadConfirmationResult::CONFIRMED, kExpectedPath)));

  pref_service()->SetInteger(
      prefs::kDownloadRestrictions,
      static_cast<int>(DownloadPrefs::DownloadRestriction::ALL_FILES));

  DetermineDownloadTarget(download_item.get(), &result);
  EXPECT_EQ(download::DOWNLOAD_INTERRUPT_REASON_FILE_BLOCKED,
            result.interrupt_reason);

  VerifyAndClearExpectations();
}

#if BUILDFLAG(IS_ANDROID)
TEST_F(ChromeDownloadManagerDelegateTest, InterceptDownloadByOfflinePages) {
  const GURL kUrl("http://example.com/foo");
  std::string mime_type = "text/html";
  bool should_intercept = delegate()->InterceptDownloadIfApplicable(
      kUrl, "", "", mime_type, "", 10, false /*is_transient*/, nullptr);
  EXPECT_TRUE(should_intercept);

  should_intercept = delegate()->InterceptDownloadIfApplicable(
      kUrl, "", "", mime_type, "", 10, true /*is_transient*/, nullptr);
  EXPECT_FALSE(should_intercept);

  should_intercept = delegate()->InterceptDownloadIfApplicable(
      kUrl, "", "attachment" /*content_disposition*/, mime_type, "", 10,
      false /*is_transient*/, nullptr);
  EXPECT_FALSE(should_intercept);
}
#endif

TEST_F(ChromeDownloadManagerDelegateTest,
       BlockedAsActiveContent_HttpsTargetOk) {
  // Active content download blocking ought not occur when the chain is secure.
  const GURL kRedirectUrl("https://example.org/");
  const GURL kSecureSilentlyBlockableFile(
      "https://example.com/foo.silently_blocked_for_testing");
  const auto kSecureOrigin = Origin::Create(GURL("https://example.org"));

#if BUILDFLAG(ENABLE_PLUGINS)
  // DownloadTargetDeterminer looks for plugin handlers if there's an
  // extension.
  content::PluginService::GetInstance()->Init();
#endif
  std::unique_ptr<download::MockDownloadItem> download_item =
      PrepareDownloadItemForInsecureBlocking(kSecureSilentlyBlockableFile,
                                             kSecureOrigin, kRedirectUrl);
  DetermineDownloadTargetResult result;
  base::test::ScopedFeatureList feature_list;
  base::HistogramTester histograms;

  feature_list.InitAndEnableFeature(features::kTreatUnsafeDownloadsAsActive);
  DetermineDownloadTarget(download_item.get(), &result);
  EXPECT_EQ(download::DOWNLOAD_INTERRUPT_REASON_NONE, result.interrupt_reason);
  histograms.ExpectUniqueSample(
      kInsecureDownloadHistogramName,
      InsecureDownloadSecurityStatus::kInitiatorSecureFileSecure, 1);
  ExpectExtensionOnlyIn(InsecureDownloadExtensions::kTest,
                        kInsecureDownloadExtensionInitiatorSecure,
                        kInsecureDownloadHistogramTargetSecure, histograms);
}

TEST_F(ChromeDownloadManagerDelegateTest, BlockedAsActiveContent_HttpPageOk) {
  // Active content download blocking ought not occur on HTTP pages.
  const GURL kHttpUrl("http://example.com/foo");
  const GURL kHttpsUrl("https://example.com/foo");
  const auto kInsecureOrigin = Origin::Create(GURL("http://example.org"));

  DetermineDownloadTargetResult result;
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kTreatUnsafeDownloadsAsActive);

  // Blocking shouldn't occur if the target is secure.
  {
    base::HistogramTester histograms;
    std::unique_ptr<download::MockDownloadItem> download_item =
        PrepareDownloadItemForInsecureBlocking(kHttpsUrl, kInsecureOrigin,
                                               absl::nullopt);
    DetermineDownloadTarget(download_item.get(), &result);

    EXPECT_EQ(download::DOWNLOAD_INTERRUPT_REASON_NONE,
              result.interrupt_reason);
    histograms.ExpectUniqueSample(
        kInsecureDownloadHistogramName,
        InsecureDownloadSecurityStatus::kInitiatorInsecureFileSecure, 1);
    ExpectExtensionOnlyIn(InsecureDownloadExtensions::kNone,
                          kInsecureDownloadExtensionInitiatorInsecure,
                          kInsecureDownloadHistogramTargetSecure, histograms);
  }

  // Nor should blocking occur if the target is insecure.
  {
    base::HistogramTester histograms;
    std::unique_ptr<download::MockDownloadItem> download_item =
        PrepareDownloadItemForInsecureBlocking(kHttpUrl, kInsecureOrigin,
                                               absl::nullopt);
    DetermineDownloadTarget(download_item.get(), &result);

    EXPECT_EQ(download::DOWNLOAD_INTERRUPT_REASON_NONE,
              result.interrupt_reason);
    histograms.ExpectUniqueSample(
        kInsecureDownloadHistogramName,
        InsecureDownloadSecurityStatus::kInitiatorInsecureFileInsecure, 1);
    ExpectExtensionOnlyIn(InsecureDownloadExtensions::kNone,
                          kInsecureDownloadExtensionInitiatorInsecure,
                          kInsecureDownloadHistogramTargetInsecure, histograms);
  }
}

TEST_F(ChromeDownloadManagerDelegateTest,
       BlockedAsActiveContent_InferredInitiatorStillBlocked) {
  // Verify context-menu-initiated downloads are blocked when warranted.
  const GURL kInsecureSilentlyBlockableFile(
      "http://example.com/foo.silently_blocked_for_testing");
  const auto kSecureOrigin = Origin::Create(GURL("https://example.org"));
#if BUILDFLAG(ENABLE_PLUGINS)
  // DownloadTargetDeterminer looks for plugin handlers when there's a file
  // extension.
  content::PluginService::GetInstance()->Init();
#endif

  std::unique_ptr<download::MockDownloadItem> download_item =
      PrepareDownloadItemForInsecureBlocking(kInsecureSilentlyBlockableFile,
                                             absl::nullopt, absl::nullopt);
  ON_CALL(*download_item, GetTabUrl())
      .WillByDefault(ReturnRefOfCopy(kSecureOrigin.GetURL()));
  ON_CALL(*download_item, GetDownloadSource())
      .WillByDefault(Return(download::DownloadSource::CONTEXT_MENU));
  DetermineDownloadTargetResult result;
  base::HistogramTester histograms;
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kTreatUnsafeDownloadsAsActive);

  DetermineDownloadTarget(download_item.get(), &result);

  EXPECT_EQ(download::DownloadItem::InsecureDownloadStatus::BLOCK,
            result.insecure_download_status);
  histograms.ExpectUniqueSample(
      kInsecureDownloadHistogramName,
      InsecureDownloadSecurityStatus::kInitiatorInferredSecureFileInsecure, 1);
  ExpectExtensionOnlyIn(InsecureDownloadExtensions::kTest,
                        kInsecureDownloadExtensionInitiatorInferredSecure,
                        kInsecureDownloadHistogramTargetInsecure, histograms);
}

TEST_F(ChromeDownloadManagerDelegateTest, BlockedAsActiveContent_HttpChain) {
  // Tests blocking unsafe active content downloads when a step in the referrer
  // chain is HTTP, using the default mime-type matching policy.
  const GURL kRedirectUrl("http://example.org/");
  const GURL kSecureSilentlyBlockableFile(
      "https://example.com/foo.silently_blocked_for_testing");
  const auto kSecureOrigin = Origin::Create(GURL("https://example.org"));

#if BUILDFLAG(ENABLE_PLUGINS)
  // DownloadTargetDeterminer looks for plugin handlers if there's an
  // extension.
  content::PluginService::GetInstance()->Init();
#endif
  std::unique_ptr<download::MockDownloadItem> download_item =
      PrepareDownloadItemForInsecureBlocking(kSecureSilentlyBlockableFile,
                                             kSecureOrigin, kRedirectUrl);
  DetermineDownloadTargetResult result;
  base::test::ScopedFeatureList feature_list;
  base::HistogramTester histograms;

  feature_list.InitAndEnableFeature(features::kTreatUnsafeDownloadsAsActive);
  DetermineDownloadTarget(download_item.get(), &result);

  EXPECT_EQ(download::DOWNLOAD_INTERRUPT_REASON_FILE_BLOCKED,
            result.interrupt_reason);
  histograms.ExpectUniqueSample(
      kInsecureDownloadHistogramName,
      InsecureDownloadSecurityStatus::kInitiatorSecureFileInsecure, 1);
  ExpectExtensionOnlyIn(InsecureDownloadExtensions::kTest,
                        kInsecureDownloadExtensionInitiatorSecure,
                        kInsecureDownloadHistogramTargetInsecure, histograms);
}

TEST_F(ChromeDownloadManagerDelegateTest,
       BlockedAsActiveContent_BenignExtensionsIgnored) {
  // Verifies benign extensions are not blocked for active content blocking.
  // As of M89, there are no 'safe' extensions, so this test only works if the
  // extension is explicitly allowlisted (and will be removed soon).
  const GURL kFooUrl("http://example.com/file.foo");
  const auto kSecureOrigin = Origin::Create(GURL("https://example.org"));

#if BUILDFLAG(ENABLE_PLUGINS)
  // DownloadTargetDeterminer looks for plugin handlers if there's an
  // extension.
  content::PluginService::GetInstance()->Init();
#endif

  std::unique_ptr<download::MockDownloadItem> foo_download_item =
      PrepareDownloadItemForInsecureBlocking(kFooUrl, kSecureOrigin,
                                             absl::nullopt);

  VerifyMixedContentExtensionOverride(
      foo_download_item.get(),
      {{"TreatSilentBlockListAsAllowlist", "true"},
       {"SilentBlockExtensionList", "foo"}},
      InsecureDownloadExtensions::kUnknown,
      download::DOWNLOAD_INTERRUPT_REASON_NONE,
      download::DownloadItem::InsecureDownloadStatus::SAFE);
}

// Verify that downloads ending in a blob URL are considered secure.
TEST_F(ChromeDownloadManagerDelegateTest,
       BlockedAsActiveContent_BlobConsideredSecure) {
  // Verifies blob URLs are not blocked for active content blocking.
  const GURL kRedirectUrl("https://example.org/");
  const GURL kFinalUrl("blob:null/xyz.foo");
  const auto kSecureOrigin = Origin::Create(GURL("https://example.org"));

  DetermineDownloadTargetResult result;
  base::test::ScopedFeatureList feature_list;
  base::HistogramTester histograms;

  std::unique_ptr<download::MockDownloadItem> download_item =
      PrepareDownloadItemForInsecureBlocking(kFinalUrl, kSecureOrigin,
                                             kRedirectUrl);

  feature_list.InitAndEnableFeature(features::kTreatUnsafeDownloadsAsActive);

#if BUILDFLAG(ENABLE_PLUGINS)
  // DownloadTargetDeterminer looks for plugin handlers if there's an
  // extension.
  content::PluginService::GetInstance()->Init();
#endif

  DetermineDownloadTarget(download_item.get(), &result);
  EXPECT_EQ(download::DOWNLOAD_INTERRUPT_REASON_NONE, result.interrupt_reason);
  EXPECT_EQ(download::DownloadItem::InsecureDownloadStatus::SAFE,
            result.insecure_download_status);
  histograms.ExpectUniqueSample(
      kInsecureDownloadHistogramName,
      InsecureDownloadSecurityStatus::kInitiatorSecureFileSecure, 1);
  ExpectExtensionOnlyIn(InsecureDownloadExtensions::kUnknown,
                        kInsecureDownloadExtensionInitiatorSecure,
                        kInsecureDownloadHistogramTargetSecure, histograms);
}

TEST_F(ChromeDownloadManagerDelegateTest, BlockedAsActiveContent_SilentBlock) {
  // Verifies that any extension is silently blocked by default, but may be
  // overridden by feature parameters.
  const GURL kFooUrl("http://example.com/file.foo");
  const auto kSecureOrigin = Origin::Create(GURL("https://example.org"));

#if BUILDFLAG(ENABLE_PLUGINS)
  // DownloadTargetDeterminer looks for plugin handlers if there's an
  // extension.
  content::PluginService::GetInstance()->Init();
#endif

  std::unique_ptr<download::MockDownloadItem> foo_download_item =
      PrepareDownloadItemForInsecureBlocking(kFooUrl, kSecureOrigin,
                                             absl::nullopt);

  // Test everything is blocked normally.
  VerifyMixedContentExtensionOverride(
      foo_download_item.get(), {{}}, InsecureDownloadExtensions::kUnknown,
      download::DOWNLOAD_INTERRUPT_REASON_FILE_BLOCKED,
      download::DownloadItem::InsecureDownloadStatus::SILENT_BLOCK);

  // An extension can punch through silent blocking if it's allowlisted.
  VerifyMixedContentExtensionOverride(
      foo_download_item.get(),
      {{"SilentBlockExtensionList", "foo"},
       {"TreatSilentBlockListAsAllowlist", "true"}},
      InsecureDownloadExtensions::kUnknown,
      download::DOWNLOAD_INTERRUPT_REASON_NONE,
      download::DownloadItem::InsecureDownloadStatus::SAFE);

  // And if that happens it can still be subject to other treatment.
  VerifyMixedContentExtensionOverride(
      foo_download_item.get(),
      {{"SilentBlockExtensionList", "foo"},
       {"TreatSilentBlockListAsAllowlist", "true"},
       {"BlockExtensionList", "foo"},
       {"TreatBlockListAsAllowlist", "false"}},
      InsecureDownloadExtensions::kUnknown,
      download::DOWNLOAD_INTERRUPT_REASON_NONE,
      download::DownloadItem::InsecureDownloadStatus::BLOCK);

  // It's also possible to punch through silent blocking by swapping
  // configuration to a blocklist, but that's not expected to be needed again.
  VerifyMixedContentExtensionOverride(
      foo_download_item.get(),
      {{"SilentBlockExtensionList", "bar"},
       {"TreatSilentBlockListAsAllowlist", "false"}},
      InsecureDownloadExtensions::kUnknown,
      download::DOWNLOAD_INTERRUPT_REASON_NONE,
      download::DownloadItem::InsecureDownloadStatus::SAFE);
}

TEST_F(ChromeDownloadManagerDelegateTest, BlockedAsActiveContent_Warn) {
  // Verifies that active mixed content download warning can still be configured
  // by feature parameter.
  const GURL kFooUrl("http://example.com/file.foo");
  const auto kSecureOrigin = Origin::Create(GURL("https://example.org"));

#if BUILDFLAG(ENABLE_PLUGINS)
  // DownloadTargetDeterminer looks for plugin handlers if there's an
  // extension.
  content::PluginService::GetInstance()->Init();
#endif

  std::unique_ptr<download::MockDownloadItem> foo_download_item =
      PrepareDownloadItemForInsecureBlocking(kFooUrl, kSecureOrigin,
                                             absl::nullopt);

  // By default, nothing is warned on since everything is silently blocked.
  VerifyMixedContentExtensionOverride(
      foo_download_item.get(), {{}}, InsecureDownloadExtensions::kUnknown,
      download::DOWNLOAD_INTERRUPT_REASON_FILE_BLOCKED,
      download::DownloadItem::InsecureDownloadStatus::SILENT_BLOCK);

  // This is true no matter what you do on the warn extension configuration.
  VerifyMixedContentExtensionOverride(
      foo_download_item.get(),
      {{"WarnExtensionList", "foo"}, {"TreatWarnListAsAllowlist", "true"}},
      InsecureDownloadExtensions::kUnknown,
      download::DOWNLOAD_INTERRUPT_REASON_FILE_BLOCKED,
      download::DownloadItem::InsecureDownloadStatus::SILENT_BLOCK);
  VerifyMixedContentExtensionOverride(
      foo_download_item.get(),
      {{"WarnExtensionList", "foo"}, {"TreatWarnListAsAllowlist", "false"}},
      InsecureDownloadExtensions::kUnknown,
      download::DOWNLOAD_INTERRUPT_REASON_FILE_BLOCKED,
      download::DownloadItem::InsecureDownloadStatus::SILENT_BLOCK);

  // To get to a warning, you need to disable other forms of blocking.
  // By default, carving out silent blocking will leave the extension as safe.
  VerifyMixedContentExtensionOverride(
      foo_download_item.get(),
      {{"SilentBlockExtensionList", "foo"},
       {"TreatSilentBlockListAsAllowlist", "true"}},
      InsecureDownloadExtensions::kUnknown,
      download::DOWNLOAD_INTERRUPT_REASON_NONE,
      download::DownloadItem::InsecureDownloadStatus::SAFE);
  // But from there you can individually warn on specific extensions.
  VerifyMixedContentExtensionOverride(
      foo_download_item.get(),
      {{"SilentBlockExtensionList", "foo"},
       {"TreatSilentBlockListAsAllowlist", "true"},
       {"WarnExtensionList", "foo"},
       {"TreatWarnListAsAllowlist", "false"}},
      InsecureDownloadExtensions::kUnknown,
      download::DOWNLOAD_INTERRUPT_REASON_NONE,
      download::DownloadItem::InsecureDownloadStatus::WARN);
  // Or warn on everything.
  VerifyMixedContentExtensionOverride(
      foo_download_item.get(),
      {{"SilentBlockExtensionList", "foo"},
       {"TreatSilentBlockListAsAllowlist", "true"},
       {"WarnExtensionList", ""},
       {"TreatWarnListAsAllowlist", "true"}},
      InsecureDownloadExtensions::kUnknown,
      download::DOWNLOAD_INTERRUPT_REASON_NONE,
      download::DownloadItem::InsecureDownloadStatus::WARN);
}

TEST_F(ChromeDownloadManagerDelegateTest, BlockedAsActiveContent_Block) {
  // Verifies that active mixed content download user-visible blocking works
  // when configured via feature parameter.
  const GURL kFooUrl("http://example.com/file.foo");
  const GURL kBarUrl("http://example.com/file.bar");
  const GURL kInsecureBlockableFile("http://example.com/foo.exe");
  const auto kSecureOrigin = Origin::Create(GURL("https://example.org"));
#if BUILDFLAG(ENABLE_PLUGINS)
  // DownloadTargetDeterminer looks for plugin handlers if there's an
  // extension.
  content::PluginService::GetInstance()->Init();
#endif

  std::unique_ptr<download::MockDownloadItem> blocked_download_item =
      PrepareDownloadItemForInsecureBlocking(kInsecureBlockableFile,
                                             kSecureOrigin, absl::nullopt);
  std::unique_ptr<download::MockDownloadItem> foo_download_item =
      PrepareDownloadItemForInsecureBlocking(kFooUrl, kSecureOrigin,
                                             absl::nullopt);
  std::unique_ptr<download::MockDownloadItem> bar_download_item =
      PrepareDownloadItemForInsecureBlocking(kBarUrl, kSecureOrigin,
                                             absl::nullopt);

  // Test that toggling the allowlist parameter impacts blocking.
  VerifyMixedContentExtensionOverride(
      blocked_download_item.get(),
      {{"TreatSilentBlockListAsAllowlist", "false"},
       {"TreatBlockListAsAllowlist", "true"}},
      InsecureDownloadExtensions::kMSExecutable,
      download::DOWNLOAD_INTERRUPT_REASON_NONE,
      download::DownloadItem::InsecureDownloadStatus::BLOCK);
  VerifyMixedContentExtensionOverride(
      foo_download_item.get(),
      {{"TreatSilentBlockListAsAllowlist", "false"},
       {"TreatBlockListAsAllowlist", "false"}},
      InsecureDownloadExtensions::kUnknown,
      download::DOWNLOAD_INTERRUPT_REASON_NONE,
      download::DownloadItem::InsecureDownloadStatus::SAFE);

  // Test extensions selected via parameter are indeed blocked.
  VerifyMixedContentExtensionOverride(
      foo_download_item.get(),
      {{"TreatSilentBlockListAsAllowlist", "false"},
       {"BlockExtensionList", "foo,bar"}},
      InsecureDownloadExtensions::kUnknown,
      download::DOWNLOAD_INTERRUPT_REASON_NONE,
      download::DownloadItem::InsecureDownloadStatus::BLOCK);
  VerifyMixedContentExtensionOverride(
      bar_download_item.get(),
      {{"TreatSilentBlockListAsAllowlist", "false"},
       {"BlockExtensionList", "foo,bar"}},
      InsecureDownloadExtensions::kUnknown,
      download::DOWNLOAD_INTERRUPT_REASON_NONE,
      download::DownloadItem::InsecureDownloadStatus::BLOCK);

  // Test that overriding extensions AND allowlisting work together.
  VerifyMixedContentExtensionOverride(
      foo_download_item.get(),
      {{"TreatSilentBlockListAsAllowlist", "false"},
       {"BlockExtensionList", "foo"},
       {"TreatBlockListAsAllowlist", "true"}},
      InsecureDownloadExtensions::kUnknown,
      download::DOWNLOAD_INTERRUPT_REASON_NONE,
      download::DownloadItem::InsecureDownloadStatus::SAFE);
  VerifyMixedContentExtensionOverride(
      bar_download_item.get(),
      {{"TreatSilentBlockListAsAllowlist", "false"},
       {"BlockExtensionList", "foo"},
       {"TreatBlockListAsAllowlist", "true"}},
      InsecureDownloadExtensions::kUnknown,
      download::DOWNLOAD_INTERRUPT_REASON_NONE,
      download::DownloadItem::InsecureDownloadStatus::BLOCK);
}

// MIXEDSCRIPT content setting only applies to Desktop.
#if !BUILDFLAG(IS_ANDROID)
TEST_F(ChromeDownloadManagerDelegateTest,
       BlockedAsActiveContent_PolicyOverride) {
  // Verifies that active mixed content download blocking is overridden by the
  // "Insecure content" site setting.
  const GURL kInsecureWarnableFile("http://example.com/foo.warn_for_testing");
  const GURL kInsecureBlockableFile("http://example.com/foo.exe");
  const GURL kInsecureSilentlyBlockableFile(
      "http://example.com/foo.silently_blocked_for_testing");
  const auto kSecureOrigin = Origin::Create(GURL("https://example.org"));

#if BUILDFLAG(ENABLE_PLUGINS)
  // DownloadTargetDeterminer looks for plugin handlers if there's an
  // extension.
  content::PluginService::GetInstance()->Init();
#endif

  std::unique_ptr<download::MockDownloadItem> warned_download_item =
      PrepareDownloadItemForInsecureBlocking(kInsecureWarnableFile,
                                             kSecureOrigin, absl::nullopt);
  std::unique_ptr<download::MockDownloadItem> blocked_download_item =
      PrepareDownloadItemForInsecureBlocking(kInsecureBlockableFile,
                                             kSecureOrigin, absl::nullopt);
  std::unique_ptr<download::MockDownloadItem> silent_blocked_download_item =
      PrepareDownloadItemForInsecureBlocking(kInsecureSilentlyBlockableFile,
                                             kSecureOrigin, absl::nullopt);

  HostContentSettingsMapFactory::GetForProfile(profile())
      ->SetContentSettingDefaultScope(kSecureOrigin.GetURL(), GURL(),
                                      ContentSettingsType::MIXEDSCRIPT,
                                      CONTENT_SETTING_ALLOW);

  VerifyMixedContentExtensionOverride(
      warned_download_item.get(), {{}}, InsecureDownloadExtensions::kTest,
      download::DOWNLOAD_INTERRUPT_REASON_NONE,
      download::DownloadItem::InsecureDownloadStatus::SAFE);
  VerifyMixedContentExtensionOverride(
      blocked_download_item.get(), {{}},
      InsecureDownloadExtensions::kMSExecutable,
      download::DOWNLOAD_INTERRUPT_REASON_NONE,
      download::DownloadItem::InsecureDownloadStatus::SAFE);
  VerifyMixedContentExtensionOverride(
      silent_blocked_download_item.get(), {{}},
      InsecureDownloadExtensions::kTest,
      download::DOWNLOAD_INTERRUPT_REASON_NONE,
      download::DownloadItem::InsecureDownloadStatus::SAFE);
}
#endif  // !BUILDFLAG(IS_ANDROID)

TEST_F(ChromeDownloadManagerDelegateTest, InsecureDownloadsBlocked) {
  const GURL kSecureUrl("https://example.net/");
  const GURL kInsecureUrl("http://example.net/");
  const GURL kBlobFile("blob:null/xyz.foo");
  const GURL kSecureFile("https://example.com/foo");
  const GURL kInsecureFile("http://example.com/foo");
  const auto kSecureOrigin = Origin::Create(GURL("https://example.org"));
  const auto kInsecureOrigin = Origin::Create(GURL("http://example.org"));

  struct {
    // The file's final URL.
    const GURL& download_url;
    // The origin that linked to or initiated the download.
    const absl::optional<url::Origin>& initiator_origin;
    // One URL that the download may have redirected through.
    const absl::optional<GURL>& redirect_url;

    const download::DownloadInterruptReason expected_interrupt_reason;
    const download::DownloadItem::InsecureDownloadStatus
        expected_insecure_download_status;
  } kTestCases[] = {
      // Secure files, with or without redirects, shouldn't be blocked.
      {kSecureFile, kSecureOrigin, kSecureUrl,
       download::DOWNLOAD_INTERRUPT_REASON_NONE,
       download::DownloadItem::InsecureDownloadStatus::SAFE},
      {kSecureFile, kSecureOrigin, absl::nullopt,
       download::DOWNLOAD_INTERRUPT_REASON_NONE,
       download::DownloadItem::InsecureDownloadStatus::SAFE},
      // Secure files initiated from insecure origins should be blocked.
      {kSecureFile, kInsecureOrigin, absl::nullopt,
       download::DOWNLOAD_INTERRUPT_REASON_NONE,
       download::DownloadItem::InsecureDownloadStatus::BLOCK},
      // Insecure files initiated from secure origins should be silently blocked
      // as mixed downloads.
      {kInsecureFile, kSecureOrigin, absl::nullopt,
       download::DOWNLOAD_INTERRUPT_REASON_FILE_BLOCKED,
       download::DownloadItem::InsecureDownloadStatus::SILENT_BLOCK},
      // Secure files initiated from secure origins but redirected insecurely
      // should be silently blocked as mixed downloads.
      {kSecureFile, kSecureOrigin, kInsecureUrl,
       download::DOWNLOAD_INTERRUPT_REASON_FILE_BLOCKED,
       download::DownloadItem::InsecureDownloadStatus::SILENT_BLOCK},
      // Blobs initiated from secure origins shouldn't be blocked.
      {kBlobFile, kSecureOrigin, kSecureUrl,
       download::DOWNLOAD_INTERRUPT_REASON_NONE,
       download::DownloadItem::InsecureDownloadStatus::SAFE},
  };

#if BUILDFLAG(ENABLE_PLUGINS)
  // DownloadTargetDeterminer looks for plugin handlers if there's an
  // extension.
  content::PluginService::GetInstance()->Init();
#endif

  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kBlockInsecureDownloads);

  for (const auto& test_case : kTestCases) {
    std::unique_ptr<download::MockDownloadItem> download_item =
        PrepareDownloadItemForInsecureBlocking(test_case.download_url,
                                               test_case.initiator_origin,
                                               test_case.redirect_url);

    DetermineDownloadTargetResult result;
    DetermineDownloadTarget(download_item.get(), &result);
    EXPECT_EQ(test_case.expected_interrupt_reason, result.interrupt_reason);
    EXPECT_EQ(test_case.expected_insecure_download_status,
              result.insecure_download_status);
  }
}

// Test that we block context-menu-initiated downloads if initiator is insecure.
TEST_F(ChromeDownloadManagerDelegateTest,
       InsecureDownloadsBlocked_InferredInitiatorBlocked) {
  const GURL kInsecureFile("http://example.com/foo");
  const auto kSecureOrigin = Origin::Create(GURL("https://example.org"));
#if BUILDFLAG(ENABLE_PLUGINS)
  // DownloadTargetDeterminer looks for plugin handlers when there's a file
  // extension.
  content::PluginService::GetInstance()->Init();
#endif

  std::unique_ptr<download::MockDownloadItem> download_item =
      PrepareDownloadItemForInsecureBlocking(kInsecureFile, absl::nullopt,
                                             absl::nullopt);
  ON_CALL(*download_item, GetTabUrl())
      .WillByDefault(ReturnRefOfCopy(kSecureOrigin.GetURL()));
  ON_CALL(*download_item, GetDownloadSource())
      .WillByDefault(Return(download::DownloadSource::CONTEXT_MENU));
  DetermineDownloadTargetResult result;
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kBlockInsecureDownloads);

  DetermineDownloadTarget(download_item.get(), &result);

  EXPECT_EQ(download::DOWNLOAD_INTERRUPT_REASON_NONE, result.interrupt_reason);
  EXPECT_EQ(download::DownloadItem::InsecureDownloadStatus::BLOCK,
            result.insecure_download_status);
}

TEST_F(ChromeDownloadManagerDelegateTest, WithoutHistoryDbNextId) {
  delegate()->GetNextId(base::BindOnce(
      &ChromeDownloadManagerDelegateTest::GetNextId, base::Unretained(this)));
  delegate()->GetNextId(base::BindOnce(
      &ChromeDownloadManagerDelegateTest::GetNextId, base::Unretained(this)));
  // When download database fails to initialize, id will be set to
  // |download::DownloadItem::kInvalidId|.
  delegate()->GetDownloadIdReceiverCallback().Run(
      download::DownloadItem::kInvalidId);
  std::vector<uint32_t> expected_ids = std::vector<uint32_t>{
      download::DownloadItem::kInvalidId, download::DownloadItem::kInvalidId};
  EXPECT_EQ(expected_ids, download_ids());
}

TEST_F(ChromeDownloadManagerDelegateTest, WithHistoryDbNextId) {
  delegate()->GetNextId(base::BindOnce(
      &ChromeDownloadManagerDelegateTest::GetNextId, base::Unretained(this)));
  delegate()->GetNextId(base::BindOnce(
      &ChromeDownloadManagerDelegateTest::GetNextId, base::Unretained(this)));
  // Simulates a valid download database with no records.
  delegate()->GetDownloadIdReceiverCallback().Run(1u);
  std::vector<uint32_t> expected_ids = std::vector<uint32_t>{1u, 2u};
  EXPECT_EQ(expected_ids, download_ids());
}

TEST_F(ChromeDownloadManagerDelegateTest, SanitizeGoogleSearchLink) {
  const GURL kGoogleSearchUrl("https://www.google.com/search?q=google");
  for (auto is_safe_search_enabled : {true, false}) {
    auto* prefs = profile()->GetPrefs();
    prefs->SetBoolean(prefs::kForceGoogleSafeSearch, is_safe_search_enabled);

    download::DownloadUrlParameters params(kGoogleSearchUrl,
                                           TRAFFIC_ANNOTATION_FOR_TESTS);

    delegate()->SanitizeDownloadParameters(&params);
    GURL expected_url = kGoogleSearchUrl;
    if (is_safe_search_enabled)
      safe_search_util::ForceGoogleSafeSearch(expected_url, &expected_url);
    EXPECT_EQ(params.url(), expected_url);
  }
}

#if !BUILDFLAG(IS_ANDROID)
namespace {
// Verify the file picker confirmation result matches |expected_result|. Run
// |completion_closure| on completion.
void VerifyFilePickerConfirmation(DownloadConfirmationResult expected_result,
                                  base::RepeatingClosure completion_closure,
                                  DownloadConfirmationResult result,
                                  const base::FilePath& virtual_path) {
  ASSERT_EQ(result, expected_result);
  std::move(completion_closure).Run();
}
}  // namespace

// Test that it is fine to remove a download before its file picker is being
// shown.
TEST_F(ChromeDownloadManagerDelegateTest,
       RemovingDownloadBeforeShowingFilePicker) {
  GURL download_url("http://example.com/foo.txt");

  std::unique_ptr<download::MockDownloadItem> download1 =
      CreateActiveDownloadItem(0);
  EXPECT_CALL(*download1, GetURL())
      .Times(AnyNumber())
      .WillRepeatedly(ReturnRef(download_url));
  EXPECT_CALL(*download1, GetTargetDisposition())
      .Times(AnyNumber())
      .WillRepeatedly(Return(DownloadItem::TARGET_DISPOSITION_PROMPT));

  std::unique_ptr<download::MockDownloadItem> download2 =
      CreateActiveDownloadItem(1);
  EXPECT_CALL(*download2, GetURL())
      .Times(AnyNumber())
      .WillRepeatedly(ReturnRef(download_url));
  EXPECT_CALL(*download2, GetTargetDisposition())
      .Times(AnyNumber())
      .WillRepeatedly(Return(DownloadItem::TARGET_DISPOSITION_PROMPT));

  EXPECT_CALL(*delegate(), RequestConfirmation_(_, _, _, _))
      .WillRepeatedly(Invoke(
          delegate(),
          &TestChromeDownloadManagerDelegate::RequestConfirmationConcrete));

  base::FilePath expected_prompt_path(GetPathInDownloadDir("foo.txt"));
  delegate()->RequestConfirmation(download1.get(), expected_prompt_path,
                                  DownloadConfirmationReason::NAME_TOO_LONG,
                                  base::DoNothing());
  base::RunLoop run_loop;
  // Verify that the second download's file picker will be canceled, because
  // it will be removed from the DownloadManager.
  delegate()->RequestConfirmation(
      download2.get(), expected_prompt_path,
      DownloadConfirmationReason::NAME_TOO_LONG,
      base::BindRepeating(&VerifyFilePickerConfirmation,
                          DownloadConfirmationResult::CANCELED,
                          run_loop.QuitClosure()));
  // Make the manager no longer return the 2nd download as if the latter is
  // removed.
  EXPECT_CALL(*download_manager(), GetDownloadByGuid(download2->GetGuid()))
      .WillRepeatedly(Return(nullptr));
  // Complete the first download, so the second download's file picker should
  // be handled. And since the second download is removed from the manager,
  // the file picker should be canceled.
  OnConfirmationCallbackComplete(base::DoNothing(),
                                 DownloadConfirmationResult::CONFIRMED,
                                 expected_prompt_path);

  run_loop.Run();
}
#endif  // BUILDFLAG(IS_ANDROID)

#if !BUILDFLAG(IS_ANDROID)
#if !BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(ChromeDownloadManagerDelegateTest, ScheduleCancelForEphemeralWarning) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {safe_browsing::kDownloadBubble, safe_browsing::kDownloadBubbleV2}, {});

  std::unique_ptr<download::MockDownloadItem> download_item =
      CreateActiveDownloadItem(0);
  EXPECT_CALL(*download_item, GetDangerType())
      .WillRepeatedly(Return(download::DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE));

  delegate()->ScheduleCancelForEphemeralWarning(download_item->GetGuid());

  // Cancel should not be called until threshold is reached
  EXPECT_CALL(*download_item, Cancel(false)).Times(0);
  task_environment()->AdvanceClock(base::Minutes(59));
  base::RunLoop().RunUntilIdle();
  EXPECT_CALL(*download_item, Cancel(false)).Times(1);
  task_environment()->AdvanceClock(base::Hours(1));
  task_environment()->RunUntilIdle();
}

TEST_F(ChromeDownloadManagerDelegateTest,
       ScheduleCancelForEphemeralWarning_DownloadKept) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {safe_browsing::kDownloadBubble, safe_browsing::kDownloadBubbleV2}, {});
  std::unique_ptr<download::MockDownloadItem> download_item =
      CreateActiveDownloadItem(0);
  EXPECT_CALL(*download_item, GetDangerType())
      .WillRepeatedly(Return(download::DOWNLOAD_DANGER_TYPE_USER_VALIDATED));

  delegate()->ScheduleCancelForEphemeralWarning(download_item->GetGuid());

  // Cancel should not be called until threshold is reached
  EXPECT_CALL(*download_item, Cancel(false)).Times(0);
  task_environment()->AdvanceClock(base::Hours(1));
  base::RunLoop().RunUntilIdle();
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

TEST_F(ChromeDownloadManagerDelegateTest, CancelAllEphemeralWarnings) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {safe_browsing::kDownloadBubble, safe_browsing::kDownloadBubbleV2}, {});
  std::vector<download::DownloadItem*> items;
  auto safe_item = CreateActiveDownloadItem(0);
  EXPECT_CALL(*safe_item, GetDangerType())
      .WillRepeatedly(Return(download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS));
  auto dangerous_item = CreateActiveDownloadItem(0);
  EXPECT_CALL(*dangerous_item, GetDangerType())
      .WillRepeatedly(Return(download::DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE));
  auto canceled_item = CreateActiveDownloadItem(0);
  EXPECT_CALL(*canceled_item, GetDangerType())
      .WillRepeatedly(Return(download::DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE));
  EXPECT_CALL(*canceled_item, GetState())
      .WillRepeatedly(Return(DownloadItem::CANCELLED));
  items.push_back(safe_item.get());
  items.push_back(dangerous_item.get());
  items.push_back(canceled_item.get());
  EXPECT_CALL(*download_manager(), GetAllDownloads(_))
      .WillRepeatedly(SetArgPointee<0>(items));

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // No cancels should go through for Ash.
  EXPECT_CALL(*safe_item, Cancel(false)).Times(0);
  EXPECT_CALL(*dangerous_item, Cancel(false)).Times(0);
  EXPECT_CALL(*canceled_item, Cancel(false)).Times(0);
#else
  EXPECT_CALL(*safe_item, Cancel(false)).Times(0);
  EXPECT_CALL(*dangerous_item, Cancel(false)).Times(1);
  EXPECT_CALL(*canceled_item, Cancel(false)).Times(0);
#endif

  delegate()->CancelAllEphemeralWarnings();
}
#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(FULL_SAFE_BROWSING)
namespace {

struct SafeBrowsingTestParameters {
  download::DownloadDangerType initial_danger_type;
  DownloadFileType::DangerLevel initial_danger_level;
  safe_browsing::DownloadCheckResult verdict;
  DownloadPrefs::DownloadRestriction download_restriction;

  download::DownloadDangerType expected_danger_type;
  bool blocked;
};

class TestDownloadProtectionService
    : public safe_browsing::DownloadProtectionService {
 public:
  TestDownloadProtectionService() : DownloadProtectionService(nullptr) {}

  void CheckClientDownload(
      DownloadItem* download_item,
      safe_browsing::CheckDownloadRepeatingCallback callback) override {
    std::move(callback).Run(MockCheckClientDownload());
  }
  MOCK_METHOD0(MockCheckClientDownload, safe_browsing::DownloadCheckResult());
};

class ChromeDownloadManagerDelegateTestWithSafeBrowsing
    : public ChromeDownloadManagerDelegateTest,
      public ::testing::WithParamInterface<SafeBrowsingTestParameters> {
 public:
  void SetUp() override;
  void TearDown() override;
  TestDownloadProtectionService* download_protection_service() {
    return test_download_protection_service_.get();
  }

 private:
  std::unique_ptr<TestDownloadProtectionService>
      test_download_protection_service_;
};

void ChromeDownloadManagerDelegateTestWithSafeBrowsing::SetUp() {
  ChromeDownloadManagerDelegateTest::SetUp();
  test_download_protection_service_ =
      std::make_unique<::testing::StrictMock<TestDownloadProtectionService>>();
  ON_CALL(*delegate(), GetDownloadProtectionService())
      .WillByDefault(Return(test_download_protection_service_.get()));
}

void ChromeDownloadManagerDelegateTestWithSafeBrowsing::TearDown() {
  test_download_protection_service_.reset();
  ChromeDownloadManagerDelegateTest::TearDown();
}

const SafeBrowsingTestParameters kSafeBrowsingTestCases[] = {
    // SAFE verdict for a safe file.
    {download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
     DownloadFileType::NOT_DANGEROUS, safe_browsing::DownloadCheckResult::SAFE,
     DownloadPrefs::DownloadRestriction::NONE,

     download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
     /*blocked=*/false},

    // UNKNOWN verdict for a safe file.
    {download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
     DownloadFileType::NOT_DANGEROUS,
     safe_browsing::DownloadCheckResult::UNKNOWN,
     DownloadPrefs::DownloadRestriction::NONE,

     download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
     /*blocked=*/false},

    // DANGEROUS verdict for a safe file.
    {download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
     DownloadFileType::NOT_DANGEROUS,
     safe_browsing::DownloadCheckResult::DANGEROUS,
     DownloadPrefs::DownloadRestriction::NONE,

     download::DOWNLOAD_DANGER_TYPE_DANGEROUS_CONTENT,
     /*blocked=*/false},

    // UNCOMMON verdict for a safe file.
    {download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
     DownloadFileType::NOT_DANGEROUS,
     safe_browsing::DownloadCheckResult::UNCOMMON,
     DownloadPrefs::DownloadRestriction::NONE,

     download::DOWNLOAD_DANGER_TYPE_UNCOMMON_CONTENT,
     /*blocked=*/false},

    // POTENTIALLY_UNWANTED verdict for a safe file.
    {download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
     DownloadFileType::NOT_DANGEROUS,
     safe_browsing::DownloadCheckResult::POTENTIALLY_UNWANTED,
     DownloadPrefs::DownloadRestriction::NONE,

     download::DOWNLOAD_DANGER_TYPE_POTENTIALLY_UNWANTED,
     /*blocked=*/false},

    // SAFE verdict for a potentially dangerous file.
    {download::DOWNLOAD_DANGER_TYPE_MAYBE_DANGEROUS_CONTENT,
     DownloadFileType::ALLOW_ON_USER_GESTURE,
     safe_browsing::DownloadCheckResult::SAFE,
     DownloadPrefs::DownloadRestriction::NONE,

     download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
     /*blocked=*/false},

    // UNKNOWN verdict for a potentially dangerous file.
    {download::DOWNLOAD_DANGER_TYPE_MAYBE_DANGEROUS_CONTENT,
     DownloadFileType::ALLOW_ON_USER_GESTURE,
     safe_browsing::DownloadCheckResult::UNKNOWN,
     DownloadPrefs::DownloadRestriction::NONE,

     download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
     /*blocked=*/false},

    // UNKNOWN verdict for a potentially dangerous file blocked by policy.
    {download::DOWNLOAD_DANGER_TYPE_MAYBE_DANGEROUS_CONTENT,
     DownloadFileType::ALLOW_ON_USER_GESTURE,
     safe_browsing::DownloadCheckResult::UNKNOWN,
     DownloadPrefs::DownloadRestriction::DANGEROUS_FILES,

     download::DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE,
     /*blocked=*/true},

    // UNKNOWN verdict for a potentially dangerous file not blocked by policy.
    {download::DOWNLOAD_DANGER_TYPE_MAYBE_DANGEROUS_CONTENT,
     DownloadFileType::ALLOW_ON_USER_GESTURE,
     safe_browsing::DownloadCheckResult::UNKNOWN,
     DownloadPrefs::DownloadRestriction::MALICIOUS_FILES,

     download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
     /*blocked=*/false},

    // DANGEROUS verdict for a potentially dangerous file.
    {download::DOWNLOAD_DANGER_TYPE_MAYBE_DANGEROUS_CONTENT,
     DownloadFileType::ALLOW_ON_USER_GESTURE,
     safe_browsing::DownloadCheckResult::DANGEROUS,
     DownloadPrefs::DownloadRestriction::NONE,

     download::DOWNLOAD_DANGER_TYPE_DANGEROUS_CONTENT,
     /*blocked=*/false},

    // DANGEROUS verdict for a potentially dangerous file block by policy.
    {download::DOWNLOAD_DANGER_TYPE_MAYBE_DANGEROUS_CONTENT,
     DownloadFileType::ALLOW_ON_USER_GESTURE,
     safe_browsing::DownloadCheckResult::DANGEROUS,
     DownloadPrefs::DownloadRestriction::MALICIOUS_FILES,

     download::DOWNLOAD_DANGER_TYPE_DANGEROUS_CONTENT,
     /*blocked=*/true},

    // DANGEROUS verdict for a potentially dangerous file block by policy.
    {download::DOWNLOAD_DANGER_TYPE_MAYBE_DANGEROUS_CONTENT,
     DownloadFileType::ALLOW_ON_USER_GESTURE,
     safe_browsing::DownloadCheckResult::DANGEROUS,
     DownloadPrefs::DownloadRestriction::MALICIOUS_FILES,

     download::DOWNLOAD_DANGER_TYPE_DANGEROUS_HOST,
     /*blocked=*/true},

    // DANGEROUS verdict for a potentially dangerous file block by policy.
    {download::DOWNLOAD_DANGER_TYPE_MAYBE_DANGEROUS_CONTENT,
     DownloadFileType::ALLOW_ON_USER_GESTURE,
     safe_browsing::DownloadCheckResult::DANGEROUS,
     DownloadPrefs::DownloadRestriction::MALICIOUS_FILES,

     download::DOWNLOAD_DANGER_TYPE_DANGEROUS_URL,
     /*blocked=*/true},

    // UNCOMMON verdict for a potentially dangerous file.
    {download::DOWNLOAD_DANGER_TYPE_MAYBE_DANGEROUS_CONTENT,
     DownloadFileType::ALLOW_ON_USER_GESTURE,
     safe_browsing::DownloadCheckResult::UNCOMMON,
     DownloadPrefs::DownloadRestriction::NONE,

     download::DOWNLOAD_DANGER_TYPE_UNCOMMON_CONTENT,
     /*blocked=*/false},

    // POTENTIALLY_UNWANTED verdict for a potentially dangerous file.
    {download::DOWNLOAD_DANGER_TYPE_MAYBE_DANGEROUS_CONTENT,
     DownloadFileType::ALLOW_ON_USER_GESTURE,
     safe_browsing::DownloadCheckResult::POTENTIALLY_UNWANTED,
     DownloadPrefs::DownloadRestriction::NONE,

     download::DOWNLOAD_DANGER_TYPE_POTENTIALLY_UNWANTED,
     /*blocked=*/false},

    // POTENTIALLY_UNWANTED verdict for a potentially dangerous file, blocked by
    // policy.
    {download::DOWNLOAD_DANGER_TYPE_MAYBE_DANGEROUS_CONTENT,
     DownloadFileType::ALLOW_ON_USER_GESTURE,
     safe_browsing::DownloadCheckResult::POTENTIALLY_UNWANTED,
     DownloadPrefs::DownloadRestriction::POTENTIALLY_DANGEROUS_FILES,

     download::DOWNLOAD_DANGER_TYPE_POTENTIALLY_UNWANTED,
     /*blocked=*/true},

    // POTENTIALLY_UNWANTED verdict for a potentially dangerous file, blocked by
    // policy.
    {download::DOWNLOAD_DANGER_TYPE_MAYBE_DANGEROUS_CONTENT,
     DownloadFileType::ALLOW_ON_USER_GESTURE,
     safe_browsing::DownloadCheckResult::POTENTIALLY_UNWANTED,
     DownloadPrefs::DownloadRestriction::DANGEROUS_FILES,

     download::DOWNLOAD_DANGER_TYPE_POTENTIALLY_UNWANTED,
     /*blocked=*/true},

    // POTENTIALLY_UNWANTED verdict for a potentially dangerous file, not
    // blocked by policy.
    {download::DOWNLOAD_DANGER_TYPE_MAYBE_DANGEROUS_CONTENT,
     DownloadFileType::ALLOW_ON_USER_GESTURE,
     safe_browsing::DownloadCheckResult::POTENTIALLY_UNWANTED,
     DownloadPrefs::DownloadRestriction::MALICIOUS_FILES,

     download::DOWNLOAD_DANGER_TYPE_POTENTIALLY_UNWANTED,
     /*blocked=*/false},

    // SAFE verdict for a dangerous file.
    {download::DOWNLOAD_DANGER_TYPE_MAYBE_DANGEROUS_CONTENT,
     DownloadFileType::DANGEROUS, safe_browsing::DownloadCheckResult::SAFE,
     DownloadPrefs::DownloadRestriction::NONE,

     download::DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE,
     /*blocked=*/false},

    // UNKNOWN verdict for a dangerous file.
    {download::DOWNLOAD_DANGER_TYPE_MAYBE_DANGEROUS_CONTENT,
     DownloadFileType::DANGEROUS, safe_browsing::DownloadCheckResult::UNKNOWN,
     DownloadPrefs::DownloadRestriction::NONE,

     download::DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE,
     /*blocked=*/false},

    // DANGEROUS verdict for a dangerous file.
    {download::DOWNLOAD_DANGER_TYPE_MAYBE_DANGEROUS_CONTENT,
     DownloadFileType::DANGEROUS, safe_browsing::DownloadCheckResult::DANGEROUS,
     DownloadPrefs::DownloadRestriction::NONE,

     download::DOWNLOAD_DANGER_TYPE_DANGEROUS_CONTENT,
     /*blocked=*/false},

    // UNCOMMON verdict for a dangerous file.
    {download::DOWNLOAD_DANGER_TYPE_MAYBE_DANGEROUS_CONTENT,
     DownloadFileType::DANGEROUS, safe_browsing::DownloadCheckResult::UNCOMMON,
     DownloadPrefs::DownloadRestriction::NONE,

     download::DOWNLOAD_DANGER_TYPE_UNCOMMON_CONTENT,
     /*blocked=*/false},

    // POTENTIALLY_UNWANTED verdict for a dangerous file.
    {download::DOWNLOAD_DANGER_TYPE_MAYBE_DANGEROUS_CONTENT,
     DownloadFileType::DANGEROUS,
     safe_browsing::DownloadCheckResult::POTENTIALLY_UNWANTED,
     DownloadPrefs::DownloadRestriction::NONE,

     download::DOWNLOAD_DANGER_TYPE_POTENTIALLY_UNWANTED,
     /*blocked=*/false},
};

INSTANTIATE_TEST_SUITE_P(_,
                         ChromeDownloadManagerDelegateTestWithSafeBrowsing,
                         ::testing::ValuesIn(kSafeBrowsingTestCases));

}  // namespace

TEST_P(ChromeDownloadManagerDelegateTestWithSafeBrowsing, CheckClientDownload) {
  const SafeBrowsingTestParameters& kParameters = GetParam();

  std::unique_ptr<download::MockDownloadItem> download_item =
      CreateActiveDownloadItem(0);
  EXPECT_CALL(*delegate(), GetDownloadProtectionService());
  EXPECT_CALL(*download_protection_service(), MockCheckClientDownload())
      .WillOnce(Return(kParameters.verdict));
  EXPECT_CALL(*download_item, GetDangerType())
      .WillRepeatedly(Return(kParameters.initial_danger_type));
  EXPECT_CALL(*download_item, RequireSafetyChecks())
      .WillRepeatedly(Return(true));

  if (kParameters.initial_danger_level != DownloadFileType::NOT_DANGEROUS) {
    DownloadItemModel(download_item.get())
        .SetDangerLevel(kParameters.initial_danger_level);
  }

  if (kParameters.blocked) {
    EXPECT_CALL(*download_item,
                OnContentCheckCompleted(
                    // Specifying a dangerous type here would take precedence
                    // over the blocking of the file.
                    download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
                    download::DOWNLOAD_INTERRUPT_REASON_FILE_BLOCKED));
  } else {
    EXPECT_CALL(*download_item, OnContentCheckCompleted(
                                    kParameters.expected_danger_type,
                                    download::DOWNLOAD_INTERRUPT_REASON_NONE));
  }

  pref_service()->SetInteger(
      prefs::kDownloadRestrictions,
      static_cast<int>(kParameters.download_restriction));

  base::RunLoop run_loop;
  ASSERT_FALSE(delegate()->ShouldCompleteDownload(download_item.get(),
                                                  run_loop.QuitClosure()));
  run_loop.Run();
}

TEST_P(ChromeDownloadManagerDelegateTestWithSafeBrowsing,
       SkipCheckClientDownload) {
  const SafeBrowsingTestParameters& kParameters = GetParam();

  std::unique_ptr<download::MockDownloadItem> download_item =
      CreateActiveDownloadItem(0);
  EXPECT_CALL(*download_item, GetDangerType())
      .WillRepeatedly(Return(kParameters.initial_danger_type));
  EXPECT_CALL(*download_item, RequireSafetyChecks())
      .WillRepeatedly(Return(false));

  base::RunLoop run_loop;
  ASSERT_TRUE(delegate()->ShouldCompleteDownload(download_item.get(),
                                                 run_loop.QuitClosure()));
}

TEST_F(ChromeDownloadManagerDelegateTestWithSafeBrowsing,
       TrustedSourcesPolicyNotTrusted) {
  GURL download_url("http://untrusted.com/best-download-ever.exe");
  pref_service()->SetBoolean(prefs::kSafeBrowsingForTrustedSourcesEnabled,
                             false);
  std::unique_ptr<download::MockDownloadItem> download_item =
      CreateActiveDownloadItem(0);
  EXPECT_CALL(*download_item, GetURL()).WillRepeatedly(ReturnRef(download_url));
  EXPECT_CALL(*download_item, RequireSafetyChecks())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*delegate(), GetDownloadProtectionService());
  EXPECT_CALL(*download_protection_service(), MockCheckClientDownload())
      .WillOnce(Return(safe_browsing::DownloadCheckResult::SAFE));
  EXPECT_CALL(*download_item, GetDangerType())
      .WillRepeatedly(Return(download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS));

  base::RunLoop run_loop;
  ASSERT_FALSE(delegate()->ShouldCompleteDownload(download_item.get(),
                                                  run_loop.QuitClosure()));
  run_loop.Run();
}

#if !BUILDFLAG(IS_WIN)
// TODO(crbug.com/739204) Add a Windows version of this test.
TEST_F(ChromeDownloadManagerDelegateTestWithSafeBrowsing,
       TrustedSourcesPolicyTrusted) {
  base::CommandLine* command_line(base::CommandLine::ForCurrentProcess());
  DCHECK(command_line);
  command_line->AppendSwitchASCII(switches::kTrustedDownloadSources,
                                  "trusted.com");
  GURL download_url("http://trusted.com/best-download-ever.exe");
  pref_service()->SetBoolean(prefs::kSafeBrowsingForTrustedSourcesEnabled,
                             false);
  std::unique_ptr<download::MockDownloadItem> download_item =
      CreateActiveDownloadItem(0);
  EXPECT_CALL(*download_item, GetURL()).WillRepeatedly(ReturnRef(download_url));
  EXPECT_CALL(*download_item, RequireSafetyChecks())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*delegate(), GetDownloadProtectionService()).Times(0);
  EXPECT_TRUE(delegate()->ShouldCompleteDownload(download_item.get(),
                                                 base::OnceClosure()));
}
#endif  // !BUILDFLAG(IS_WIN)
#endif  // FULL_SAFE_BROWSING

#if BUILDFLAG(IS_ANDROID)

namespace {

class AndroidDownloadInfobarCounter
    : public infobars::InfoBarManager::Observer {
 public:
  explicit AndroidDownloadInfobarCounter(content::WebContents* web_contents)
      : infobar_manager_(
            infobars::ContentInfoBarManager::FromWebContents(web_contents)) {
    infobar_manager_->AddObserver(this);
  }

  ~AndroidDownloadInfobarCounter() override {
    infobar_manager_->RemoveObserver(this);
  }

  int CheckAndResetInfobarCount() {
    int count = infobar_count_;
    infobar_count_ = 0;
    return count;
  }

 private:
  void OnInfoBarAdded(infobars::InfoBar* infobar) override {
    if (infobar->delegate()->GetIdentifier() ==
        infobars::InfoBarDelegate::DUPLICATE_DOWNLOAD_INFOBAR_DELEGATE_ANDROID)
      ++infobar_count_;
    infobar->delegate()->InfoBarDismissed();
    infobar->RemoveSelf();
  }

  raw_ptr<infobars::ContentInfoBarManager> infobar_manager_;
  int infobar_count_ = 0;
};

class TestDownloadDialogBridge : public DownloadDialogBridge {
 public:
  TestDownloadDialogBridge() = default;

  TestDownloadDialogBridge(const TestDownloadDialogBridge&) = delete;
  TestDownloadDialogBridge& operator=(const TestDownloadDialogBridge&) = delete;

  // DownloadDialogBridge implementation.
  void ShowDialog(gfx::NativeWindow native_window,
                  int64_t total_bytes,
                  ConnectionType connection_type,
                  DownloadLocationDialogType dialog_type,
                  const base::FilePath& suggested_path,
                  bool is_incognito,
                  DownloadDialogBridge::DialogCallback callback) override {
    dialog_shown_count_++;
    dialog_type_ = dialog_type;
    if (callback) {
      DownloadDialogResult result;
      result.location_result = DownloadLocationDialogResult::USER_CANCELED;
      std::move(callback).Run(std::move(result));
    }
  }

  // Returns the number of times ShowDialog has been called.
  int GetDialogShownCount() { return dialog_shown_count_; }

  // Returns the type of the last dialog that was called to be shown.
  DownloadLocationDialogType GetDialogType() { return dialog_type_; }

  // Resets the stored information.
  void ResetStoredVariables() {
    dialog_shown_count_ = 0;
    dialog_type_ = DownloadLocationDialogType::NO_DIALOG;
  }

 private:
  int dialog_shown_count_;
  DownloadLocationDialogType dialog_type_;
  DownloadTargetDeterminerDelegate::ConfirmationCallback
      dialog_complete_callback_;
};

}  // namespace

TEST_F(ChromeDownloadManagerDelegateTest, RequestConfirmation_Android) {
  DeleteContents();
  SetContents(CreateTestWebContents());

  base::test::ScopedFeatureList scoped_list;
  profile()->GetTestingPrefService()->SetInteger(
      prefs::kPromptForDownloadAndroid,
      static_cast<int>(DownloadPromptStatus::SHOW_PREFERENCE));

  enum class WebContents { AVAILABLE, NONE };
  enum class ExpectPath { FULL, EMPTY };
  struct {
    DownloadConfirmationReason confirmation_reason;
    DownloadConfirmationResult expected_result;
    WebContents web_contents;
    DownloadLocationDialogType dialog_type;
    ExpectPath path;
  } kTestCases[] = {
      // SAVE_AS
      {DownloadConfirmationReason::SAVE_AS,
       DownloadConfirmationResult::CONTINUE_WITHOUT_CONFIRMATION,
       WebContents::AVAILABLE, DownloadLocationDialogType::NO_DIALOG,
       ExpectPath::FULL},
      {DownloadConfirmationReason::SAVE_AS,
       DownloadConfirmationResult::CONTINUE_WITHOUT_CONFIRMATION,
       WebContents::NONE, DownloadLocationDialogType::NO_DIALOG,
       ExpectPath::FULL},

      // !web_contents
      {DownloadConfirmationReason::PREFERENCE,
       DownloadConfirmationResult::CONTINUE_WITHOUT_CONFIRMATION,
       WebContents::NONE, DownloadLocationDialogType::NO_DIALOG,
       ExpectPath::FULL},
      {DownloadConfirmationReason::TARGET_CONFLICT,
       DownloadConfirmationResult::CANCELED, WebContents::NONE,
       DownloadLocationDialogType::NO_DIALOG, ExpectPath::EMPTY},
      {DownloadConfirmationReason::TARGET_NO_SPACE,
       DownloadConfirmationResult::CANCELED, WebContents::NONE,
       DownloadLocationDialogType::NO_DIALOG, ExpectPath::EMPTY},
      {DownloadConfirmationReason::TARGET_PATH_NOT_WRITEABLE,
       DownloadConfirmationResult::CANCELED, WebContents::NONE,
       DownloadLocationDialogType::NO_DIALOG, ExpectPath::EMPTY},
      {DownloadConfirmationReason::NAME_TOO_LONG,
       DownloadConfirmationResult::CANCELED, WebContents::NONE,
       DownloadLocationDialogType::NO_DIALOG, ExpectPath::EMPTY},

      // UNEXPECTED
      {DownloadConfirmationReason::UNEXPECTED,
       DownloadConfirmationResult::CANCELED, WebContents::AVAILABLE,
       DownloadLocationDialogType::NO_DIALOG, ExpectPath::EMPTY},
      {DownloadConfirmationReason::UNEXPECTED,
       DownloadConfirmationResult::CANCELED, WebContents::NONE,
       DownloadLocationDialogType::NO_DIALOG, ExpectPath::EMPTY},

      // TARGET_CONFLICT
      {DownloadConfirmationReason::TARGET_CONFLICT,
       DownloadConfirmationResult::CANCELED, WebContents::AVAILABLE,
       DownloadLocationDialogType::NAME_CONFLICT, ExpectPath::EMPTY},

      // Other error dialogs
      {DownloadConfirmationReason::TARGET_NO_SPACE,
       DownloadConfirmationResult::CANCELED, WebContents::AVAILABLE,
       DownloadLocationDialogType::LOCATION_FULL, ExpectPath::EMPTY},
      {DownloadConfirmationReason::TARGET_PATH_NOT_WRITEABLE,
       DownloadConfirmationResult::CANCELED, WebContents::AVAILABLE,
       DownloadLocationDialogType::LOCATION_NOT_FOUND, ExpectPath::EMPTY},
      {DownloadConfirmationReason::NAME_TOO_LONG,
       DownloadConfirmationResult::CANCELED, WebContents::AVAILABLE,
       DownloadLocationDialogType::NAME_TOO_LONG, ExpectPath::EMPTY},
  };

  EXPECT_CALL(*delegate(), RequestConfirmation_(_, _, _, _))
      .WillRepeatedly(Invoke(
          delegate(),
          &TestChromeDownloadManagerDelegate::RequestConfirmationConcrete));
  base::FilePath fake_path = GetPathInDownloadDir(FILE_PATH_LITERAL("foo.txt"));
  GURL url("http://example.com");
  TestDownloadDialogBridge* dialog_bridge = new TestDownloadDialogBridge();
  delegate()->SetDownloadDialogBridgeForTesting(
      static_cast<DownloadDialogBridge*>(dialog_bridge));

  for (const auto& test_case : kTestCases) {
    std::unique_ptr<download::MockDownloadItem> download_item =
        CreateActiveDownloadItem(1);
    content::DownloadItemUtils::AttachInfoForTesting(
        download_item.get(), profile(),
        test_case.web_contents == WebContents::AVAILABLE ? web_contents()
                                                         : nullptr);
    EXPECT_CALL(*download_item, GetURL()).WillRepeatedly(ReturnRef(url));
    dialog_bridge->ResetStoredVariables();

    base::RunLoop loop;
    const auto callback = base::BindRepeating(
        [](const base::RepeatingClosure& quit_closure,
           DownloadConfirmationResult expected_result,
           const base::FilePath& expected_path,
           DownloadConfirmationResult actual_result,
           const base::FilePath& actual_path) {
          EXPECT_EQ(expected_result, actual_result);
          EXPECT_EQ(expected_path, actual_path);
          quit_closure.Run();
        },
        loop.QuitClosure(), test_case.expected_result,
        test_case.path == ExpectPath::FULL ? fake_path : base::FilePath());
    delegate()->RequestConfirmation(download_item.get(), fake_path,
                                    test_case.confirmation_reason, callback);
    loop.Run();

    EXPECT_EQ(
        test_case.dialog_type != DownloadLocationDialogType::NO_DIALOG ? 1 : 0,
        dialog_bridge->GetDialogShownCount());
    EXPECT_EQ(test_case.dialog_type, dialog_bridge->GetDialogType());

    EXPECT_CALL(*download_item, GetState())
        .WillRepeatedly(Return(DownloadItem::COMPLETE));
    download_item->NotifyObserversDownloadUpdated();
  }
}
#endif  // BUILDFLAG(IS_ANDROID)
