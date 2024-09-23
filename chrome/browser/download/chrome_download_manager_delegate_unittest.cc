// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/chrome_download_manager_delegate.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/uuid.h"
#include "build/build_config.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/download/download_core_service_factory.h"
#include "chrome/browser/download/download_core_service_impl.h"
#include "chrome/browser/download/download_item_model.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/download/insecure_download_blocking.h"
#include "chrome/browser/tab_group_sync/tab_group_sync_tab_state.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
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
#include "components/download/public/common/download_stats.h"
#include "components/download/public/common/download_target_info.h"
#include "components/download/public/common/mock_download_item.h"
#include "components/enterprise/buildflags/buildflags.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/buildflags.h"
#include "components/safe_browsing/content/common/file_type_policies_test_util.h"
#include "components/safe_search_api/safe_search_util.h"
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
#include "ui/shell_dialogs/selected_file_info.h"
#include "url/origin.h"

#if BUILDFLAG(FULL_SAFE_BROWSING)
#include "chrome/browser/policy/dm_token_utils.h"
#include "chrome/browser/safe_browsing/download_protection/download_protection_service.h"
#include "chrome/browser/safe_browsing/download_protection/download_protection_util.h"
#include "chrome/browser/safe_browsing/test_safe_browsing_service.h"
#include "components/enterprise/obfuscation/core/utils.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#endif

#if BUILDFLAG(ENABLE_PLUGINS)
#include "content/public/browser/plugin_service.h"
#endif

#if BUILDFLAG(IS_ANDROID)
#include "base/android/build_info.h"
#include "chrome/browser/download/download_prompt_status.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/infobars/core/infobar.h"
#include "components/infobars/core/infobar_delegate.h"
#include "components/infobars/core/infobar_manager.h"
#endif

#if BUILDFLAG(ENTERPRISE_CLOUD_CONTENT_ANALYSIS)
#include "chrome/browser/enterprise/connectors/test/deep_scanning_test_utils.h"
#endif  // BUILDFLAG(ENTERPRISE_CLOUD_CONTENT_ANALYSIS)

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
using ::testing::StrictMock;
using ::testing::WithArg;
using url::Origin;

namespace {

class MockWebContentsDelegate : public content::WebContentsDelegate {
 public:
  ~MockWebContentsDelegate() override {}
  MOCK_METHOD(void,
              CanDownload,
              (const GURL&,
               const std::string&,
               base::OnceCallback<void(bool)> callback),
              (override));
};

ACTION_P3(ScheduleCallback2, result0, result1) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(arg0), result0, result1));
}

// Subclass of the ChromeDownloadManagerDelegate that replaces a few interaction
// points for ease of testing.
class TestChromeDownloadManagerDelegate : public ChromeDownloadManagerDelegate {
 public:
  explicit TestChromeDownloadManagerDelegate(Profile* profile)
      : ChromeDownloadManagerDelegate(profile) {
    ON_CALL(*this, MockCheckDownloadUrl(_, _))
        .WillByDefault(Return(download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS));
#if BUILDFLAG(FULL_SAFE_BROWSING)
    ON_CALL(*this, GetDownloadProtectionService())
        .WillByDefault(Return(nullptr));
#endif
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
  // ChromeDownloadManagerDelegate responds to various DownloadTargetDeterminer
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

#if BUILDFLAG(FULL_SAFE_BROWSING)
  MOCK_METHOD0(GetDownloadProtectionService,
               safe_browsing::DownloadProtectionService*());
#endif

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

  raw_ptr<ChromeDownloadManagerDelegate> delegate_ = nullptr;
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

  download::DownloadTargetInfo DetermineDownloadTarget(DownloadItem* download);

  void OnConfirmationCallbackComplete(
      DownloadTargetDeterminerDelegate::ConfirmationCallback callback,
      DownloadConfirmationResult result,
      const ui::SelectedFileInfo& selected_file_info);

  base::FilePath GetDownloadDirectory() const { return test_download_dir_; }
  TestChromeDownloadManagerDelegate* delegate();
  content::MockDownloadManager* download_manager();
  DownloadPrefs* download_prefs();
  PrefService* pref_service();

  // Creates a mock download item as used by mixed download blocking tests.
  std::unique_ptr<download::MockDownloadItem>
  PrepareDownloadItemForInsecureBlocking(
      const GURL& download_url,
      const std::optional<url::Origin>& request_initiator,
      const std::optional<GURL>& redirect_url);

  const std::vector<uint32_t>& download_ids() const { return download_ids_; }
  void GetNextId(uint32_t next_id) { download_ids_.emplace_back(next_id); }

  void VerifyMixedContentExtensionOverride(
      DownloadItem* download_item,
      const base::FieldTrialParams& parameters,
      InsecureDownloadExtensions extension,
      download::DownloadInterruptReason interrupt_reason,
      download::DownloadItem::InsecureDownloadStatus insecure_download_status);

  MockWebContentsDelegate* web_contents_delegate() {
    return &web_contents_delegate_;
  }

 private:
  base::FilePath test_download_dir_;
  raw_ptr<sync_preferences::TestingPrefServiceSyncable> pref_service_ = nullptr;
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
  pref_service_ = nullptr;
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
      .WillByDefault(ReturnRefOfCopy(std::optional<Origin>()));
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
  std::string guid = base::Uuid::GenerateRandomV4().AsLowercaseString();
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

download::DownloadTargetInfo
ChromeDownloadManagerDelegateTest::DetermineDownloadTarget(
    DownloadItem* download_item) {
  base::test::TestFuture<download::DownloadTargetInfo> future;
  base::OnceCallback callback = future.GetCallback();
  EXPECT_TRUE(delegate()->DetermineDownloadTarget(download_item, &callback));
  EXPECT_FALSE(callback);  // DetermineDownloadTarget() took the callback.
  return future.Get();
}

void ChromeDownloadManagerDelegateTest::OnConfirmationCallbackComplete(
    DownloadTargetDeterminerDelegate::ConfirmationCallback callback,
    DownloadConfirmationResult result,
    const ui::SelectedFileInfo& selected_file_info) {
  delegate_->OnConfirmationCallbackComplete(std::move(callback), result,
                                            selected_file_info);
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
    const std::optional<Origin>& request_initiator,
    const std::optional<GURL>& redirect_url) {
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
  base::HistogramTester histograms;
  base::test::ScopedFeatureList feature_list;

  feature_list.InitAndEnableFeatureWithParameters(
      features::kTreatUnsafeDownloadsAsActive, parameters);

  download::DownloadTargetInfo target_info =
      DetermineDownloadTarget(download_item);

  EXPECT_EQ(interrupt_reason, target_info.interrupt_reason);
  EXPECT_EQ(insecure_download_status, target_info.insecure_download_status);
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
    base::FilePath expected_prompt_path(GetPathInDownloadDir("foo.txt"));
    base::FilePath user_selected_path(GetPathInDownloadDir("bar/baz.txt"));
    EXPECT_CALL(*delegate(), RequestConfirmation_(save_as_download.get(),
                                                  expected_prompt_path, _, _))
        .WillOnce(WithArg<3>(
            ScheduleCallback2(DownloadConfirmationResult::CONFIRMED,
                              ui::SelectedFileInfo(user_selected_path))));
    download::DownloadTargetInfo target_info =
        DetermineDownloadTarget(save_as_download.get());
    EXPECT_EQ(user_selected_path, target_info.target_path);
    VerifyAndClearExpectations();
  }

  {
    // The prompt path for the second download is the user selected directory
    // from the previous download.
    base::FilePath expected_prompt_path(GetPathInDownloadDir("bar/foo.txt"));
    EXPECT_CALL(*delegate(), RequestConfirmation_(save_as_download.get(),
                                                  expected_prompt_path, _, _))
        .WillOnce(WithArg<3>(ScheduleCallback2(
            DownloadConfirmationResult::CANCELED, ui::SelectedFileInfo())));
    download::DownloadTargetInfo target_info =
        DetermineDownloadTarget(save_as_download.get());
    VerifyAndClearExpectations();
  }

  {
    // Start an automatic download. This one should get the default download
    // path since the last download path only affects Save As downloads.
    base::FilePath expected_path(GetPathInDownloadDir("foo.txt"));
    download::DownloadTargetInfo target_info =
        DetermineDownloadTarget(automatic_download.get());
    EXPECT_EQ(expected_path, target_info.target_path);
    VerifyAndClearExpectations();
  }

  {
    // The prompt path for the next download should be the default.
    download_prefs()->SetSaveFilePath(download_prefs()->DownloadPath());
    base::FilePath expected_prompt_path(GetPathInDownloadDir("foo.txt"));
    EXPECT_CALL(*delegate(), RequestConfirmation_(save_as_download.get(),
                                                  expected_prompt_path, _, _))
        .WillOnce(WithArg<3>(ScheduleCallback2(
            DownloadConfirmationResult::CANCELED, ui::SelectedFileInfo())));
    download::DownloadTargetInfo target_info =
        DetermineDownloadTarget(save_as_download.get());
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

  EXPECT_CALL(*delegate(), MockReserveVirtualPath(_, _, _, _, _))
      .WillOnce(DoAll(SetArgPointee<4>(PathValidationResult::CONFLICT),
                      ReturnArg<1>()));
  EXPECT_CALL(*delegate(),
              RequestConfirmation_(
                  _, _, DownloadConfirmationReason::TARGET_CONFLICT, _))
      .WillOnce(
          WithArg<3>(ScheduleCallback2(DownloadConfirmationResult::CONFIRMED,
                                       ui::SelectedFileInfo(kExpectedPath))));
  download::DownloadTargetInfo target_info =
      DetermineDownloadTarget(download_item.get());
  EXPECT_EQ(download::DownloadItem::TARGET_DISPOSITION_PROMPT,
            target_info.target_disposition);
  EXPECT_EQ(kExpectedPath, target_info.target_path);

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
    download::DownloadTargetInfo target_info =
        DetermineDownloadTarget(download_item.get());

    EXPECT_EQ(DownloadFileType::ALLOW_ON_USER_GESTURE,
              DownloadItemModel(download_item.get()).GetDangerLevel());
    EXPECT_EQ(download::DOWNLOAD_DANGER_TYPE_MAYBE_DANGEROUS_CONTENT,
              target_info.danger_type);
  }

  {
    const std::string kSafeContentDisposition(
        "attachment; filename=\"foo.txt\"");
    EXPECT_CALL(*download_item, GetContentDisposition())
        .WillRepeatedly(Return(kSafeContentDisposition));
    download::DownloadTargetInfo target_info =
        DetermineDownloadTarget(download_item.get());
    EXPECT_EQ(DownloadFileType::NOT_DANGEROUS,
              DownloadItemModel(download_item.get()).GetDangerLevel());
    EXPECT_EQ(download::DOWNLOAD_DANGER_TYPE_MAYBE_DANGEROUS_CONTENT,
              target_info.danger_type);
  }

  {
    const std::string kModerateContentDisposition(
        "attachment; filename=\"foo.crx\"");
    EXPECT_CALL(*download_item, GetContentDisposition())
        .WillRepeatedly(Return(kModerateContentDisposition));
    download::DownloadTargetInfo target_info =
        DetermineDownloadTarget(download_item.get());
    EXPECT_EQ(DownloadFileType::ALLOW_ON_USER_GESTURE,
              DownloadItemModel(download_item.get()).GetDangerLevel());
    EXPECT_EQ(download::DOWNLOAD_DANGER_TYPE_MAYBE_DANGEROUS_CONTENT,
              target_info.danger_type);
  }
}

TEST_F(ChromeDownloadManagerDelegateTest, DragAndDropDangerous) {
#if BUILDFLAG(ENABLE_PLUGINS)
  content::PluginService::GetInstance()->Init();
#endif

  GURL url("http://example.com/foo");
  base::FilePath path(GetPathInDownloadDir("foo.evil_file_type"));
  safe_browsing::FileTypePoliciesTestOverlay scoped_dangerous =
      safe_browsing::ScopedMarkAllFilesDangerousForTesting();

  std::unique_ptr<download::MockDownloadItem> download_item =
      CreateActiveDownloadItem(0);
  EXPECT_CALL(*download_item, GetURL()).WillRepeatedly(ReturnRef(url));
  EXPECT_CALL(*download_item, GetDownloadSource())
      .WillRepeatedly(Return(download::DownloadSource::DRAG_AND_DROP));
  EXPECT_CALL(*download_item, GetForcedFilePath())
      .WillRepeatedly(ReturnRef(path));
  EXPECT_CALL(*delegate(), MockCheckDownloadUrl(_, _))
      .WillRepeatedly(
          Return(download::DOWNLOAD_DANGER_TYPE_MAYBE_DANGEROUS_CONTENT));

  download::DownloadTargetInfo target_info =
      DetermineDownloadTarget(download_item.get());

  EXPECT_EQ(DownloadFileType::DANGEROUS,
            DownloadItemModel(download_item.get()).GetDangerLevel());
}

TEST_F(ChromeDownloadManagerDelegateTest, BlockedByPolicy) {
  const GURL kUrl("http://example.com/foo");
  const std::string kTargetDisposition("attachment; filename=\"foo.txt\"");

  std::unique_ptr<download::MockDownloadItem> download_item =
      CreateActiveDownloadItem(0);
  EXPECT_CALL(*download_item, GetURL()).WillRepeatedly(ReturnRef(kUrl));
  EXPECT_CALL(*download_item, GetContentDisposition())
      .WillRepeatedly(Return(kTargetDisposition));
  EXPECT_CALL(*download_item, RequireSafetyChecks())
      .WillRepeatedly(Return(true));

  base::FilePath kExpectedPath = GetPathInDownloadDir("bar.txt");


  EXPECT_CALL(*delegate(), MockReserveVirtualPath(_, _, _, _, _))
      .WillOnce(DoAll(SetArgPointee<4>(PathValidationResult::CONFLICT),
                      ReturnArg<1>()));
  EXPECT_CALL(*delegate(),
              RequestConfirmation_(
                  _, _, DownloadConfirmationReason::TARGET_CONFLICT, _))
      .WillOnce(
          WithArg<3>(ScheduleCallback2(DownloadConfirmationResult::CONFIRMED,
                                       ui::SelectedFileInfo(kExpectedPath))));

  pref_service()->SetInteger(
      prefs::kDownloadRestrictions,
      static_cast<int>(DownloadPrefs::DownloadRestriction::ALL_FILES));

  download::DownloadTargetInfo target_info =
      DetermineDownloadTarget(download_item.get());
  EXPECT_EQ(download::DOWNLOAD_INTERRUPT_REASON_FILE_BLOCKED,
            target_info.interrupt_reason);

  VerifyAndClearExpectations();
}

TEST_F(ChromeDownloadManagerDelegateTest, NoSafetyChecksNotBlockedByPolicy) {
  const GURL kUrl("http://example.com/foo");
  const std::string kTargetDisposition("attachment; filename=\"foo.txt\"");

  std::unique_ptr<download::MockDownloadItem> download_item =
      CreateActiveDownloadItem(0);
  EXPECT_CALL(*download_item, GetURL()).WillRepeatedly(ReturnRef(kUrl));
  EXPECT_CALL(*download_item, GetContentDisposition())
      .WillRepeatedly(Return(kTargetDisposition));
  EXPECT_CALL(*download_item, RequireSafetyChecks())
      .WillRepeatedly(Return(false));

  base::FilePath kExpectedPath = GetPathInDownloadDir("bar.txt");


  EXPECT_CALL(*delegate(), MockReserveVirtualPath(_, _, _, _, _))
      .WillOnce(DoAll(SetArgPointee<4>(PathValidationResult::CONFLICT),
                      ReturnArg<1>()));
  EXPECT_CALL(*delegate(),
              RequestConfirmation_(
                  _, _, DownloadConfirmationReason::TARGET_CONFLICT, _))
      .WillOnce(
          WithArg<3>(ScheduleCallback2(DownloadConfirmationResult::CONFIRMED,
                                       ui::SelectedFileInfo(kExpectedPath))));

  pref_service()->SetInteger(
      prefs::kDownloadRestrictions,
      static_cast<int>(DownloadPrefs::DownloadRestriction::ALL_FILES));

  download::DownloadTargetInfo target_info =
      DetermineDownloadTarget(download_item.get());
  EXPECT_EQ(download::DOWNLOAD_INTERRUPT_REASON_NONE,
            target_info.interrupt_reason);

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

namespace {
class TestDownloadMessageBridge : public DownloadMessageBridge {
 public:
  TestDownloadMessageBridge() = default;

  TestDownloadMessageBridge(const TestDownloadMessageBridge&) = delete;
  TestDownloadMessageBridge& operator=(const TestDownloadMessageBridge&) =
      delete;

  void ShowUnsupportedDownloadMessage(
      content::WebContents* web_contents) override {
    message_shown_count_++;
  }

  // Returns the number of times ShowUnsupportedDownloadMessage has been called.
  int GetMessageShownCount() { return message_shown_count_; }

 private:
  int message_shown_count_;
};

}  // namespace

TEST_F(ChromeDownloadManagerDelegateTest, InterceptDownloadForAutomotive) {
  if (!base::android::BuildInfo::GetInstance()->is_automotive()) {
    GTEST_SKIP() << "This test should only run on automotive.";
  }
  base::HistogramTester histograms;

  TestDownloadMessageBridge* message_bridge = new TestDownloadMessageBridge();
  delegate()->SetDownloadMessageBridgeForTesting(
      static_cast<DownloadMessageBridge*>(message_bridge));

  const GURL kUrl("http://example.com/foo");
  std::string mime_type = "image/png";
  bool should_intercept = delegate()->InterceptDownloadIfApplicable(
      kUrl, "", "", mime_type, "", 10, false /*is_transient*/, nullptr);
  EXPECT_FALSE(should_intercept);

  mime_type = "application/pdf";
  should_intercept = delegate()->InterceptDownloadIfApplicable(
      kUrl, "", "", mime_type, "", 10, false /*is_transient*/, nullptr);
  EXPECT_TRUE(should_intercept);
  histograms.ExpectUniqueSample("Download.Blocked.ContentType.Automotive",
                                download::DownloadContent::PDF, 1);

  EXPECT_EQ(1, message_bridge->GetMessageShownCount());
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
  base::test::ScopedFeatureList feature_list;
  base::HistogramTester histograms;

  feature_list.InitAndEnableFeature(features::kTreatUnsafeDownloadsAsActive);
  download::DownloadTargetInfo target_info =
      DetermineDownloadTarget(download_item.get());
  EXPECT_EQ(download::DOWNLOAD_INTERRUPT_REASON_NONE,
            target_info.interrupt_reason);
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

  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kTreatUnsafeDownloadsAsActive);

  // Blocking shouldn't occur if the target is secure.
  {
    base::HistogramTester histograms;
    std::unique_ptr<download::MockDownloadItem> download_item =
        PrepareDownloadItemForInsecureBlocking(kHttpsUrl, kInsecureOrigin,
                                               std::nullopt);
    download::DownloadTargetInfo target_info =
        DetermineDownloadTarget(download_item.get());

    EXPECT_EQ(download::DOWNLOAD_INTERRUPT_REASON_NONE,
              target_info.interrupt_reason);
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
                                               std::nullopt);
    download::DownloadTargetInfo target_info =
        DetermineDownloadTarget(download_item.get());

    EXPECT_EQ(download::DOWNLOAD_INTERRUPT_REASON_NONE,
              target_info.interrupt_reason);
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
                                             std::nullopt, std::nullopt);
  ON_CALL(*download_item, GetTabUrl())
      .WillByDefault(ReturnRefOfCopy(kSecureOrigin.GetURL()));
  ON_CALL(*download_item, GetDownloadSource())
      .WillByDefault(Return(download::DownloadSource::CONTEXT_MENU));
  base::HistogramTester histograms;
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kTreatUnsafeDownloadsAsActive);

  download::DownloadTargetInfo target_info =
      DetermineDownloadTarget(download_item.get());

  EXPECT_EQ(download::DownloadItem::InsecureDownloadStatus::BLOCK,
            target_info.insecure_download_status);
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
  base::test::ScopedFeatureList feature_list;
  base::HistogramTester histograms;

  feature_list.InitAndEnableFeature(features::kTreatUnsafeDownloadsAsActive);
  download::DownloadTargetInfo target_info =
      DetermineDownloadTarget(download_item.get());

  EXPECT_EQ(download::DOWNLOAD_INTERRUPT_REASON_FILE_BLOCKED,
            target_info.interrupt_reason);
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
                                             std::nullopt);

  VerifyMixedContentExtensionOverride(
      foo_download_item.get(),
      {{"TreatSilentBlockListAsAllowlist", "true"},
       {"SilentBlockExtensionList", "foo"}},
      InsecureDownloadExtensions::kUnknown,
      download::DOWNLOAD_INTERRUPT_REASON_NONE,
      download::DownloadItem::InsecureDownloadStatus::SAFE);
}

// Verify that downloads initiated by a non-unique hostname are blocked and
// we record that the download was from a non-unique source.
TEST_F(ChromeDownloadManagerDelegateTest,
       BlockedAsActiveContent_NonUniqueInitiator) {
  const GURL kRedirectUrl("https://example.org/");
  const GURL kFinalUrl("https://example.org/xyz.foo");
  const auto kInitiator = Origin::Create(GURL("http://10.0.0.1"));

  base::HistogramTester histograms;

  std::unique_ptr<download::MockDownloadItem> download_item =
      PrepareDownloadItemForInsecureBlocking(kFinalUrl, kInitiator,
                                             kRedirectUrl);

#if BUILDFLAG(ENABLE_PLUGINS)
  // DownloadTargetDeterminer looks for plugin handlers if there's an
  // extension.
  content::PluginService::GetInstance()->Init();
#endif

  download::DownloadTargetInfo target_info =
      DetermineDownloadTarget(download_item.get());
  EXPECT_EQ(download::DOWNLOAD_INTERRUPT_REASON_NONE,
            target_info.interrupt_reason);
  EXPECT_EQ(download::DownloadItem::InsecureDownloadStatus::BLOCK,
            target_info.insecure_download_status);
  histograms.ExpectUniqueSample(
      kInsecureDownloadHistogramName,
      InsecureDownloadSecurityStatus::kInitiatorInsecureNonUniqueFileSecure, 1);
  ExpectExtensionOnlyIn(InsecureDownloadExtensions::kUnknown,
                        kInsecureDownloadExtensionInitiatorInsecureNonUnique,
                        kInsecureDownloadHistogramTargetSecure, histograms);
}

// Verify that downloads from a non-unique download url aren't treated as secure
// nor do they record different metrics.
TEST_F(ChromeDownloadManagerDelegateTest,
       BlockedAsActiveContent_NonUniqueFinalUrl) {
  const GURL kRedirectUrl("https://example.org/");
  const GURL kFinalUrl("http://10.0.0.1/xyz.foo");
  const auto kInitiator = Origin::Create(GURL("https://example.org"));

  base::test::ScopedFeatureList feature_list;
  base::HistogramTester histograms;

  std::unique_ptr<download::MockDownloadItem> download_item =
      PrepareDownloadItemForInsecureBlocking(kFinalUrl, kInitiator,
                                             kRedirectUrl);

  feature_list.InitAndEnableFeature(features::kTreatUnsafeDownloadsAsActive);

#if BUILDFLAG(ENABLE_PLUGINS)
  // DownloadTargetDeterminer looks for plugin handlers if there's an
  // extension.
  content::PluginService::GetInstance()->Init();
#endif

  download::DownloadTargetInfo target_info =
      DetermineDownloadTarget(download_item.get());
  EXPECT_EQ(download::DOWNLOAD_INTERRUPT_REASON_FILE_BLOCKED,
            target_info.interrupt_reason);
  EXPECT_EQ(download::DownloadItem::InsecureDownloadStatus::SILENT_BLOCK,
            target_info.insecure_download_status);
  histograms.ExpectUniqueSample(
      kInsecureDownloadHistogramName,
      InsecureDownloadSecurityStatus::kInitiatorSecureFileInsecure, 1);
  ExpectExtensionOnlyIn(InsecureDownloadExtensions::kUnknown,
                        kInsecureDownloadExtensionInitiatorSecure,
                        kInsecureDownloadHistogramTargetInsecure, histograms);
}

// Verify that downloads coming from localhost are considered secure.
TEST_F(ChromeDownloadManagerDelegateTest, BlockedAsActiveContent_Localhost) {
  const GURL kRedirectUrl("https://example.org/");
  const GURL kFinalUrl("http://127.0.0.1/xyz.foo");
  const auto kInitiator = Origin::Create(GURL("https://example.org"));

  base::test::ScopedFeatureList feature_list;
  base::HistogramTester histograms;

  std::unique_ptr<download::MockDownloadItem> download_item =
      PrepareDownloadItemForInsecureBlocking(kFinalUrl, kInitiator,
                                             kRedirectUrl);

  feature_list.InitAndEnableFeature(features::kTreatUnsafeDownloadsAsActive);

#if BUILDFLAG(ENABLE_PLUGINS)
  // DownloadTargetDeterminer looks for plugin handlers if there's an
  // extension.
  content::PluginService::GetInstance()->Init();
#endif

  download::DownloadTargetInfo target_info =
      DetermineDownloadTarget(download_item.get());
  EXPECT_EQ(download::DOWNLOAD_INTERRUPT_REASON_NONE,
            target_info.interrupt_reason);
  EXPECT_EQ(download::DownloadItem::InsecureDownloadStatus::SAFE,
            target_info.insecure_download_status);
  histograms.ExpectUniqueSample(
      kInsecureDownloadHistogramName,
      InsecureDownloadSecurityStatus::kInitiatorSecureFileSecure, 1);
  ExpectExtensionOnlyIn(InsecureDownloadExtensions::kUnknown,
                        kInsecureDownloadExtensionInitiatorSecure,
                        kInsecureDownloadHistogramTargetSecure, histograms);
}

// Verify that downloads initiated by localhost are considered secure.
TEST_F(ChromeDownloadManagerDelegateTest,
       BlockedAsActiveContent_LocalhostInitiator) {
  const GURL kRedirectUrl("https://example.org/");
  const GURL kFinalUrl("https://example.org/xyz.foo");
  const auto kInitiator = Origin::Create(GURL("http://localhost"));

  base::test::ScopedFeatureList feature_list;
  base::HistogramTester histograms;

  std::unique_ptr<download::MockDownloadItem> download_item =
      PrepareDownloadItemForInsecureBlocking(kFinalUrl, kInitiator,
                                             kRedirectUrl);

  feature_list.InitAndEnableFeature(features::kTreatUnsafeDownloadsAsActive);

#if BUILDFLAG(ENABLE_PLUGINS)
  // DownloadTargetDeterminer looks for plugin handlers if there's an
  // extension.
  content::PluginService::GetInstance()->Init();
#endif

  download::DownloadTargetInfo target_info =
      DetermineDownloadTarget(download_item.get());
  EXPECT_EQ(download::DOWNLOAD_INTERRUPT_REASON_NONE,
            target_info.interrupt_reason);
  EXPECT_EQ(download::DownloadItem::InsecureDownloadStatus::SAFE,
            target_info.insecure_download_status);
  histograms.ExpectUniqueSample(
      kInsecureDownloadHistogramName,
      InsecureDownloadSecurityStatus::kInitiatorSecureFileSecure, 1);
  ExpectExtensionOnlyIn(InsecureDownloadExtensions::kUnknown,
                        kInsecureDownloadExtensionInitiatorSecure,
                        kInsecureDownloadHistogramTargetSecure, histograms);
}

// Verify that insecure in a blob URL are considered secure.
TEST_F(ChromeDownloadManagerDelegateTest,
       BlockedAsActiveContent_BlobConsideredSecure) {
  // Verifies blob URLs are not blocked for active content blocking.
  const GURL kRedirectUrl("https://example.org/");
  const GURL kFinalUrl("blob:null/xyz.foo");
  const auto kSecureOrigin = Origin::Create(GURL("https://example.org"));

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

  download::DownloadTargetInfo target_info =
      DetermineDownloadTarget(download_item.get());
  EXPECT_EQ(download::DOWNLOAD_INTERRUPT_REASON_NONE,
            target_info.interrupt_reason);
  EXPECT_EQ(download::DownloadItem::InsecureDownloadStatus::SAFE,
            target_info.insecure_download_status);
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
                                             std::nullopt);

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
                                             std::nullopt);

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
                                             kSecureOrigin, std::nullopt);
  std::unique_ptr<download::MockDownloadItem> foo_download_item =
      PrepareDownloadItemForInsecureBlocking(kFooUrl, kSecureOrigin,
                                             std::nullopt);
  std::unique_ptr<download::MockDownloadItem> bar_download_item =
      PrepareDownloadItemForInsecureBlocking(kBarUrl, kSecureOrigin,
                                             std::nullopt);

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
                                             kSecureOrigin, std::nullopt);
  std::unique_ptr<download::MockDownloadItem> blocked_download_item =
      PrepareDownloadItemForInsecureBlocking(kInsecureBlockableFile,
                                             kSecureOrigin, std::nullopt);
  std::unique_ptr<download::MockDownloadItem> silent_blocked_download_item =
      PrepareDownloadItemForInsecureBlocking(kInsecureSilentlyBlockableFile,
                                             kSecureOrigin, std::nullopt);

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

TEST_F(ChromeDownloadManagerDelegateTest, DownloadBlockedForSyncedTab) {
  GURL download_url = GURL("http://test.com/abc");
  EXPECT_CALL(*web_contents_delegate(), CanDownload(_, _, _))
      .WillRepeatedly([](const GURL& url, const std::string& request_method,
                         base::OnceCallback<void(bool)> callback) {
        std::move(callback).Run(true);
      });
  delegate()->CheckDownloadAllowed(
      base::BindRepeating(&RenderViewHostTestHarness::web_contents,
                          base::Unretained(this)),
      download_url, "GET", std::nullopt, false, false, "application/pdf",
      std::nullopt, base::BindOnce([](bool allowed) { EXPECT_TRUE(allowed); }));
  base::RunLoop().RunUntilIdle();

  TabGroupSyncTabState::CreateForWebContents(web_contents());
  delegate()->CheckDownloadAllowed(
      base::BindRepeating(&RenderViewHostTestHarness::web_contents,
                          base::Unretained(this)),
      download_url, "GET", std::nullopt, false, false, "application/pdf",
      std::nullopt,
      base::BindOnce([](bool allowed) { EXPECT_FALSE(allowed); }));
  base::RunLoop().RunUntilIdle();
}
#endif  // !BUILDFLAG(IS_ANDROID)

TEST_F(ChromeDownloadManagerDelegateTest, InsecureDownloadsBlocked) {
  const GURL kSecureUrl("https://example.net/");
  const GURL kInsecureUrl("http://example.net/");
  const GURL kNullBlobFile("blob:null/foo");
  const GURL kSecureBlobFile("blob:https://example.com/foo");
  const GURL kInsecureBlobFile("blob:http://example.com/foo");
  const GURL kTextFile("http://example.com/foo.txt");
  const GURL kImageFile("http://example.com/foo.png");
  const GURL kMovieFile("http://example.com/foo.mp4");
  const GURL kSecureFile("https://example.com/foo");
  const GURL kInsecureFile("http://example.com/foo");
  const GURL kLocalFile("http://localhost/foo");
  const auto kSecureOrigin = Origin::Create(GURL("https://example.org"));
  const auto kInsecureOrigin = Origin::Create(GURL("http://example.org"));
  const auto kBlankOrigin = Origin::Create(GURL(""));

  const struct {
    // The file's final URL.
    GURL download_url;
    // The origin that linked to or initiated the download.
    std::optional<url::Origin> initiator_origin;
    // One URL that the download may have redirected through.
    std::optional<GURL> redirect_url;

    download::DownloadInterruptReason expected_interrupt_reason;
    download::DownloadItem::InsecureDownloadStatus
        expected_insecure_download_status;

    const std::string test_name;
  } kTestCases[] = {
      // Secure files, with or without redirects, shouldn't be blocked.
      {kSecureFile, kSecureOrigin, kSecureUrl,
       download::DOWNLOAD_INTERRUPT_REASON_NONE,
       download::DownloadItem::InsecureDownloadStatus::SAFE,
       "secure_with_secure_redirect"},
      {kSecureFile, kSecureOrigin, std::nullopt,
       download::DOWNLOAD_INTERRUPT_REASON_NONE,
       download::DownloadItem::InsecureDownloadStatus::SAFE,
       "secure_no_redirect"},
      // Secure files initiated from insecure origins should be blocked.
      {kSecureFile, kInsecureOrigin, std::nullopt,
       download::DOWNLOAD_INTERRUPT_REASON_NONE,
       download::DownloadItem::InsecureDownloadStatus::BLOCK,
       "insecure_no_redirect"},
      // Insecure files initiated from secure origins should be silently blocked
      // as mixed downloads.
      {kInsecureFile, kSecureOrigin, std::nullopt,
       download::DOWNLOAD_INTERRUPT_REASON_FILE_BLOCKED,
       download::DownloadItem::InsecureDownloadStatus::SILENT_BLOCK, "mixdl"},
      // Secure files initiated from secure origins but redirected insecurely
      // should be silently blocked as mixed downloads.
      {kSecureFile, kSecureOrigin, kInsecureUrl,
       download::DOWNLOAD_INTERRUPT_REASON_FILE_BLOCKED,
       download::DownloadItem::InsecureDownloadStatus::SILENT_BLOCK,
       "insecure_redirect"},
      // Blobs initiated from secure origins shouldn't be blocked.
      {kNullBlobFile, kSecureOrigin, kSecureUrl,
       download::DOWNLOAD_INTERRUPT_REASON_NONE,
       download::DownloadItem::InsecureDownloadStatus::SAFE,
       "null_secure_blob"},
      {kSecureBlobFile, kSecureOrigin, kSecureUrl,
       download::DOWNLOAD_INTERRUPT_REASON_NONE,
       download::DownloadItem::InsecureDownloadStatus::SAFE,
       "secure_secure_blob"},
      // Neither should blobs initiated from unknown origins, out of an
      // abundance of caution.
      {kNullBlobFile, std::nullopt, std::nullopt,
       download::DOWNLOAD_INTERRUPT_REASON_NONE,
       download::DownloadItem::InsecureDownloadStatus::SAFE,
       "null_unknown_blob"},
      {kSecureBlobFile, std::nullopt, std::nullopt,
       download::DOWNLOAD_INTERRUPT_REASON_NONE,
       download::DownloadItem::InsecureDownloadStatus::SAFE,
       "secure_unknown_blob"},
      // Empty origins show up as opaque and should be treated like we don't
      // have an initiator.  Note this may introduce a bypass risk but insecure
      // download warnings are not a security boundary.
      {kNullBlobFile, kBlankOrigin, std::nullopt,
       download::DOWNLOAD_INTERRUPT_REASON_NONE,
       download::DownloadItem::InsecureDownloadStatus::SAFE, "null_blank_blob"},
      {kSecureBlobFile, kBlankOrigin, std::nullopt,
       download::DOWNLOAD_INTERRUPT_REASON_NONE,
       download::DownloadItem::InsecureDownloadStatus::SAFE,
       "secure_blank_blob"},
      // If we affirmatively know that a blob's initiator is insecure, however,
      // it should still be blocked.
      {kNullBlobFile, kInsecureOrigin, std::nullopt,
       download::DOWNLOAD_INTERRUPT_REASON_NONE,
       download::DownloadItem::InsecureDownloadStatus::BLOCK,
       "null_insecure_blob"},
      {kInsecureBlobFile, kInsecureOrigin, std::nullopt,
       download::DOWNLOAD_INTERRUPT_REASON_NONE,
       download::DownloadItem::InsecureDownloadStatus::BLOCK,
       "insecure_insecure_blob"},
      {kSecureBlobFile, kInsecureOrigin, std::nullopt,
       download::DOWNLOAD_INTERRUPT_REASON_NONE,
       download::DownloadItem::InsecureDownloadStatus::BLOCK,
       "secure_insecure_blob"},
      // Text, images, audio, etc shouldn't be blocked, even when insecure.
      {kTextFile, kInsecureOrigin, kInsecureUrl,
       download::DOWNLOAD_INTERRUPT_REASON_NONE,
       download::DownloadItem::InsecureDownloadStatus::SAFE, "insecure_txt"},
      {kImageFile, kInsecureOrigin, kInsecureUrl,
       download::DOWNLOAD_INTERRUPT_REASON_NONE,
       download::DownloadItem::InsecureDownloadStatus::SAFE, "insecure_png"},
      {kMovieFile, kInsecureOrigin, kInsecureUrl,
       download::DOWNLOAD_INTERRUPT_REASON_NONE,
       download::DownloadItem::InsecureDownloadStatus::SAFE, "insecure_mp4"},
      // Files hosted on localhost are always secure.
      {kLocalFile, kInsecureOrigin, kInsecureUrl,
       download::DOWNLOAD_INTERRUPT_REASON_NONE,
       download::DownloadItem::InsecureDownloadStatus::SAFE, "local_secure"},
  };

#if BUILDFLAG(ENABLE_PLUGINS)
  // DownloadTargetDeterminer looks for plugin handlers if there's an
  // extension.
  content::PluginService::GetInstance()->Init();
#endif

  // Warning behavior is slightly different with HFM enabled. Enabled behavior
  // is tested in another test.
  pref_service()->SetBoolean(prefs::kHttpsOnlyModeEnabled, false);

  for (const auto& test_case : kTestCases) {
    std::unique_ptr<download::MockDownloadItem> download_item =
        PrepareDownloadItemForInsecureBlocking(test_case.download_url,
                                               test_case.initiator_origin,
                                               test_case.redirect_url);

    download::DownloadTargetInfo target_info =
        DetermineDownloadTarget(download_item.get());
    EXPECT_EQ(test_case.expected_interrupt_reason, target_info.interrupt_reason)
        << "Bad interrupt reason for " << test_case.test_name;
    EXPECT_EQ(test_case.expected_insecure_download_status,
              target_info.insecure_download_status)
        << "Bad insecure download status for " << test_case.test_name;
  }
}

// Verify that insecure downloads not blocked normally are blocked when
// HTTPS-First mode is enabled.
TEST_F(ChromeDownloadManagerDelegateTest,
       InsecureDownloadsBlocked_ExclusionsRemovedInHFM) {
  const GURL kRedirectUrl("http://example.org/");
  const GURL kFinalUrl("http://example.org/xyz.txt");
  const auto kInitiator = Origin::Create(GURL("http://example.org"));

  pref_service()->SetBoolean(prefs::kHttpsOnlyModeEnabled, true);

  base::HistogramTester histograms;

  std::unique_ptr<download::MockDownloadItem> download_item =
      PrepareDownloadItemForInsecureBlocking(kFinalUrl, kInitiator,
                                             kRedirectUrl);

#if BUILDFLAG(ENABLE_PLUGINS)
  // DownloadTargetDeterminer looks for plugin handlers if there's an
  // extension.
  content::PluginService::GetInstance()->Init();
#endif

  download::DownloadTargetInfo target_info =
      DetermineDownloadTarget(download_item.get());
  EXPECT_EQ(download::DOWNLOAD_INTERRUPT_REASON_NONE,
            target_info.interrupt_reason);
  EXPECT_EQ(download::DownloadItem::InsecureDownloadStatus::BLOCK,
            target_info.insecure_download_status);
  histograms.ExpectUniqueSample(
      kInsecureDownloadHistogramName,
      InsecureDownloadSecurityStatus::kInitiatorInsecureFileInsecure, 1);
  ExpectExtensionOnlyIn(InsecureDownloadExtensions::kText,
                        kInsecureDownloadExtensionInitiatorInsecure,
                        kInsecureDownloadHistogramTargetInsecure, histograms);
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
      PrepareDownloadItemForInsecureBlocking(kInsecureFile, std::nullopt,
                                             std::nullopt);
  ON_CALL(*download_item, GetTabUrl())
      .WillByDefault(ReturnRefOfCopy(kSecureOrigin.GetURL()));
  ON_CALL(*download_item, GetDownloadSource())
      .WillByDefault(Return(download::DownloadSource::CONTEXT_MENU));

  download::DownloadTargetInfo target_info =
      DetermineDownloadTarget(download_item.get());

  EXPECT_EQ(download::DOWNLOAD_INTERRUPT_REASON_NONE,
            target_info.interrupt_reason);
  EXPECT_EQ(download::DownloadItem::InsecureDownloadStatus::BLOCK,
            target_info.insecure_download_status);
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
    prefs->SetBoolean(policy::policy_prefs::kForceGoogleSafeSearch,
                      is_safe_search_enabled);

    download::DownloadUrlParameters params(kGoogleSearchUrl,
                                           TRAFFIC_ANNOTATION_FOR_TESTS);

    delegate()->SanitizeDownloadParameters(&params);
    GURL expected_url = kGoogleSearchUrl;
    if (is_safe_search_enabled)
      safe_search_api::ForceGoogleSafeSearch(expected_url, &expected_url);
    EXPECT_EQ(params.url(), expected_url);
  }
}

#if !BUILDFLAG(IS_ANDROID)
namespace {
// Verify the file picker confirmation result matches |expected_result|. Run
// |completion_closure| on completion.
void VerifyFilePickerConfirmation(
    DownloadConfirmationResult expected_result,
    base::RepeatingClosure completion_closure,
    DownloadConfirmationResult result,
    const ui::SelectedFileInfo& selected_file_info) {
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
                                 ui::SelectedFileInfo(expected_prompt_path));

  run_loop.Run();
}
#endif  // BUILDFLAG(IS_ANDROID)

#if !BUILDFLAG(IS_ANDROID)
#if !BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(ChromeDownloadManagerDelegateTest, ScheduleCancelForEphemeralWarning) {
  std::unique_ptr<download::MockDownloadItem> download_item =
      CreateActiveDownloadItem(0);
  EXPECT_CALL(*download_item, GetDangerType())
      .WillRepeatedly(Return(download::DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE));

  delegate()->ScheduleCancelForEphemeralWarning(download_item->GetGuid());

  // Cancel should not be called until threshold is reached
  EXPECT_CALL(*download_item, Cancel(false)).Times(0);
  task_environment()->FastForwardBy(base::Minutes(59));
  EXPECT_CALL(*download_item, Cancel(false)).Times(1);
  task_environment()->FastForwardBy(base::Hours(1));
}

TEST_F(ChromeDownloadManagerDelegateTest,
       ScheduleCancelForEphemeralWarning_DownloadKept) {
  std::unique_ptr<download::MockDownloadItem> download_item =
      CreateActiveDownloadItem(0);
  EXPECT_CALL(*download_item, GetDangerType())
      .WillRepeatedly(Return(download::DOWNLOAD_DANGER_TYPE_USER_VALIDATED));

  delegate()->ScheduleCancelForEphemeralWarning(download_item->GetGuid());

  // Cancel should not be called until threshold is reached
  EXPECT_CALL(*download_item, Cancel(false)).Times(0);
  task_environment()->FastForwardBy(base::Hours(1));
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

TEST_F(ChromeDownloadManagerDelegateTest, CancelAllEphemeralWarnings) {
  std::vector<raw_ptr<download::DownloadItem, VectorExperimental>> items;
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

using ReportType = safe_browsing::ClientSafeBrowsingReportRequest::ReportType;

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
      safe_browsing::CheckDownloadRepeatingCallback callback,
      base::optional_ref<const std::string> password) override {
    std::move(callback).Run(MockCheckClientDownload());
  }

  bool MaybeCheckClientDownload(
      download::DownloadItem* item,
      safe_browsing::CheckDownloadRepeatingCallback callback) override {
    std::move(callback).Run(MockCheckClientDownload());
    return true;
  }

  MOCK_METHOD0(MockCheckClientDownload, safe_browsing::DownloadCheckResult());
};

class FakeSafeBrowsingService : public safe_browsing::TestSafeBrowsingService {
 public:
  FakeSafeBrowsingService() = default;
  FakeSafeBrowsingService(const FakeSafeBrowsingService&) = delete;
  FakeSafeBrowsingService& operator=(const FakeSafeBrowsingService&) = delete;

  void SendDownloadReport(
      download::DownloadItem* download,
      ReportType report_type,
      bool did_proceed,
      std::optional<bool> show_download_in_folder) override {
    actual_sent_report_type_ = report_type;
    actual_sent_did_proceed_ = did_proceed;
    return;
  }

  void PersistDownloadReportAndSendOnNextStartup(
      download::DownloadItem* download,
      ReportType report_type,
      bool did_proceed,
      std::optional<bool> show_download_in_folder) override {
    actual_persisted_report_type_ = report_type;
    actual_persisted_did_proceed_ = did_proceed;
    return;
  }

  std::optional<ReportType> GetActualSentReportType() {
    return actual_sent_report_type_;
  }

  std::optional<bool> GetActualSentDidProceedValue() {
    return actual_sent_did_proceed_;
  }

  std::optional<ReportType> GetActualPersistedReportType() {
    return actual_persisted_report_type_;
  }

  std::optional<bool> GetActualPersistedDidProceedValue() {
    return actual_persisted_did_proceed_;
  }

 protected:
  ~FakeSafeBrowsingService() override = default;

 private:
  std::optional<ReportType> actual_sent_report_type_;
  std::optional<ReportType> actual_persisted_report_type_;
  std::optional<bool> actual_sent_did_proceed_;
  std::optional<bool> actual_persisted_did_proceed_;
};

class ChromeDownloadManagerDelegateTestWithSafeBrowsing
    : public ChromeDownloadManagerDelegateTest,
      public ::testing::WithParamInterface<SafeBrowsingTestParameters> {
 public:
  ChromeDownloadManagerDelegateTestWithSafeBrowsing() = default;

  void SetUp() override;
  void TearDown() override;
  TestDownloadProtectionService* download_protection_service() {
    return test_download_protection_service_.get();
  }

  FakeSafeBrowsingService* safe_browsing_service() { return sb_service_.get(); }

 protected:
  std::unique_ptr<download::MockDownloadItem>
  SetUpDangerousDownloadItemForCanceledReport() {
    std::unique_ptr<download::MockDownloadItem> download_item =
        CreateActiveDownloadItem(0);
    ON_CALL(*download_item, GetDangerType())
        .WillByDefault(Return(download::DOWNLOAD_DANGER_TYPE_DANGEROUS_HOST));
    ON_CALL(*download_item, IsDangerous()).WillByDefault(Return(true));
    safe_browsing::DownloadProtectionService::SetDownloadProtectionData(
        download_item.get(), "token",
        safe_browsing::ClientDownloadResponse::DANGEROUS_HOST,
        safe_browsing::ClientDownloadResponse::TailoredVerdict());
    return download_item;
  }

 private:
  std::unique_ptr<TestDownloadProtectionService>
      test_download_protection_service_;
  scoped_refptr<FakeSafeBrowsingService> sb_service_;
};

void ChromeDownloadManagerDelegateTestWithSafeBrowsing::SetUp() {
  ChromeDownloadManagerDelegateTest::SetUp();
  sb_service_ = base::MakeRefCounted<StrictMock<FakeSafeBrowsingService>>();
  TestingBrowserProcess::GetGlobal()->SetSafeBrowsingService(sb_service_.get());
  test_download_protection_service_ =
      std::make_unique<::testing::StrictMock<TestDownloadProtectionService>>();
  ON_CALL(*delegate(), GetDownloadProtectionService())
      .WillByDefault(Return(test_download_protection_service_.get()));
}

void ChromeDownloadManagerDelegateTestWithSafeBrowsing::TearDown() {
  test_download_protection_service_.reset();
  sb_service_ = nullptr;
  TestingBrowserProcess::GetGlobal()->SetSafeBrowsingService(nullptr);
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

// Auto cancel is only available on platforms with download bubble.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(ChromeDownloadManagerDelegateTestWithSafeBrowsing,
       AutoCanceledReport_Sent) {
  safe_browsing::SetSafeBrowsingState(
      profile()->GetPrefs(),
      safe_browsing::SafeBrowsingState::ENHANCED_PROTECTION);
  std::unique_ptr<download::MockDownloadItem> download_item =
      SetUpDangerousDownloadItemForCanceledReport();

  delegate()->ScheduleCancelForEphemeralWarning(download_item->GetGuid());
  EXPECT_CALL(*download_item, Cancel(false)).Times(1);
  task_environment()->FastForwardBy(base::Hours(1));

  EXPECT_EQ(safe_browsing::ClientSafeBrowsingReportRequest::
                DANGEROUS_DOWNLOAD_AUTO_DELETED,
            safe_browsing_service()->GetActualSentReportType().value());
  EXPECT_FALSE(safe_browsing_service()->GetActualSentDidProceedValue().value());
  EXPECT_FALSE(
      safe_browsing_service()->GetActualPersistedReportType().has_value());
  EXPECT_FALSE(
      safe_browsing_service()->GetActualPersistedDidProceedValue().has_value());
}

TEST_F(ChromeDownloadManagerDelegateTestWithSafeBrowsing,
       AutoCanceledReport_NotSentNotDangerous) {
  safe_browsing::SetSafeBrowsingState(
      profile()->GetPrefs(),
      safe_browsing::SafeBrowsingState::ENHANCED_PROTECTION);
  std::unique_ptr<download::MockDownloadItem> download_item =
      SetUpDangerousDownloadItemForCanceledReport();
  ON_CALL(*download_item, GetDangerType())
      .WillByDefault(Return(download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS));

  delegate()->ScheduleCancelForEphemeralWarning(download_item->GetGuid());
  EXPECT_CALL(*download_item, Cancel(false)).Times(0);
  task_environment()->FastForwardBy(base::Hours(1));

  EXPECT_FALSE(safe_browsing_service()->GetActualSentReportType().has_value());
  EXPECT_FALSE(
      safe_browsing_service()->GetActualSentDidProceedValue().has_value());
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

TEST_F(ChromeDownloadManagerDelegateTestWithSafeBrowsing,
       CanceledReportAtShutdown_Persisted) {
  safe_browsing::SetSafeBrowsingState(
      profile()->GetPrefs(),
      safe_browsing::SafeBrowsingState::ENHANCED_PROTECTION);
  std::unique_ptr<download::MockDownloadItem> download_item =
      SetUpDangerousDownloadItemForCanceledReport();

  delegate()->OnDownloadCanceledAtShutdown(download_item.get());

  EXPECT_EQ(safe_browsing::ClientSafeBrowsingReportRequest::
                DANGEROUS_DOWNLOAD_PROFILE_CLOSED,
            safe_browsing_service()->GetActualPersistedReportType().value());
  EXPECT_FALSE(
      safe_browsing_service()->GetActualPersistedDidProceedValue().value());
  EXPECT_FALSE(safe_browsing_service()->GetActualSentReportType().has_value());
  EXPECT_FALSE(
      safe_browsing_service()->GetActualSentDidProceedValue().has_value());
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

// TODO(crbug.com/41328715) Add a Windows version of this test.
#if !BUILDFLAG(IS_WIN)
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

TEST_F(ChromeDownloadManagerDelegateTestWithSafeBrowsing,
       TrustedSourcesDontExemptEnterpriseScans) {
  base::CommandLine* command_line(base::CommandLine::ForCurrentProcess());
  DCHECK(command_line);
  command_line->AppendSwitchASCII(switches::kTrustedDownloadSources,
                                  "trusted.com");

  policy::SetDMTokenForTesting(policy::DMToken::CreateValidToken("dm_token"));
  enterprise_connectors::test::SetAnalysisConnector(
      pref_service(), enterprise_connectors::FILE_DOWNLOADED,
      R"({
        "service_provider": "google",
        "enable": [
          {
            "url_list": ["*"],
            "tags": ["malware"]
          }
        ],
        "block_until_verdict": 1
      })");

  GURL download_url("http://trusted.com/best-download-ever.exe");
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
  policy::SetDMTokenForTesting(policy::DMToken::CreateEmptyToken());
}
#endif  // !BUILDFLAG(IS_WIN)

TEST_F(ChromeDownloadManagerDelegateTestWithSafeBrowsing,
       ShouldObfuscateDownload) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      enterprise_obfuscation::kEnterpriseFileObfuscation);

  std::unique_ptr<download::MockDownloadItem> download_item =
      CreateActiveDownloadItem(0);

  // Chrome-initiated download
  EXPECT_CALL(*download_item, RequireSafetyChecks())
      .WillRepeatedly(Return(false));
  EXPECT_FALSE(delegate()->ShouldObfuscateDownload(download_item.get()));

  // User-initiated download, no matching connector policies
  EXPECT_CALL(*download_item, RequireSafetyChecks())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*delegate(), GetDownloadProtectionService())
      .WillRepeatedly(Return(nullptr));
  EXPECT_FALSE(delegate()->ShouldObfuscateDownload(download_item.get()));

  // User-initiated download, matching connector policies
  auto mock_protection_service =
      std::make_unique<::testing::StrictMock<TestDownloadProtectionService>>();
  EXPECT_CALL(*delegate(), GetDownloadProtectionService())
      .WillRepeatedly(Return(mock_protection_service.get()));

  policy::SetDMTokenForTesting(policy::DMToken::CreateValidToken("dm_token"));
  enterprise_connectors::test::SetAnalysisConnector(
      pref_service(), enterprise_connectors::FILE_DOWNLOADED,
      R"({
        "service_provider": "google",
        "enable": [
          {
            "url_list": ["*"],
            "tags": ["malware", "dlp"]
          }
        ],
        "block_until_verdict": 1
      })");

  EXPECT_TRUE(delegate()->ShouldObfuscateDownload(download_item.get()));

  // User-initiated download, matching connector policies, but report-only
  enterprise_connectors::test::SetAnalysisConnector(
      pref_service(), enterprise_connectors::FILE_DOWNLOADED,
      R"({
        "service_provider": "google",
        "enable": [
          {
            "url_list": ["*"],
            "tags": ["malware", "dlp"]
          }
        ],
        "block_until_verdict": 0
      })");

  EXPECT_FALSE(delegate()->ShouldObfuscateDownload(download_item.get()));
}
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

  raw_ptr<infobars::ContentInfoBarManager> infobar_manager_ = nullptr;
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
                  Profile* profile,
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

    base::test::TestFuture<DownloadConfirmationResult,
                           const ui::SelectedFileInfo&>
        future;
    delegate()->RequestConfirmation(download_item.get(), fake_path,
                                    test_case.confirmation_reason,
                                    future.GetCallback());
    EXPECT_EQ(test_case.expected_result, future.Get<0>());
    EXPECT_EQ(test_case.path == ExpectPath::FULL
                  ? ui::SelectedFileInfo(fake_path)
                  : ui::SelectedFileInfo(),
              future.Get<1>());

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
