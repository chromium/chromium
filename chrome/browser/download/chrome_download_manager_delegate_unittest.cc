// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/chrome_download_manager_delegate.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/guid.h"
#include "base/location.h"
#include "base/optional.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/download/download_item_model.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/download/download_target_info.h"
#include "chrome/browser/download/mixed_content_download_blocking.h"
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
#include "components/download/public/common/download_features.h"
#include "components/download/public/common/download_interrupt_reasons.h"
#include "components/download/public/common/mock_download_item.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/buildflags.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/browser/download_item_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/test/mock_download_manager.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/origin.h"

#if BUILDFLAG(FULL_SAFE_BROWSING)
#include "chrome/browser/safe_browsing/download_protection/download_protection_service.h"
#endif

#if BUILDFLAG(ENABLE_PLUGINS)
#include "content/public/browser/plugin_service.h"
#endif

#if defined(OS_ANDROID)
#include "chrome/browser/download/download_prompt_status.h"
#include "chrome/browser/infobars/infobar_service.h"
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

ACTION_P3(ScheduleCallback3, result0, result1, result2) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(arg0), result0, result1, result2));
}

// Struct for holding the result of calling DetermineDownloadTarget.
struct DetermineDownloadTargetResult {
  DetermineDownloadTargetResult();

  base::FilePath target_path;
  download::DownloadItem::TargetDisposition disposition;
  download::DownloadDangerType danger_type;
  download::DownloadItem::MixedContentStatus mixed_content_status;
  base::FilePath intermediate_path;
  download::DownloadInterruptReason interrupt_reason;
  base::Optional<download::DownloadSchedule> download_schedule;
};

DetermineDownloadTargetResult::DetermineDownloadTargetResult()
    : disposition(download::DownloadItem::TARGET_DISPOSITION_OVERWRITE),
      danger_type(download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS),
      mixed_content_status(download::DownloadItem::MixedContentStatus::UNKNOWN),
      interrupt_reason(download::DOWNLOAD_INTERRUPT_REASON_NONE) {}

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
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), result, path_to_return));
  }

#if defined(OS_ANDROID)
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

 private:
  friend class ChromeDownloadManagerDelegateTest;
};

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

  // Creates a mock download item as used by HTTP download blocking tests.
  std::unique_ptr<download::MockDownloadItem>
  PrepareDownloadItemForMixedContent(
      const GURL& download_url,
      const base::Optional<url::Origin>& request_initiator,
      const base::Optional<GURL>& redirect_url);

  const std::vector<uint32_t>& download_ids() const { return download_ids_; }
  void GetNextId(uint32_t next_id) { download_ids_.emplace_back(next_id); }

  void VerifyMixedContentExtensionOverride(
      DownloadItem* download_item,
      const base::FieldTrialParams& parameters,
      InsecureDownloadExtensions extension,
      download::DownloadInterruptReason interrupt_reason,
      download::DownloadItem::MixedContentStatus mixed_content_status);

 private:
  base::FilePath test_download_dir_;
  sync_preferences::TestingPrefServiceSyncable* pref_service_;
  std::unique_ptr<content::MockDownloadManager> download_manager_;
  std::unique_ptr<TestChromeDownloadManagerDelegate> delegate_;
  MockWebContentsDelegate web_contents_delegate_;
  std::vector<uint32_t> download_ids_;
  TestingProfileManager testing_profile_manager_;
};

ChromeDownloadManagerDelegateTest::ChromeDownloadManagerDelegateTest()
    : download_manager_(new ::testing::NiceMock<content::MockDownloadManager>),
      testing_profile_manager_(TestingBrowserProcess::GetGlobal()) {}

void ChromeDownloadManagerDelegateTest::SetUp() {
  ChromeRenderViewHostTestHarness::SetUp();

  CHECK(profile());

  test_download_dir_ = profile()->GetPath().AppendASCII("TestDownloadDir");
  ASSERT_TRUE(base::CreateDirectory(test_download_dir_));

  delegate_ =
      std::make_unique<::testing::NiceMock<TestChromeDownloadManagerDelegate>>(
          profile());
  download_prefs()->SkipSanitizeDownloadTargetPathForTesting();
  download_prefs()->SetDownloadPath(test_download_dir_);
  delegate_->SetDownloadManager(download_manager_.get());
  pref_service_ = profile()->GetTestingPrefService();
  web_contents()->SetDelegate(&web_contents_delegate_);

#if defined(OS_ANDROID)
  pref_service_->SetInteger(prefs::kPromptForDownloadAndroid,
                            static_cast<int>(DownloadPromptStatus::DONT_SHOW));

  if (base::FeatureList::IsEnabled(download::features::kDownloadLater)) {
    pref_service_->SetInteger(
        prefs::kDownloadLaterPromptStatus,
        static_cast<int>(DownloadLaterPromptStatus::kDontShow));
  }
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
      .WillByDefault(ReturnRefOfCopy(base::Optional<Origin>()));
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
  content::DownloadItemUtils::AttachInfo(item.get(), profile(), web_contents());
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
    download::DownloadItem::MixedContentStatus mixed_content_status,
    const base::FilePath& intermediate_path,
    base::Optional<download::DownloadSchedule> download_schedule,
    download::DownloadInterruptReason interrupt_reason) {
  result->target_path = target_path;
  result->disposition = target_disposition;
  result->danger_type = danger_type;
  result->mixed_content_status = mixed_content_status;
  result->intermediate_path = intermediate_path;
  result->interrupt_reason = interrupt_reason;
  result->download_schedule = std::move(download_schedule);
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
ChromeDownloadManagerDelegateTest::PrepareDownloadItemForMixedContent(
    const GURL& download_url,
    const base::Optional<Origin>& request_initiator,
    const base::Optional<GURL>& redirect_url) {
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

  std::vector<const std::string> histograms;
  for (auto* initiator : initiator_types) {
    for (auto* download : download_types) {
      histograms.push_back(GetDLBlockingHistogramName(initiator, download));
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
// |interrupt_reason| and |mixed_content_status|. Used by
// BlockedAsActiveContent_ tests.
void ChromeDownloadManagerDelegateTest::VerifyMixedContentExtensionOverride(
    DownloadItem* download_item,
    const base::FieldTrialParams& parameters,
    InsecureDownloadExtensions extension,
    download::DownloadInterruptReason interrupt_reason,
    download::DownloadItem::MixedContentStatus mixed_content_status) {
  DetermineDownloadTargetResult result;
  base::HistogramTester histograms;
  base::test::ScopedFeatureList feature_list;

  feature_list.InitAndEnableFeatureWithParameters(
      features::kTreatUnsafeDownloadsAsActive, parameters);

  DetermineDownloadTarget(download_item, &result);

  EXPECT_EQ(interrupt_reason, result.interrupt_reason);
  EXPECT_EQ(mixed_content_status, result.mixed_content_status);
  histograms.ExpectUniqueSample(
      kInsecureDownloadHistogramName,
      InsecureDownloadSecurityStatus::kInitiatorSecureFileInsecure, 1);
  ExpectExtensionOnlyIn(extension, kInsecureDownloadExtensionInitiatorSecure,
                        kInsecureDownloadHistogramTargetInsecure, histograms);
}

// TODO(https://crbug.com/1042727): Fix test GURL scoping and remove this getter
// function.
GURL ForceGoogleSafeSearch(const GURL& url) {
  GURL new_url;
  safe_search_util::ForceGoogleSafeSearch(url, &new_url);
  return new_url;
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
        .WillOnce(
            WithArg<3>(ScheduleCallback3(DownloadConfirmationResult::CONFIRMED,
                                         user_selected_path, base::nullopt)));
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
        .WillOnce(
            WithArg<3>(ScheduleCallback3(DownloadConfirmationResult::CANCELED,
                                         base::FilePath(), base::nullopt)));
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
        .WillOnce(
            WithArg<3>(ScheduleCallback3(DownloadConfirmationResult::CANCELED,
                                         base::FilePath(), base::nullopt)));
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
      .WillOnce(
          WithArg<3>(ScheduleCallback3(DownloadConfirmationResult::CONFIRMED,
                                       kExpectedPath, base::nullopt)));
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

    EXPECT_EQ(DownloadFileType::DANGEROUS,
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
      .WillOnce(
          WithArg<3>(ScheduleCallback3(DownloadConfirmationResult::CONFIRMED,
                                       kExpectedPath, base::nullopt)));

  pref_service()->SetInteger(
      prefs::kDownloadRestrictions,
      static_cast<int>(DownloadPrefs::DownloadRestriction::ALL_FILES));

  DetermineDownloadTarget(download_item.get(), &result);
  EXPECT_EQ(download::DOWNLOAD_INTERRUPT_REASON_FILE_BLOCKED,
            result.interrupt_reason);

  VerifyAndClearExpectations();
}

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
      PrepareDownloadItemForMixedContent(kSecureSilentlyBlockableFile,
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
        PrepareDownloadItemForMixedContent(kHttpsUrl, kInsecureOrigin,
                                           base::nullopt);
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
        PrepareDownloadItemForMixedContent(kHttpUrl, kInsecureOrigin,
                                           base::nullopt);
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
      PrepareDownloadItemForMixedContent(kInsecureSilentlyBlockableFile,
                                         base::nullopt, base::nullopt);
  ON_CALL(*download_item, GetTabUrl())
      .WillByDefault(ReturnRefOfCopy(kSecureOrigin.GetURL()));
  ON_CALL(*download_item, GetDownloadSource())
      .WillByDefault(Return(download::DownloadSource::CONTEXT_MENU));
  DetermineDownloadTargetResult result;
  base::HistogramTester histograms;
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kTreatUnsafeDownloadsAsActive);

  DetermineDownloadTarget(download_item.get(), &result);

  EXPECT_EQ(download::DownloadItem::MixedContentStatus::BLOCK,
            result.mixed_content_status);
  histograms.ExpectUniqueSample(
      kInsecureDownloadHistogramName,
      InsecureDownloadSecurityStatus::kInitiatorInferredSecureFileInsecure, 1);
  ExpectExtensionOnlyIn(InsecureDownloadExtensions::kTest,
                        kInsecureDownloadExtensionInitiatorInferredSecure,
                        kInsecureDownloadHistogramTargetInsecure, histograms);
}

#if defined(OS_ANDROID)
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
      PrepareDownloadItemForMixedContent(kSecureSilentlyBlockableFile,
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
  const GURL kFooUrl("http://example.com/file.foo");
  const auto kSecureOrigin = Origin::Create(GURL("https://example.org"));

#if BUILDFLAG(ENABLE_PLUGINS)
  // DownloadTargetDeterminer looks for plugin handlers if there's an
  // extension.
  content::PluginService::GetInstance()->Init();
#endif

  std::unique_ptr<download::MockDownloadItem> foo_download_item =
      PrepareDownloadItemForMixedContent(kFooUrl, kSecureOrigin, base::nullopt);

  VerifyMixedContentExtensionOverride(
      foo_download_item.get(), {{"TreatWarnListAsAllowlist", "false"}},
      InsecureDownloadExtensions::kUnknown,
      download::DOWNLOAD_INTERRUPT_REASON_NONE,
      download::DownloadItem::MixedContentStatus::SAFE);
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
      PrepareDownloadItemForMixedContent(kFinalUrl, kSecureOrigin,
                                         kRedirectUrl);

  feature_list.InitAndEnableFeature(features::kTreatUnsafeDownloadsAsActive);

#if BUILDFLAG(ENABLE_PLUGINS)
  // DownloadTargetDeterminer looks for plugin handlers if there's an
  // extension.
  content::PluginService::GetInstance()->Init();
#endif

  DetermineDownloadTarget(download_item.get(), &result);
  EXPECT_EQ(download::DOWNLOAD_INTERRUPT_REASON_NONE, result.interrupt_reason);
  EXPECT_EQ(download::DownloadItem::MixedContentStatus::SAFE,
            result.mixed_content_status);
  histograms.ExpectUniqueSample(
      kInsecureDownloadHistogramName,
      InsecureDownloadSecurityStatus::kInitiatorSecureFileSecure, 1);
  ExpectExtensionOnlyIn(InsecureDownloadExtensions::kUnknown,
                        kInsecureDownloadExtensionInitiatorSecure,
                        kInsecureDownloadHistogramTargetSecure, histograms);
}

TEST_F(ChromeDownloadManagerDelegateTest, BlockedAsActiveContent_SilentBlock) {
  // Verifies that active mixed content download silent blocking works by
  // default, and that extensions can be overridden by feature parameter.
  const GURL kFooUrl("http://example.com/file.foo");
  const GURL kBarUrl("http://example.com/file.bar");
  const GURL kInsecureSilentlyBlockableFile(
      "http://example.com/foo.silently_blocked_for_testing");
  const auto kSecureOrigin = Origin::Create(GURL("https://example.org"));

#if BUILDFLAG(ENABLE_PLUGINS)
  // DownloadTargetDeterminer looks for plugin handlers if there's an
  // extension.
  content::PluginService::GetInstance()->Init();
#endif

  std::unique_ptr<download::MockDownloadItem> blocked_download_item =
      PrepareDownloadItemForMixedContent(kInsecureSilentlyBlockableFile,
                                         kSecureOrigin, base::nullopt);
  std::unique_ptr<download::MockDownloadItem> foo_download_item =
      PrepareDownloadItemForMixedContent(kFooUrl, kSecureOrigin, base::nullopt);
  std::unique_ptr<download::MockDownloadItem> bar_download_item =
      PrepareDownloadItemForMixedContent(kBarUrl, kSecureOrigin, base::nullopt);

  // Test default-blocked extensions are blocked normally, but not when
  // overridden.
  VerifyMixedContentExtensionOverride(
      blocked_download_item.get(), {{}}, InsecureDownloadExtensions::kTest,
      download::DOWNLOAD_INTERRUPT_REASON_FILE_BLOCKED,
      download::DownloadItem::MixedContentStatus::SILENT_BLOCK);
  VerifyMixedContentExtensionOverride(
      blocked_download_item.get(), {{"SilentBlockExtensionList", "foo"}},
      InsecureDownloadExtensions::kTest,
      download::DOWNLOAD_INTERRUPT_REASON_NONE,
      download::DownloadItem::MixedContentStatus::WARN);

  // Test default-listed extensions aren't blocked, but other extensions
  // are, when the extension list is configured as an allowlist.
  VerifyMixedContentExtensionOverride(
      blocked_download_item.get(),
      {{"TreatSilentBlockListAsAllowlist", "true"}},
      InsecureDownloadExtensions::kTest,
      download::DOWNLOAD_INTERRUPT_REASON_NONE,
      download::DownloadItem::MixedContentStatus::WARN);
  VerifyMixedContentExtensionOverride(
      foo_download_item.get(), {{"TreatSilentBlockListAsAllowlist", "true"}},
      InsecureDownloadExtensions::kUnknown,
      download::DOWNLOAD_INTERRUPT_REASON_FILE_BLOCKED,
      download::DownloadItem::MixedContentStatus::SILENT_BLOCK);

  // Test extensions blocked via parameter are indeed blocked.
  VerifyMixedContentExtensionOverride(
      foo_download_item.get(), {{"SilentBlockExtensionList", "foo,bar"}},
      InsecureDownloadExtensions::kUnknown,
      download::DOWNLOAD_INTERRUPT_REASON_FILE_BLOCKED,
      download::DownloadItem::MixedContentStatus::SILENT_BLOCK);
  VerifyMixedContentExtensionOverride(
      bar_download_item.get(), {{"SilentBlockExtensionList", "foo,bar"}},
      InsecureDownloadExtensions::kUnknown,
      download::DOWNLOAD_INTERRUPT_REASON_FILE_BLOCKED,
      download::DownloadItem::MixedContentStatus::SILENT_BLOCK);

  // Test that overriding extensions AND allowlisting work together.
  VerifyMixedContentExtensionOverride(
      foo_download_item.get(),
      {{"SilentBlockExtensionList", "foo"},
       {"TreatSilentBlockListAsAllowlist", "true"}},
      InsecureDownloadExtensions::kUnknown,
      download::DOWNLOAD_INTERRUPT_REASON_NONE,
      download::DownloadItem::MixedContentStatus::WARN);
  VerifyMixedContentExtensionOverride(
      bar_download_item.get(),
      {{"SilentBlockExtensionList", "foo"},
       {"TreatSilentBlockListAsAllowlist", "true"}},
      InsecureDownloadExtensions::kUnknown,
      download::DOWNLOAD_INTERRUPT_REASON_FILE_BLOCKED,
      download::DownloadItem::MixedContentStatus::SILENT_BLOCK);
}

TEST_F(ChromeDownloadManagerDelegateTest, BlockedAsActiveContent_Warn) {
  // Verifies that active mixed content download warning works by default, and
  // that extensions can be overridden by feature parameter.
  const GURL kFooUrl("http://example.com/file.foo");
  const GURL kBarUrl("http://example.com/file.bar");
  const GURL kDontWarnUrl("http://example.com/file.dont_warn_for_testing");
  const GURL kInsecureWarnableFile("http://example.com/foo.warn_for_testing");
  const auto kSecureOrigin = Origin::Create(GURL("https://example.org"));

#if BUILDFLAG(ENABLE_PLUGINS)
  // DownloadTargetDeterminer looks for plugin handlers if there's an
  // extension.
  content::PluginService::GetInstance()->Init();
#endif

  std::unique_ptr<download::MockDownloadItem> warned_download_item =
      PrepareDownloadItemForMixedContent(kInsecureWarnableFile, kSecureOrigin,
                                         base::nullopt);
  std::unique_ptr<download::MockDownloadItem> foo_download_item =
      PrepareDownloadItemForMixedContent(kFooUrl, kSecureOrigin, base::nullopt);
  std::unique_ptr<download::MockDownloadItem> bar_download_item =
      PrepareDownloadItemForMixedContent(kBarUrl, kSecureOrigin, base::nullopt);
  std::unique_ptr<download::MockDownloadItem> dont_warn_download_item =
      PrepareDownloadItemForMixedContent(kDontWarnUrl, kSecureOrigin,
                                         base::nullopt);

  // WarnExtensionList is configured as an allowlist by default, so verify that
  // extensions not listed are warned on normally, but not if they're listed.
  VerifyMixedContentExtensionOverride(
      warned_download_item.get(), {{}}, InsecureDownloadExtensions::kTest,
      download::DOWNLOAD_INTERRUPT_REASON_NONE,
      download::DownloadItem::MixedContentStatus::WARN);
  VerifyMixedContentExtensionOverride(
      warned_download_item.get(), {{"WarnExtensionList", "warn_for_testing"}},
      InsecureDownloadExtensions::kTest,
      download::DOWNLOAD_INTERRUPT_REASON_NONE,
      download::DownloadItem::MixedContentStatus::SAFE);

  // Test default-warned extensions aren't still warned, but listed extensions
  // are, when warnings are configured as a blocklist.
  VerifyMixedContentExtensionOverride(
      warned_download_item.get(), {{"TreatWarnListAsAllowlist", "false"}},
      InsecureDownloadExtensions::kTest,
      download::DOWNLOAD_INTERRUPT_REASON_NONE,
      download::DownloadItem::MixedContentStatus::SAFE);
  VerifyMixedContentExtensionOverride(
      dont_warn_download_item.get(), {{"TreatWarnListAsAllowlist", "false"}},
      InsecureDownloadExtensions::kTest,
      download::DOWNLOAD_INTERRUPT_REASON_NONE,
      download::DownloadItem::MixedContentStatus::WARN);

  // Test extensions selected via parameter are indeed warned on.
  VerifyMixedContentExtensionOverride(
      foo_download_item.get(),
      {{"WarnExtensionList", "foo,bar"}, {"TreatWarnListAsAllowlist", "false"}},
      InsecureDownloadExtensions::kUnknown,
      download::DOWNLOAD_INTERRUPT_REASON_NONE,
      download::DownloadItem::MixedContentStatus::WARN);
  VerifyMixedContentExtensionOverride(
      bar_download_item.get(),
      {{"WarnExtensionList", "foo,bar"}, {"TreatWarnListAsAllowlist", "false"}},
      InsecureDownloadExtensions::kUnknown,
      download::DOWNLOAD_INTERRUPT_REASON_NONE,
      download::DownloadItem::MixedContentStatus::WARN);
}

TEST_F(ChromeDownloadManagerDelegateTest, BlockedAsActiveContent_Block) {
  // Verifies that active mixed content download user-visible blocking works by
  // default, and that extensions can be overridden by feature parameter.
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
      PrepareDownloadItemForMixedContent(kInsecureBlockableFile, kSecureOrigin,
                                         base::nullopt);
  std::unique_ptr<download::MockDownloadItem> foo_download_item =
      PrepareDownloadItemForMixedContent(kFooUrl, kSecureOrigin, base::nullopt);
  std::unique_ptr<download::MockDownloadItem> bar_download_item =
      PrepareDownloadItemForMixedContent(kBarUrl, kSecureOrigin, base::nullopt);

  // Test default-blocked extensions are blocked normally, but not when
  // overridden.
  VerifyMixedContentExtensionOverride(
      blocked_download_item.get(), {{}},
      InsecureDownloadExtensions::kMSExecutable,
      download::DOWNLOAD_INTERRUPT_REASON_NONE,
      download::DownloadItem::MixedContentStatus::BLOCK);
  VerifyMixedContentExtensionOverride(
      blocked_download_item.get(), {{"BlockExtensionList", "foo"}},
      InsecureDownloadExtensions::kMSExecutable,
      download::DOWNLOAD_INTERRUPT_REASON_NONE,
      download::DownloadItem::MixedContentStatus::WARN);

  // Test default-listed extensions aren't blocked, but other extensions
  // are, when the extension list is configured as an allowlist.
  VerifyMixedContentExtensionOverride(
      blocked_download_item.get(), {{"TreatBlockListAsAllowlist", "true"}},
      InsecureDownloadExtensions::kMSExecutable,
      download::DOWNLOAD_INTERRUPT_REASON_NONE,
      download::DownloadItem::MixedContentStatus::WARN);
  VerifyMixedContentExtensionOverride(
      foo_download_item.get(), {{"TreatBlockListAsAllowlist", "true"}},
      InsecureDownloadExtensions::kUnknown,
      download::DOWNLOAD_INTERRUPT_REASON_NONE,
      download::DownloadItem::MixedContentStatus::BLOCK);

  // Test extensions selected via parameter are indeed blocked.
  VerifyMixedContentExtensionOverride(
      foo_download_item.get(), {{"BlockExtensionList", "foo,bar"}},
      InsecureDownloadExtensions::kUnknown,
      download::DOWNLOAD_INTERRUPT_REASON_NONE,
      download::DownloadItem::MixedContentStatus::BLOCK);
  VerifyMixedContentExtensionOverride(
      bar_download_item.get(), {{"BlockExtensionList", "foo,bar"}},
      InsecureDownloadExtensions::kUnknown,
      download::DOWNLOAD_INTERRUPT_REASON_NONE,
      download::DownloadItem::MixedContentStatus::BLOCK);

  // Test that overriding extensions AND allowlisting work together.
  VerifyMixedContentExtensionOverride(
      foo_download_item.get(),
      {{"BlockExtensionList", "foo"}, {"TreatBlockListAsAllowlist", "true"}},
      InsecureDownloadExtensions::kUnknown,
      download::DOWNLOAD_INTERRUPT_REASON_NONE,
      download::DownloadItem::MixedContentStatus::WARN);
  VerifyMixedContentExtensionOverride(
      bar_download_item.get(),
      {{"BlockExtensionList", "foo"}, {"TreatBlockListAsAllowlist", "true"}},
      InsecureDownloadExtensions::kUnknown,
      download::DOWNLOAD_INTERRUPT_REASON_NONE,
      download::DownloadItem::MixedContentStatus::BLOCK);
}

// MIXEDSCRIPT content setting only applies to Desktop.
#if !defined(OS_ANDROID)
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
      PrepareDownloadItemForMixedContent(kInsecureWarnableFile, kSecureOrigin,
                                         base::nullopt);
  std::unique_ptr<download::MockDownloadItem> blocked_download_item =
      PrepareDownloadItemForMixedContent(kInsecureBlockableFile, kSecureOrigin,
                                         base::nullopt);
  std::unique_ptr<download::MockDownloadItem> silent_blocked_download_item =
      PrepareDownloadItemForMixedContent(kInsecureSilentlyBlockableFile,
                                         kSecureOrigin, base::nullopt);

  HostContentSettingsMapFactory::GetForProfile(profile())
      ->SetContentSettingDefaultScope(kSecureOrigin.GetURL(), GURL(),
                                      ContentSettingsType::MIXEDSCRIPT,
                                      CONTENT_SETTING_ALLOW);

  VerifyMixedContentExtensionOverride(
      warned_download_item.get(), {{}}, InsecureDownloadExtensions::kTest,
      download::DOWNLOAD_INTERRUPT_REASON_NONE,
      download::DownloadItem::MixedContentStatus::SAFE);
  VerifyMixedContentExtensionOverride(
      blocked_download_item.get(), {{}},
      InsecureDownloadExtensions::kMSExecutable,
      download::DOWNLOAD_INTERRUPT_REASON_NONE,
      download::DownloadItem::MixedContentStatus::SAFE);
  VerifyMixedContentExtensionOverride(
      silent_blocked_download_item.get(), {{}},
      InsecureDownloadExtensions::kTest,
      download::DOWNLOAD_INTERRUPT_REASON_NONE,
      download::DownloadItem::MixedContentStatus::SAFE);
}
#endif  // !OS_ANDROID

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
  static const GURL kGoogleSearchUrl("https://www.google.com/search?q=google");
  static const GURL kGoogleSearchUrlSanitized(
      ForceGoogleSafeSearch(kGoogleSearchUrl));

  for (auto is_safe_search_enabled : {true, false}) {
    auto* prefs = profile()->GetPrefs();
    prefs->SetBoolean(prefs::kForceGoogleSafeSearch, is_safe_search_enabled);

    download::DownloadUrlParameters params(kGoogleSearchUrl,
                                           TRAFFIC_ANNOTATION_FOR_TESTS);

    delegate()->SanitizeDownloadParameters(&params);
    const auto& actual_url =
        is_safe_search_enabled ? kGoogleSearchUrlSanitized : kGoogleSearchUrl;
    EXPECT_EQ(params.url(), actual_url);
  }
}

#if !defined(OS_ANDROID)
namespace {
// Verify the file picker confirmation result matches |expected_result|. Run
// |completion_closure| on completion.
void VerifyFilePickerConfirmation(
    DownloadConfirmationResult expected_result,
    base::RepeatingClosure completion_closure,
    DownloadConfirmationResult result,
    const base::FilePath& virtual_path,
    base::Optional<download::DownloadSchedule> download_schedule) {
  ASSERT_EQ(result, expected_result);
  ASSERT_FALSE(download_schedule)
      << "DownloadSchedule is only used on Android.";
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
#endif  // OS_ANDROID

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

     download::DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE,
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

     download::DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE,
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

    // POTENTIALLY_UNWANTED verdict for a potentially dangerous file, not
    // blocked by policy.
    {download::DOWNLOAD_DANGER_TYPE_MAYBE_DANGEROUS_CONTENT,
     DownloadFileType::ALLOW_ON_USER_GESTURE,
     safe_browsing::DownloadCheckResult::POTENTIALLY_UNWANTED,
     DownloadPrefs::DownloadRestriction::DANGEROUS_FILES,

     download::DOWNLOAD_DANGER_TYPE_POTENTIALLY_UNWANTED,
     /*blocked=*/false},

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

TEST_F(ChromeDownloadManagerDelegateTestWithSafeBrowsing,
       TrustedSourcesPolicyNotTrusted) {
  GURL download_url("http://untrusted.com/best-download-ever.exe");
  pref_service()->SetBoolean(prefs::kSafeBrowsingForTrustedSourcesEnabled,
                             false);
  std::unique_ptr<download::MockDownloadItem> download_item =
      CreateActiveDownloadItem(0);
  EXPECT_CALL(*download_item, GetURL()).WillRepeatedly(ReturnRef(download_url));

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

#if !defined(OS_WIN)
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
  EXPECT_CALL(*delegate(), GetDownloadProtectionService()).Times(0);
  EXPECT_TRUE(delegate()->ShouldCompleteDownload(download_item.get(),
                                                 base::OnceClosure()));
}
#endif  // OS_WIN
#endif  // FULL_SAFE_BROWSING

#if defined(OS_ANDROID)

namespace {

class AndroidDownloadInfobarCounter
    : public infobars::InfoBarManager::Observer {
 public:
  explicit AndroidDownloadInfobarCounter(content::WebContents* web_contents)
      : infobar_service_(InfoBarService::FromWebContents(web_contents)) {
    infobar_service_->AddObserver(this);
  }

  ~AndroidDownloadInfobarCounter() override {
    infobar_service_->RemoveObserver(this);
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

  InfoBarService* infobar_service_;
  int infobar_count_ = 0;
};

class TestDownloadDialogBridge : public DownloadDialogBridge {
 public:
  TestDownloadDialogBridge() = default;

  // DownloadDialogBridge implementation.
  void ShowDialog(gfx::NativeWindow native_window,
                  int64_t total_bytes,
                  ConnectionType connection_type,
                  DownloadLocationDialogType dialog_type,
                  const base::FilePath& suggested_path,
                  bool supports_later_dialog,
                  bool show_date_time_picker,
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

  DISALLOW_COPY_AND_ASSIGN(TestDownloadDialogBridge);
};

}  // namespace

TEST_F(ChromeDownloadManagerDelegateTest, RequestConfirmation_Android) {
  DeleteContents();
  SetContents(CreateTestWebContents());

  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitAndEnableFeature(features::kDownloadsLocationChange);
  EXPECT_TRUE(base::FeatureList::IsEnabled(features::kDownloadsLocationChange));

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
    base::Optional<download::DownloadSchedule> download_schedule;
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
    content::DownloadItemUtils::AttachInfo(
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
           base::Optional<download::DownloadSchedule> expected_schedule,
           DownloadConfirmationResult actual_result,
           const base::FilePath& actual_path,
           base::Optional<download::DownloadSchedule> download_schedule) {
          EXPECT_EQ(expected_result, actual_result);
          EXPECT_EQ(expected_path, actual_path);
          EXPECT_EQ(expected_schedule, download_schedule);
          quit_closure.Run();
        },
        loop.QuitClosure(), test_case.expected_result,
        test_case.path == ExpectPath::FULL ? fake_path : base::FilePath(),
        test_case.download_schedule);
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

class MockNetworkChangeNotifier : public net::NetworkChangeNotifier {
 public:
  explicit MockNetworkChangeNotifier(ConnectionType type)
      : connection_type_(type) {}

  // net::NetworkChangeNotifier implementation.
  ConnectionType GetCurrentConnectionType() const override {
    return connection_type_;
  }

 private:
  ConnectionType connection_type_;
};

class DownloadLaterTriggerTest : public ChromeDownloadManagerDelegateTest {
 public:
  void SetUp() override {
    // Enable the download later feature first to ensure download pref loads
    // correctly.
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        download::features::kDownloadLater,
        {{download::features::kDownloadLaterMinFileSizeKb, "204800"}});
    ChromeDownloadManagerDelegateTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(DownloadLaterTriggerTest, DownloadLaterTrigger) {
  net::NetworkChangeNotifier::DisableForTest disable_for_test;
  std::unique_ptr<net::NetworkChangeNotifier> mock_network_notifier =
      std::make_unique<MockNetworkChangeNotifier>(
          ConnectionType::CONNECTION_2G);

  std::unique_ptr<download::MockDownloadItem> download_item =
      CreateActiveDownloadItem(1);
  ON_CALL(*download_item, GetTotalBytes()).WillByDefault(Return(0));

  // Slow connection.
  pref_service()->SetInteger(
      prefs::kDownloadLaterPromptStatus,
      static_cast<int>(DownloadLaterPromptStatus::kShowInitial));
  EXPECT_EQ(ConnectionType::CONNECTION_2G,
            net::NetworkChangeNotifier::GetConnectionType());
  EXPECT_TRUE(delegate()->ShouldShowDownloadLaterDialog(download_item.get()));

  mock_network_notifier.reset();
  mock_network_notifier = std::make_unique<MockNetworkChangeNotifier>(
      ConnectionType::CONNECTION_4G);
  EXPECT_FALSE(delegate()->ShouldShowDownloadLaterDialog(download_item.get()));

  // Large file.
  ON_CALL(*download_item, GetTotalBytes())
      .WillByDefault(Return(400 * 1024 * 1024));
  EXPECT_TRUE(delegate()->ShouldShowDownloadLaterDialog(download_item.get()));

  // Small file.
  ON_CALL(*download_item, GetTotalBytes())
      .WillByDefault(Return(190 * 1024 * 1024));
  EXPECT_FALSE(delegate()->ShouldShowDownloadLaterDialog(download_item.get()));

  // Pref turn off.
  pref_service()->SetInteger(
      prefs::kDownloadLaterPromptStatus,
      static_cast<int>(DownloadLaterPromptStatus::kDontShow));
  EXPECT_FALSE(delegate()->ShouldShowDownloadLaterDialog(download_item.get()));
}
#endif  // OS_ANDROID
