// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_item_model.h"

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

#include "base/check_op.h"
#include "base/i18n/rtl.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "base/time/time.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/browser/download/chrome_download_manager_delegate.h"
#include "chrome/browser/download/download_commands.h"
#include "chrome/browser/download/download_core_service_factory.h"
#include "chrome/browser/download/download_core_service_impl.h"
#include "chrome/browser/download/download_ui_model.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/download/public/common/download_danger_type.h"
#include "components/download/public/common/download_interrupt_reasons.h"
#include "components/download/public/common/download_item_rename_handler.h"
#include "components/download/public/common/mock_download_item.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/download_item_utils.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/text/bytes_formatting.h"
#include "ui/base/ui_base_features.h"

#if !BUILDFLAG(IS_ANDROID)
#include "base/strings/pattern.h"
#include "chrome/test/base/testing_profile.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "ui/views/vector_icons.h"

#if BUILDFLAG(FULL_SAFE_BROWSING)
#include "chrome/browser/safe_browsing/download_protection/download_protection_service.h"
#endif  // BUILDFLAG(FULL_SAFE_BROWSING)

#endif  // BUILDFLAG(IS_ANDROID)

using download::DownloadItem;
using offline_items_collection::FailState;
using safe_browsing::DownloadFileType;
using ::testing::_;
using ::testing::Mock;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::ReturnRefOfCopy;
using ::testing::SetArgPointee;

#if BUILDFLAG(FULL_SAFE_BROWSING)
using TailoredVerdict = safe_browsing::ClientDownloadResponse::TailoredVerdict;
#endif

namespace {

// Create a char array that has as many elements as there are download
// interrupt reasons. We can then use that in a static_assert to make sure
// that all the interrupt reason codes are accounted for. The reason codes are
// unfortunately sparse, making this necessary.
char kInterruptReasonCounter[] = {
    0,  // download::DOWNLOAD_INTERRUPT_REASON_NONE
#define INTERRUPT_REASON(name,value) 0,
#include "components/download/public/common/download_interrupt_reason_values.h"
#undef INTERRUPT_REASON
};
const size_t kInterruptReasonCount = std::size(kInterruptReasonCounter);

// Default target path for a mock download item in DownloadItemModelTest.
const base::FilePath::CharType kDefaultTargetFilePath[] =
    FILE_PATH_LITERAL("/foo/bar/foo.bar");

const base::FilePath::CharType kDefaultDisplayFileName[] =
    FILE_PATH_LITERAL("foo.bar");

// Default URL for a mock download item in DownloadItemModelTest.
const char kDefaultURL[] = "http://example.com/foo.bar";

// A DownloadCoreService that returns the TestChromeDownloadManagerDelegate.
class TestDownloadCoreService : public DownloadCoreServiceImpl {
 public:
  explicit TestDownloadCoreService(Profile* profile);
  ~TestDownloadCoreService() override;

  void set_download_manager_delegate(ChromeDownloadManagerDelegate* delegate) {
    delegate_ = delegate;
  }

  ChromeDownloadManagerDelegate* GetDownloadManagerDelegate() override;

  raw_ptr<ChromeDownloadManagerDelegate, DanglingUntriaged> delegate_;
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

class TestChromeDownloadManagerDelegate : public ChromeDownloadManagerDelegate {
 public:
  explicit TestChromeDownloadManagerDelegate(Profile* profile)
      : ChromeDownloadManagerDelegate(profile) {}
  ~TestChromeDownloadManagerDelegate() override;

  // ChromeDownloadManagerDelegate override:
  bool IsOpenInBrowserPreferredForFile(const base::FilePath& path) override;
};

TestChromeDownloadManagerDelegate::~TestChromeDownloadManagerDelegate() =
    default;

bool TestChromeDownloadManagerDelegate::IsOpenInBrowserPreferredForFile(
    const base::FilePath& path) {
  return true;
}

class FakeRenameHandler : public download::DownloadItemRenameHandler {
 public:
  explicit FakeRenameHandler(DownloadItem* download_item)
      : DownloadItemRenameHandler(download_item) {}
  ~FakeRenameHandler() override = default;

  // DownloadItemRenameHandler interface:
  bool ShowRenameProgress() override { return true; }
};

}  // namespace

class DownloadItemModelTest : public testing::Test {
 public:
  DownloadItemModelTest()
      : model_(&item_),
        testing_profile_manager_(TestingBrowserProcess::GetGlobal()) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  }

  ~DownloadItemModelTest() override {}

  void SetUp() override {
    ASSERT_TRUE(testing_profile_manager_.SetUp());
    profile_ = testing_profile_manager_.CreateTestingProfile("testing_profile");
    delegate_ =
        std::make_unique<NiceMock<TestChromeDownloadManagerDelegate>>(profile_);
    DownloadCoreServiceFactory::GetInstance()->SetTestingFactory(
        profile_, base::BindRepeating(&CreateTestDownloadCoreService));
    static_cast<TestDownloadCoreService*>(
        DownloadCoreServiceFactory::GetForBrowserContext(profile_))
        ->set_download_manager_delegate(delegate_.get());
  }

 protected:
  // Sets up defaults for the download item and sets |model_| to a new
  // DownloadItemModel that uses the mock download item.
  void SetupDownloadItemDefaults() {
    ON_CALL(item_, GetReceivedBytes()).WillByDefault(Return(1));
    ON_CALL(item_, GetTotalBytes()).WillByDefault(Return(2));
    ON_CALL(item_, TimeRemaining(_)).WillByDefault(Return(false));
    ON_CALL(item_, GetMimeType()).WillByDefault(Return("text/html"));
    ON_CALL(item_, AllDataSaved()).WillByDefault(Return(false));
    ON_CALL(item_, GetOpenWhenComplete()).WillByDefault(Return(false));
    ON_CALL(item_, GetFileExternallyRemoved()).WillByDefault(Return(false));
    ON_CALL(item_, GetState())
        .WillByDefault(Return(DownloadItem::IN_PROGRESS));
    ON_CALL(item_, GetURL())
        .WillByDefault(ReturnRefOfCopy(GURL(kDefaultURL)));
    ON_CALL(item_, GetFileNameToReportUser())
        .WillByDefault(Return(base::FilePath(kDefaultDisplayFileName)));
    ON_CALL(item_, GetTargetFilePath())
        .WillByDefault(ReturnRefOfCopy(base::FilePath(kDefaultTargetFilePath)));
    ON_CALL(item_, GetTargetDisposition())
        .WillByDefault(
            Return(DownloadItem::TARGET_DISPOSITION_OVERWRITE));
    ON_CALL(item_, IsPaused()).WillByDefault(Return(false));
    ON_CALL(item_, CanResume()).WillByDefault(Return(false));
    ON_CALL(item_, GetInsecureDownloadStatus())
        .WillByDefault(
            Return(download::DownloadItem::InsecureDownloadStatus::SAFE));
    ON_CALL(item(), GetDangerType())
        .WillByDefault(Return(download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS));
    content::DownloadItemUtils::AttachInfoForTesting(&(item()), profile_,
                                                     nullptr);
  }

  void SetupInterruptedDownloadItem(download::DownloadInterruptReason reason) {
    EXPECT_CALL(item_, GetLastReason()).WillRepeatedly(Return(reason));
    EXPECT_CALL(item_, GetState())
        .WillRepeatedly(
            Return((reason == download::DOWNLOAD_INTERRUPT_REASON_NONE)
                       ? DownloadItem::IN_PROGRESS
                       : DownloadItem::INTERRUPTED));
  }

  void SetupCompletedDownloadItem(base::TimeDelta time_since_complete) {
    ON_CALL(item_, GetFileExternallyRemoved()).WillByDefault(Return(false));
    EXPECT_CALL(item_, GetState())
        .WillRepeatedly(Return(DownloadItem::COMPLETE));
    base::Time now = base::Time::Now();
    base::TimeDelta diff = time_since_complete;
    clock_.SetNow(now);
    model_.set_clock_for_testing(&clock_);
    ON_CALL(item_, GetEndTime()).WillByDefault(Return(now - diff));
  }

  download::MockDownloadItem& item() { return item_; }

  DownloadItemModel& model() {
    return model_;
  }

  Profile* profile() { return profile_; }
  TestingProfile* testing_profile() { return profile_; }

  void SetStatusTextBuilder(bool for_bubble) {
    model_.set_status_text_builder_for_testing(for_bubble);
  }

  content::BrowserTaskEnvironment task_environment_;

 private:
  NiceMock<download::MockDownloadItem> item_;
  DownloadItemModel model_;
  base::SimpleTestClock clock_;
  TestingProfileManager testing_profile_manager_;
  raw_ptr<TestingProfile> profile_;
  std::unique_ptr<NiceMock<TestChromeDownloadManagerDelegate>> delegate_;

  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(DownloadItemModelTest, InterruptedStatus) {
  // Test that we have the correct interrupt status message for downloads that
  // are in the INTERRUPTED state.
  const struct TestCase {
    // The reason.
    download::DownloadInterruptReason reason;

    // Expected status string. This will include the progress as well.
    const char* expected_status_msg;

    // Expected bubble status string. This will include the progress as well.
    // If empty, use the expected_status_msg.
    std::string expected_bubble_status_msg;

    // Most types of interrupted downloads have combination of icon and color
    // that is not the same as "dangerous" or "suspicious" downloads.
    DownloadUIModel::DangerUiPattern expected_danger_pattern =
        DownloadUIModel::DangerUiPattern::kOther;
  } kTestCases[] = {
      {download::DOWNLOAD_INTERRUPT_REASON_NONE, "1/2 B", "1/2 B • Resuming…",
       DownloadUIModel::DangerUiPattern::kNormal},
      {download::DOWNLOAD_INTERRUPT_REASON_FILE_FAILED, "%s - Download error",
       "Something went wrong"},
      {download::DOWNLOAD_INTERRUPT_REASON_FILE_ACCESS_DENIED,
       "%s - Insufficient permissions", "Needs permission to download"},
      {download::DOWNLOAD_INTERRUPT_REASON_FILE_NO_SPACE, "%s - Disk full",
       "Out of storage space"},
      {download::DOWNLOAD_INTERRUPT_REASON_FILE_NAME_TOO_LONG,
       "%s - Path too long", "File name or location is too long"},
      {download::DOWNLOAD_INTERRUPT_REASON_FILE_TOO_LARGE,
       "%s - File too large", "File is too big for this device"},
      {download::DOWNLOAD_INTERRUPT_REASON_FILE_VIRUS_INFECTED,
       "%s - Virus detected", "Virus detected"},
      {download::DOWNLOAD_INTERRUPT_REASON_FILE_BLOCKED, "%s - Blocked",
       "Blocked by your organization"},
      {download::DOWNLOAD_INTERRUPT_REASON_FILE_SECURITY_CHECK_FAILED,
       "%s - Virus scan failed", "Virus scan failed"},
      {download::DOWNLOAD_INTERRUPT_REASON_FILE_TOO_SHORT,
       "%s - File truncated", "Something went wrong"},
      {download::DOWNLOAD_INTERRUPT_REASON_FILE_SAME_AS_SOURCE,
       "%s - Already downloaded", "Already downloaded"},
      {download::DOWNLOAD_INTERRUPT_REASON_FILE_TRANSIENT_ERROR,
       "%s - System busy", "Couldn’t finish download"},
      {download::DOWNLOAD_INTERRUPT_REASON_FILE_HASH_MISMATCH,
       "%s - Download error", "Something went wrong"},
      {download::DOWNLOAD_INTERRUPT_REASON_NETWORK_FAILED, "%s - Network error",
       "Check internet connection"},
      {download::DOWNLOAD_INTERRUPT_REASON_NETWORK_TIMEOUT,
       "%s - Network timeout", "Check internet connection"},
      {download::DOWNLOAD_INTERRUPT_REASON_NETWORK_DISCONNECTED,
       "%s - Network disconnected", "Check internet connection"},
      {download::DOWNLOAD_INTERRUPT_REASON_NETWORK_SERVER_DOWN,
       "%s - Server unavailable", "Site wasn’t available"},
      {download::DOWNLOAD_INTERRUPT_REASON_NETWORK_INVALID_REQUEST,
       "%s - Network error", "Check internet connection"},
      {download::DOWNLOAD_INTERRUPT_REASON_SERVER_FAILED, "%s - Server problem",
       "Site wasn’t available"},
      {download::DOWNLOAD_INTERRUPT_REASON_SERVER_NO_RANGE,
       "%s - Download error", "Something went wrong"},
      {download::DOWNLOAD_INTERRUPT_REASON_SERVER_BAD_CONTENT, "%s - No file",
       "File wasn’t available on site"},
      {download::DOWNLOAD_INTERRUPT_REASON_SERVER_UNAUTHORIZED,
       "%s - Needs authorization", "File wasn’t available on site"},
      {download::DOWNLOAD_INTERRUPT_REASON_SERVER_CERT_PROBLEM,
       "%s - Bad certificate", "Site wasn’t available"},
      {download::DOWNLOAD_INTERRUPT_REASON_SERVER_FORBIDDEN, "%s - Forbidden",
       "File wasn’t available on site"},
      {download::DOWNLOAD_INTERRUPT_REASON_SERVER_UNREACHABLE,
       "%s - Server unreachable", "Site wasn’t available"},
      {download::DOWNLOAD_INTERRUPT_REASON_SERVER_CONTENT_LENGTH_MISMATCH,
       "%s - File incomplete", "Couldn’t finish download"},
      {download::DOWNLOAD_INTERRUPT_REASON_SERVER_CROSS_ORIGIN_REDIRECT,
       "%s - Download error", "Something went wrong"},
      {download::DOWNLOAD_INTERRUPT_REASON_USER_CANCELED, "Canceled"},
      {download::DOWNLOAD_INTERRUPT_REASON_USER_SHUTDOWN, "%s - Shutdown",
       "Couldn’t finish download"},
      {download::DOWNLOAD_INTERRUPT_REASON_CRASH, "%s - Crash",
       "Couldn’t finish download"},
  };
  static_assert(kInterruptReasonCount == std::size(kTestCases),
                "interrupt reason mismatch");

  SetupDownloadItemDefaults();

  const char default_failed_msg[] = "Failed";
  for (const auto& test_case : kTestCases) {
    SetupInterruptedDownloadItem(test_case.reason);
    std::string expected_status_msg = base::StringPrintfNonConstexpr(
        test_case.expected_status_msg, default_failed_msg);
    std::u16string expected_bubble_status_msg =
        base::UTF8ToUTF16(test_case.expected_bubble_status_msg);
    if (expected_bubble_status_msg.empty()) {
      expected_bubble_status_msg = base::UTF8ToUTF16(expected_status_msg);
      base::ReplaceFirstSubstringAfterOffset(&expected_bubble_status_msg, 0,
                                             u"-", u"•");
    }
    SetStatusTextBuilder(/*for_bubble=*/false);
    EXPECT_EQ(expected_status_msg, base::UTF16ToUTF8(model().GetStatusText()));

    SetStatusTextBuilder(/*for_bubble=*/true);
    EXPECT_EQ(expected_bubble_status_msg, model().GetStatusText());

#if !BUILDFLAG(IS_ANDROID)
    EXPECT_EQ(model().GetDangerUiPattern(), test_case.expected_danger_pattern);
#endif
  }
}

TEST_F(DownloadItemModelTest, InterruptTooltip) {
  // Test that we have the correct interrupt tooltip for downloads that are in
  // the INTERRUPTED state.
  const struct TestCase {
    // The reason.
    download::DownloadInterruptReason reason;

    // Expected tooltip text. The tooltip text for interrupted downloads
    // typically consist of two lines. One for the filename and one for the
    // interrupt reason. The returned string contains a newline.
    const char* expected_tooltip;
  } kTestCases[] = {
      {download::DOWNLOAD_INTERRUPT_REASON_NONE, "foo.bar"},
      {download::DOWNLOAD_INTERRUPT_REASON_FILE_FAILED,
       "foo.bar\nDownload error"},
      {download::DOWNLOAD_INTERRUPT_REASON_FILE_ACCESS_DENIED,
       "foo.bar\nInsufficient permissions"},
      {download::DOWNLOAD_INTERRUPT_REASON_FILE_NO_SPACE, "foo.bar\nDisk full"},
      {download::DOWNLOAD_INTERRUPT_REASON_FILE_NAME_TOO_LONG,
       "foo.bar\nPath too long"},
      {download::DOWNLOAD_INTERRUPT_REASON_FILE_TOO_LARGE,
       "foo.bar\nFile too large"},
      {download::DOWNLOAD_INTERRUPT_REASON_FILE_VIRUS_INFECTED,
       "foo.bar\nVirus detected"},
      {download::DOWNLOAD_INTERRUPT_REASON_FILE_BLOCKED, "foo.bar\nBlocked"},
      {download::DOWNLOAD_INTERRUPT_REASON_FILE_SECURITY_CHECK_FAILED,
       "foo.bar\nVirus scan failed"},
      {download::DOWNLOAD_INTERRUPT_REASON_FILE_TOO_SHORT,
       "foo.bar\nFile truncated"},
      {download::DOWNLOAD_INTERRUPT_REASON_FILE_SAME_AS_SOURCE,
       "foo.bar\nAlready downloaded"},
      {download::DOWNLOAD_INTERRUPT_REASON_FILE_TRANSIENT_ERROR,
       "foo.bar\nSystem busy"},
      {download::DOWNLOAD_INTERRUPT_REASON_FILE_HASH_MISMATCH,
       "foo.bar\nDownload error"},
      {download::DOWNLOAD_INTERRUPT_REASON_NETWORK_FAILED,
       "foo.bar\nNetwork error"},
      {download::DOWNLOAD_INTERRUPT_REASON_NETWORK_TIMEOUT,
       "foo.bar\nNetwork timeout"},
      {download::DOWNLOAD_INTERRUPT_REASON_NETWORK_DISCONNECTED,
       "foo.bar\nNetwork disconnected"},
      {download::DOWNLOAD_INTERRUPT_REASON_NETWORK_SERVER_DOWN,
       "foo.bar\nServer unavailable"},
      {download::DOWNLOAD_INTERRUPT_REASON_NETWORK_INVALID_REQUEST,
       "foo.bar\nNetwork error"},
      {download::DOWNLOAD_INTERRUPT_REASON_SERVER_FAILED,
       "foo.bar\nServer problem"},
      {download::DOWNLOAD_INTERRUPT_REASON_SERVER_NO_RANGE,
       "foo.bar\nDownload error"},
      {download::DOWNLOAD_INTERRUPT_REASON_SERVER_BAD_CONTENT,
       "foo.bar\nNo file"},
      {download::DOWNLOAD_INTERRUPT_REASON_SERVER_UNAUTHORIZED,
       "foo.bar\nNeeds authorization"},
      {download::DOWNLOAD_INTERRUPT_REASON_SERVER_CERT_PROBLEM,
       "foo.bar\nBad certificate"},
      {download::DOWNLOAD_INTERRUPT_REASON_SERVER_FORBIDDEN,
       "foo.bar\nForbidden"},
      {download::DOWNLOAD_INTERRUPT_REASON_SERVER_UNREACHABLE,
       "foo.bar\nServer unreachable"},
      {download::DOWNLOAD_INTERRUPT_REASON_SERVER_CONTENT_LENGTH_MISMATCH,
       "foo.bar\nFile incomplete"},
      {download::DOWNLOAD_INTERRUPT_REASON_SERVER_CROSS_ORIGIN_REDIRECT,
       "foo.bar\nDownload error"},
      {download::DOWNLOAD_INTERRUPT_REASON_USER_CANCELED, "foo.bar"},
      {download::DOWNLOAD_INTERRUPT_REASON_USER_SHUTDOWN, "foo.bar\nShutdown"},
      {download::DOWNLOAD_INTERRUPT_REASON_CRASH, "foo.bar\nCrash"},
  };
  static_assert(kInterruptReasonCount == std::size(kTestCases),
                "interrupt reason mismatch");

  SetupDownloadItemDefaults();
  for (const auto& test_case : kTestCases) {
    SetupInterruptedDownloadItem(test_case.reason);
    EXPECT_EQ(test_case.expected_tooltip,
              base::UTF16ToUTF8(model().GetTooltipText()));
  }
}

TEST_F(DownloadItemModelTest, InProgressStatus) {
  const struct TestCase {
    int64_t received_bytes;             // Return value of GetReceivedBytes().
    int64_t total_bytes;                // Return value of GetTotalBytes().
    bool  time_remaining_known;         // If TimeRemaining() is known.
    bool  open_when_complete;           // GetOpenWhenComplete().
    bool is_paused;                     // IsPaused().
    const char* expected_status_msg;    // Expected status text.
    // Expected bubble status string. This will include the progress as well.
    // If empty, use the expected_status_msg.
    std::string expected_bubble_status_msg;
  } kTestCases[] = {
      // These are all the valid combinations of the above fields for a download
      // that is in IN_PROGRESS state. Go through all of them and check the
      // return
      // value of DownloadItemModel::GetStatusText(). The point isn't to lock
      // down
      // the status strings, but to make sure we end up with something sane for
      // all the circumstances we care about.
      //
      // For GetReceivedBytes()/GetTotalBytes(), we only check whether each is
      // non-zero. In addition, if |total_bytes| is zero, then
      // |time_remaining_known| is also false.
      {0, 0, false, false, false, "Starting\xE2\x80\xA6",
       "0 B \xE2\x80\xA2 Starting\xE2\x80\xA6"},
      {1, 0, false, false, false, "1 B",
       "1 B \xE2\x80\xA2 Resuming\xE2\x80\xA6"},
      {0, 2, false, false, false, "Starting\xE2\x80\xA6",
       "0/2 B \xE2\x80\xA2 Starting\xE2\x80\xA6"},
      {1, 2, false, false, false, "1/2 B",
       "1/2 B \xE2\x80\xA2 Resuming\xE2\x80\xA6"},
      {0, 2, true, false, false, "0/2 B, 10 secs left",
       "\xE2\x86\x93 0/2 B \xE2\x80\xA2 10 seconds left"},
      {1, 2, true, false, false, "1/2 B, 10 secs left",
       "\xE2\x86\x93 1/2 B \xE2\x80\xA2 10 seconds left"},
      {0, 0, false, true, false, "Opening when complete",
       "0 B \xE2\x80\xA2 Opening when complete"},
      {1, 0, false, true, false, "Opening when complete",
       "1 B \xE2\x80\xA2 Opening when complete"},
      {0, 2, false, true, false, "Opening when complete",
       "0/2 B \xE2\x80\xA2 Opening when complete"},
      {1, 2, false, true, false, "Opening when complete",
       "1/2 B \xE2\x80\xA2 Opening when complete"},
      {0, 2, true, true, false, "Opening in 10 secs\xE2\x80\xA6",
       "\xE2\x86\x93 0/2 B \xE2\x80\xA2 Opening in 10 seconds\xE2\x80\xA6"},
      {1, 2, true, true, false, "Opening in 10 secs\xE2\x80\xA6",
       "\xE2\x86\x93 1/2 B \xE2\x80\xA2 Opening in 10 seconds\xE2\x80\xA6"},
      {0, 0, false, false, true, "0 B, Paused", "0 B \xE2\x80\xA2 Paused"},
      {1, 0, false, false, true, "1 B, Paused", "1 B \xE2\x80\xA2 Paused"},
      {0, 2, false, false, true, "0/2 B, Paused", "0/2 B \xE2\x80\xA2 Paused"},
      {1, 2, false, false, true, "1/2 B, Paused", "1/2 B \xE2\x80\xA2 Paused"},
      {0, 2, true, false, true, "0/2 B, Paused", "0/2 B \xE2\x80\xA2 Paused"},
      {1, 2, true, false, true, "1/2 B, Paused", "1/2 B \xE2\x80\xA2 Paused"},
      {0, 0, false, true, true, "0 B, Paused", "0 B \xE2\x80\xA2 Paused"},
      {1, 0, false, true, true, "1 B, Paused", "1 B \xE2\x80\xA2 Paused"},
      {0, 2, false, true, true, "0/2 B, Paused", "0/2 B \xE2\x80\xA2 Paused"},
      {1, 2, false, true, true, "1/2 B, Paused", "1/2 B \xE2\x80\xA2 Paused"},
      {0, 2, true, true, true, "0/2 B, Paused", "0/2 B \xE2\x80\xA2 Paused"},
      {1, 2, true, true, true, "1/2 B, Paused", "1/2 B \xE2\x80\xA2 Paused"},
      {5, 5, false, false, false, "", "5 B \xE2\x80\xA2 Done"}};

  SetupDownloadItemDefaults();

  for (const auto& test_case : kTestCases) {
    Mock::VerifyAndClearExpectations(&item());
    Mock::VerifyAndClearExpectations(&model());
    EXPECT_CALL(item(), GetReceivedBytes())
        .WillRepeatedly(Return(test_case.received_bytes));
    EXPECT_CALL(item(), GetTotalBytes())
        .WillRepeatedly(Return(test_case.total_bytes));
    EXPECT_CALL(item(), TimeRemaining(_))
        .WillRepeatedly(
            testing::DoAll(testing::SetArgPointee<0>(base::Seconds(10)),
                           Return(test_case.time_remaining_known)));
    EXPECT_CALL(item(), GetOpenWhenComplete())
        .WillRepeatedly(Return(test_case.open_when_complete));
    EXPECT_CALL(item(), IsPaused()).WillRepeatedly(Return(test_case.is_paused));

    SetStatusTextBuilder(/*for_bubble=*/false);
    EXPECT_EQ(test_case.expected_status_msg,
              base::UTF16ToUTF8(model().GetStatusText()));
    SetStatusTextBuilder(/*for_bubble=*/true);
    EXPECT_EQ(test_case.expected_bubble_status_msg.empty()
                  ? test_case.expected_status_msg
                  : test_case.expected_bubble_status_msg,
              base::UTF16ToUTF8(model().GetStatusText()));
#if !BUILDFLAG(IS_ANDROID)
    EXPECT_EQ(model().GetDangerUiPattern(),
              DownloadUIModel::DangerUiPattern::kNormal);
#endif
  }
}

TEST_F(DownloadItemModelTest, CompletedStatus) {
  SetupDownloadItemDefaults();

  const struct TimeElapsedTestCase {
    base::TimeDelta time_since_download_complete;
    std::string expected_status_message;
    std::string expected_bubble_status_msg;
  } kTimeElapsedTestCases[] = {
      {base::Seconds(10), "", "2 B \xE2\x80\xA2 Done"},
      {base::Seconds(50), "", "2 B \xE2\x80\xA2 Done"},
      {base::Seconds(60), "", "2 B \xE2\x80\xA2 1 minute ago"},
      {base::Hours(23), "", "2 B \xE2\x80\xA2 23 hours ago"},
      // Negative TimeDeltas may happen if the system time is adjusted
      // backwards.
      {base::Seconds(-10), "", "2 B \xE2\x80\xA2 Done"},
      {base::Minutes(-10), "", "2 B \xE2\x80\xA2 Done"},
  };
  for (const auto& test_case : kTimeElapsedTestCases) {
    SetupCompletedDownloadItem(test_case.time_since_download_complete);
    SetStatusTextBuilder(/*for_bubble=*/false);
    EXPECT_EQ(base::UTF16ToUTF8(model().GetStatusText()),
              test_case.expected_status_message);
    SetStatusTextBuilder(/*for_bubble=*/true);
    EXPECT_EQ(base::UTF16ToUTF8(model().GetStatusText()),
              test_case.expected_bubble_status_msg);
#if !BUILDFLAG(IS_ANDROID)
    EXPECT_EQ(model().GetDangerUiPattern(),
              DownloadUIModel::DangerUiPattern::kNormal);
#endif
  }

  EXPECT_CALL(item(), GetDangerType())
      .WillRepeatedly(Return(download::DOWNLOAD_DANGER_TYPE_DEEP_SCANNED_SAFE));
  EXPECT_EQ("2 B \xE2\x80\xA2 Scan is done",
            base::UTF16ToUTF8(model().GetStatusText()));
#if !BUILDFLAG(IS_ANDROID)
  EXPECT_EQ(model().GetDangerUiPattern(),
            DownloadUIModel::DangerUiPattern::kNormal);
#endif

#if BUILDFLAG(IS_MAC)
  EXPECT_EQ("Show in Finder", base::UTF16ToUTF8(model().GetShowInFolderText()));
#else  // BUILDFLAG(IS_MAC)
  EXPECT_EQ("Show in folder", base::UTF16ToUTF8(model().GetShowInFolderText()));
#endif
}

TEST_F(DownloadItemModelTest, CompletedBubbleWarningStatusText) {
  SetupCompletedDownloadItem(base::Hours(1));
  SetStatusTextBuilder(/*for_bubble=*/true);

  const struct InsecureDownloadStatusTestCase {
    download::DownloadItem::InsecureDownloadStatus insecure_download_status;
    std::string expected_bubble_status_msg;
  } kInsecureDownloadStatusTestCases[] = {
      {download::DownloadItem::InsecureDownloadStatus::BLOCK,
       "Insecure download blocked"},
      {download::DownloadItem::InsecureDownloadStatus::WARN,
       "Insecure download blocked"},
  };
  for (const auto& test_case : kInsecureDownloadStatusTestCases) {
    SetupDownloadItemDefaults();
    ON_CALL(item(), GetInsecureDownloadStatus())
        .WillByDefault(Return(test_case.insecure_download_status));
    EXPECT_EQ(base::UTF16ToUTF8(model().GetStatusText()),
              test_case.expected_bubble_status_msg);
#if !BUILDFLAG(IS_ANDROID)
    EXPECT_EQ(model().GetDangerUiPattern(),
              DownloadUIModel::DangerUiPattern::kSuspicious);
#endif
  }

  const struct DangerTypeTestCase {
    download::DownloadDangerType danger_type;
    std::string expected_bubble_status_msg;
    DownloadUIModel::DangerUiPattern expected_danger_pattern;
  } kDangerTypeTestCases[] = {
      {download::DOWNLOAD_DANGER_TYPE_DANGEROUS_CONTENT,
       "Dangerous download blocked",
       DownloadUIModel::DangerUiPattern::kDangerous},
      {download::DOWNLOAD_DANGER_TYPE_DANGEROUS_HOST,
       "Dangerous download blocked",
       DownloadUIModel::DangerUiPattern::kDangerous},
      {download::DOWNLOAD_DANGER_TYPE_DANGEROUS_ACCOUNT_COMPROMISE,
       "Dangerous download blocked",
       DownloadUIModel::DangerUiPattern::kDangerous},
      {download::DOWNLOAD_DANGER_TYPE_POTENTIALLY_UNWANTED,
       "Dangerous download blocked",
       DownloadUIModel::DangerUiPattern::kDangerous},
      {download::DOWNLOAD_DANGER_TYPE_BLOCKED_PASSWORD_PROTECTED,
       "Blocked \xE2\x80\xA2 Encrypted",
       DownloadUIModel::DangerUiPattern::kOther},
      {download::DOWNLOAD_DANGER_TYPE_DANGEROUS_URL,
       "Dangerous download blocked",
       DownloadUIModel::DangerUiPattern::kDangerous},
      {download::DOWNLOAD_DANGER_TYPE_BLOCKED_TOO_LARGE,
       "Blocked \xE2\x80\xA2 Too big",
       DownloadUIModel::DangerUiPattern::kOther},
      {download::DOWNLOAD_DANGER_TYPE_SENSITIVE_CONTENT_WARNING,
       "Sensitive content", DownloadUIModel::DangerUiPattern::kOther},
      {download::DOWNLOAD_DANGER_TYPE_SENSITIVE_CONTENT_BLOCK,
       "Blocked by your organization",
       DownloadUIModel::DangerUiPattern::kOther},
      {download::DOWNLOAD_DANGER_TYPE_PROMPT_FOR_SCANNING,
       "Scan for malware \xE2\x80\xA2 Suspicious",
       DownloadUIModel::DangerUiPattern::kSuspicious},
  };
  for (const auto& test_case : kDangerTypeTestCases) {
    SCOPED_TRACE(testing::Message()
                 << "Failed for danger type "
                 << download::GetDownloadDangerTypeString(test_case.danger_type)
                 << std::endl);
    SetupDownloadItemDefaults();
    ON_CALL(item(), GetDangerType())
        .WillByDefault(Return(test_case.danger_type));
    EXPECT_EQ(base::UTF16ToUTF8(model().GetStatusText()),
              test_case.expected_bubble_status_msg);
#if !BUILDFLAG(IS_ANDROID)
    EXPECT_EQ(model().GetDangerUiPattern(), test_case.expected_danger_pattern);
#endif
  }
}

TEST_F(DownloadItemModelTest,
       CompletedBubbleWarningStatusText_FiletypeWarning) {
  SetupCompletedDownloadItem(base::Hours(1));
  SetStatusTextBuilder(/*for_bubble=*/true);
  SetupDownloadItemDefaults();
  ON_CALL(item(), GetDangerType())
      .WillByDefault(Return(download::DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE));
  EXPECT_EQ(base::UTF16ToUTF8(model().GetStatusText()),
            "Unverified download blocked");
#if !BUILDFLAG(IS_ANDROID)
  EXPECT_EQ(model().GetDangerUiPattern(),
            DownloadUIModel::DangerUiPattern::kSuspicious);
#endif

#if !BUILDFLAG(IS_ANDROID) && BUILDFLAG(FULL_SAFE_BROWSING)
  // It doesn't matter what the DownloadProtectionData is; just that it is
  // present.
  std::string token = "token";
  safe_browsing::ClientDownloadResponse::Verdict verdict =
      safe_browsing::ClientDownloadResponse::SAFE;
  safe_browsing::ClientDownloadResponse::TailoredVerdict tailored_verdict;
  safe_browsing::DownloadProtectionService::SetDownloadProtectionData(
      &item(), token, verdict, tailored_verdict);

  EXPECT_EQ(base::UTF16ToUTF8(model().GetStatusText()),
            "Suspicious download blocked");
#endif  // !BUILDFLAG(IS_ANDROID) && BUILDFLAG(FULL_SAFE_BROWSING)
}

#if !BUILDFLAG(IS_ANDROID)

TEST_F(DownloadItemModelTest, ShouldPreferOpeningInBrowser) {
  SetupDownloadItemDefaults();
  SetupCompletedDownloadItem(base::Hours(1));
  EXPECT_TRUE(model().ShouldPreferOpeningInBrowser());
}

TEST_F(DownloadItemModelTest, ShouldShowInBubble) {
  auto in_progress = DownloadItem::IN_PROGRESS;
  auto canceled = DownloadItem::CANCELLED;
  auto never = std::optional<base::Time>();
  auto two_mins_ago = base::Time::Now() - base::Minutes(2);
  auto ten_mins_ago = base::Time::Now() - base::Minutes(10);
  auto dangerous_file = download::DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE;
  auto not_dangerous = download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS;

  const struct TestCase {
    DownloadItem::DownloadState state;
    download::DownloadDangerType danger_type;
    std::optional<base::Time> shown_time;
    bool expected_should_show;
  } kTestCases[] = {
      {in_progress, not_dangerous, two_mins_ago, true},
      {in_progress, not_dangerous, ten_mins_ago, true},
      {in_progress, dangerous_file, never, true},
      {in_progress, dangerous_file, two_mins_ago, true},
      {in_progress, dangerous_file, ten_mins_ago, false},
      {canceled, dangerous_file, two_mins_ago, false},
      {canceled, dangerous_file, ten_mins_ago, false},
      {canceled, not_dangerous, two_mins_ago, true},
      {canceled, not_dangerous, ten_mins_ago, true},
  };

  SetupDownloadItemDefaults();
  for (const auto& test_case : kTestCases) {
    EXPECT_CALL(item(), GetState()).WillRepeatedly(Return(test_case.state));
    EXPECT_CALL(item(), GetDangerType())
        .WillRepeatedly(Return(test_case.danger_type));
    model().SetEphemeralWarningUiShownTime(test_case.shown_time);

    EXPECT_EQ(test_case.expected_should_show, model().ShouldShowInBubble());
  }
}

TEST_F(DownloadItemModelTest, GetBubbleStatusMessageWithBytes) {
  auto compare_results = [](std::u16string actual, std::vector<int> expected) {
    EXPECT_EQ(actual.length(), expected.size());
    for (auto it = expected.begin(); it < expected.end(); it++) {
      int index = std::distance(expected.begin(), it);
      EXPECT_EQ(actual[index], expected[index]);
    }
  };

  base::i18n::SetRTLForTesting(true);

  // Arabic
  auto* arabic_bytes = L"5 \x062A";
  auto* arabic_status = L"\x0645";
  std::u16string arabic =
      DownloadUIModel::BubbleStatusTextBuilder::GetBubbleStatusMessageWithBytes(
          base::WideToUTF16(arabic_bytes), base::WideToUTF16(arabic_status),
          false);
  std::vector<int> expected_arabic =
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_POSIX)
      {8207, 8235, 53, 32, 1578, 32, 8226, 32, 1605, 8236, 8207};
#else
      {8235, 53, 32, 1578, 32, 8226, 32, 1605, 8236};
#endif
  compare_results(arabic, expected_arabic);

  // Hebrew
  auto* hebrew_status = L"\x05D0";
  std::u16string hebrew = DownloadUIModel::BubbleStatusTextBuilder ::
      GetBubbleStatusMessageWithBytes(u"5 MB", base::WideToUTF16(hebrew_status),
                                      false);
  std::vector<int> expected_hebrew =
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_POSIX)
      {8207, 8235, 8234, 53, 32, 77, 66, 8236, 32, 8226, 32, 1488, 8236, 8207};
#else
      {8235, 8234, 53, 32, 77, 66, 8236, 32, 8226, 32, 1488, 8236};
#endif
  compare_results(hebrew, expected_hebrew);

  // English
  base::i18n::SetRTLForTesting(false);
  std::u16string english = DownloadUIModel::BubbleStatusTextBuilder ::
      GetBubbleStatusMessageWithBytes(u"5 MB", u"A", false);
  std::vector<int> expected_english = {53, 32, 77, 66, 32, 8226, 32, 65};
  compare_results(english, expected_english);
}
#endif  // !BUILDFLAG(IS_ANDROID)

TEST_F(DownloadItemModelTest, ShouldShowInShelf) {
  SetupDownloadItemDefaults();

  // By default the download item should be displayable on the shelf when it is
  // not a transient download.
  EXPECT_CALL(item(), IsTransient()).WillOnce(Return(false));
  EXPECT_TRUE(model().ShouldShowInShelf());

  EXPECT_CALL(item(), IsTransient()).WillOnce(Return(true));
  EXPECT_FALSE(model().ShouldShowInShelf());

  // Once explicitly set, ShouldShowInShelf() should return the explicit value
  // regardless of whether it's a transient download, which should no longer
  // be considered by the model after initializing it.
  EXPECT_CALL(item(), IsTransient()).Times(1);

  model().SetShouldShowInShelf(true);
  EXPECT_TRUE(model().ShouldShowInShelf());

  model().SetShouldShowInShelf(false);
  EXPECT_FALSE(model().ShouldShowInShelf());
}

TEST_F(DownloadItemModelTest, DangerLevel) {
  SetupDownloadItemDefaults();

  // Default danger level is NOT_DANGEROUS.
  EXPECT_EQ(DownloadFileType::NOT_DANGEROUS, model().GetDangerLevel());

  model().SetDangerLevel(DownloadFileType::ALLOW_ON_USER_GESTURE);
  EXPECT_EQ(DownloadFileType::ALLOW_ON_USER_GESTURE, model().GetDangerLevel());
}

TEST_F(DownloadItemModelTest, HasSupportedImageMimeType) {
  SetupDownloadItemDefaults();

  // When the item has a supported image MIME type, true should be returned.
  ON_CALL(item(), GetMimeType()).WillByDefault(Return("image/png"));
  EXPECT_TRUE(model().HasSupportedImageMimeType());

  // An unsupported MIME type should result in false being returned...
  ON_CALL(item(), GetMimeType()).WillByDefault(Return("image/unsupported"));
  EXPECT_FALSE(model().HasSupportedImageMimeType());

  // ... unless the target path has a well-known image extension.
  const base::FilePath kImagePath(FILE_PATH_LITERAL("/foo/image.png"));
  ON_CALL(item(), GetTargetFilePath()).WillByDefault(ReturnRef(kImagePath));
  EXPECT_TRUE(model().HasSupportedImageMimeType());

  // .txt and missing extensions should also result in false being returned.
  const base::FilePath kTextPath(FILE_PATH_LITERAL("/foo/image.txt"));
  ON_CALL(item(), GetTargetFilePath()).WillByDefault(ReturnRef(kTextPath));
  EXPECT_FALSE(model().HasSupportedImageMimeType());

  const base::FilePath kNoExtensionPath(FILE_PATH_LITERAL("/foo/image."));
  ON_CALL(item(), GetTargetFilePath())
      .WillByDefault(ReturnRef(kNoExtensionPath));
  EXPECT_FALSE(model().HasSupportedImageMimeType());
}

TEST_F(DownloadItemModelTest, ShouldRemoveFromShelfWhenComplete) {
  const struct TestCase {
    DownloadItem::DownloadState state;
    bool is_dangerous;  // Expectation for IsDangerous().
    bool is_auto_open;  // Expectation for GetOpenWhenComplete().
    bool auto_opened;   // Whether the download was successfully
                        // auto-opened. Expecation for GetAutoOpened().
    bool expected_result;
  } kTestCases[] = {
    // All the valid combinations of state, is_dangerous, is_auto_open and
    // auto_opened.
    //
    //                              .--- Is dangerous.
    //                             |       .--- Auto open or temporary.
    //                             |      |      .--- Auto opened.
    //                             |      |      |      .--- Expected result.
    { DownloadItem::IN_PROGRESS, false, false, false, false},
    { DownloadItem::IN_PROGRESS, false, true , false, true },
    { DownloadItem::IN_PROGRESS, true , false, false, false},
    { DownloadItem::IN_PROGRESS, true , true , false, false},
    { DownloadItem::COMPLETE,    false, false, false, false},
    { DownloadItem::COMPLETE,    false, true , false, false},
    { DownloadItem::COMPLETE,    false, false, true , true },
    { DownloadItem::COMPLETE,    false, true , true , true },
    { DownloadItem::CANCELLED,   false, false, false, false},
    { DownloadItem::CANCELLED,   false, true , false, false},
    { DownloadItem::CANCELLED,   true , false, false, false},
    { DownloadItem::CANCELLED,   true , true , false, false},
    { DownloadItem::INTERRUPTED, false, false, false, false},
    { DownloadItem::INTERRUPTED, false, true , false, false},
    { DownloadItem::INTERRUPTED, true , false, false, false},
    { DownloadItem::INTERRUPTED, true , true , false, false}
  };

  SetupDownloadItemDefaults();

  for (const auto& test_case : kTestCases) {
    EXPECT_CALL(item(), GetOpenWhenComplete())
        .WillRepeatedly(Return(test_case.is_auto_open));
    EXPECT_CALL(item(), GetState())
        .WillRepeatedly(Return(test_case.state));
    EXPECT_CALL(item(), IsDangerous())
        .WillRepeatedly(Return(test_case.is_dangerous));
    EXPECT_CALL(item(), GetAutoOpened())
        .WillRepeatedly(Return(test_case.auto_opened));

    EXPECT_EQ(test_case.expected_result,
              model().ShouldRemoveFromShelfWhenComplete());
    Mock::VerifyAndClearExpectations(&item());
    Mock::VerifyAndClearExpectations(&model());
  }
}

TEST_F(DownloadItemModelTest, ShouldShowDropdown) {
  // A few aliases for DownloadDangerTypes since the full names are fairly
  // verbose.
  download::DownloadDangerType safe =
      download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS;
  download::DownloadDangerType dangerous_file =
      download::DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE;
  download::DownloadDangerType dangerous_content =
      download::DOWNLOAD_DANGER_TYPE_DANGEROUS_CONTENT;
  download::DownloadDangerType dangerous_account_compromise =
      download::DOWNLOAD_DANGER_TYPE_DANGEROUS_ACCOUNT_COMPROMISE;
  download::DownloadDangerType blocked_encrypted =
      download::DOWNLOAD_DANGER_TYPE_BLOCKED_PASSWORD_PROTECTED;
  download::DownloadDangerType blocked_too_large =
      download::DOWNLOAD_DANGER_TYPE_BLOCKED_TOO_LARGE;
  download::DownloadDangerType blocked_sensitive =
      download::DOWNLOAD_DANGER_TYPE_SENSITIVE_CONTENT_BLOCK;

  const struct TestCase {
    DownloadItem::DownloadState state;        // Expectation for GetState()
    download::DownloadDangerType danger_type; // Expectation for GetDangerType()
    bool is_dangerous;        // Expectation for IsDangerous()
    bool expected_result;
  } kTestCases[] = {
      //                                            .--- Is dangerous.
      // Download state         Danger type         |      .--- Expected result.
      {DownloadItem::COMPLETE,  safe,              false, true},
      {DownloadItem::COMPLETE,  dangerous_file,    true,  false},
      {DownloadItem::CANCELLED, dangerous_file,    true,  true},
      {DownloadItem::COMPLETE,  dangerous_account_compromise, true, true},
      {DownloadItem::COMPLETE,  dangerous_content, true,  true},
      {DownloadItem::COMPLETE,  blocked_encrypted, true,  false},
      {DownloadItem::COMPLETE,  blocked_too_large, true,  false},
      {DownloadItem::COMPLETE,  blocked_sensitive, true,  false},
  };

  SetupDownloadItemDefaults();

  for (const auto& test_case : kTestCases) {
    EXPECT_CALL(item(), GetState()).WillRepeatedly(Return(test_case.state));
    EXPECT_CALL(item(), GetDangerType())
        .WillRepeatedly(Return(test_case.danger_type));
    EXPECT_CALL(item(), IsDangerous())
        .WillRepeatedly(Return(test_case.is_dangerous));

    EXPECT_EQ(test_case.expected_result, model().ShouldShowDropdown());
    Mock::VerifyAndClearExpectations(&item());
    Mock::VerifyAndClearExpectations(&model());
  }
}

TEST_F(DownloadItemModelTest, RenamingProgress) {
  FakeRenameHandler fake_handler(&item());
  EXPECT_CALL(item(), GetRenameHandler()).WillRepeatedly(Return(&fake_handler));
  EXPECT_CALL(item(), GetReceivedBytes()).WillRepeatedly(Return(10));
  EXPECT_CALL(item(), GetUploadedBytes()).WillRepeatedly(Return(2));
  EXPECT_CALL(item(), GetTotalBytes()).WillRepeatedly(Return(10));

  EXPECT_EQ(6, model().GetCompletedBytes());
  EXPECT_EQ(60, model().PercentComplete());
}

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH)
class DownloadItemModelTailoredWarningTest : public DownloadItemModelTest {
 public:
  DownloadItemModelTailoredWarningTest() {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    scoped_feature_list_.InitAndEnableFeature(
        safe_browsing::kDownloadTailoredWarnings);
  }

  ~DownloadItemModelTailoredWarningTest() override = default;

 protected:
  void SetupTailoredWarningForItem(
      download::DownloadDangerType danger_type,
      TailoredVerdict::TailoredVerdictType tailored_verdict_type,
      std::vector<TailoredVerdict::ExperimentalWarningAdjustment> adjustments) {
    ON_CALL(item(), GetDangerType()).WillByDefault(Return(danger_type));
    TailoredVerdict tailored_verdict;
    tailored_verdict.set_tailored_verdict_type(tailored_verdict_type);
    for (const auto& adjustment : adjustments) {
      tailored_verdict.add_adjustments(adjustment);
    }
    safe_browsing::DownloadProtectionService::SetDownloadProtectionData(
        &item(), "token",
        safe_browsing::ClientDownloadResponse::SAFE,  // placeholder
        tailored_verdict);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(DownloadItemModelTailoredWarningTest, GetTailoredWarningType) {
  SetupDownloadItemDefaults();

  const struct GetTailoredWarningTypeTestCase {
    download::DownloadDangerType danger_type;
    TailoredVerdict::TailoredVerdictType tailored_verdict_type;
    DownloadUIModel::TailoredWarningType expected_warning_type;
    DownloadUIModel::DangerUiPattern expected_danger_pattern;
  } kShouldShowTailoredWarningTestCases[] = {
      {download::DOWNLOAD_DANGER_TYPE_DANGEROUS_ACCOUNT_COMPROMISE,
       TailoredVerdict::COOKIE_THEFT,
       DownloadUIModel::TailoredWarningType::kCookieTheft,
       DownloadUIModel::DangerUiPattern::kDangerous},
      {download::DOWNLOAD_DANGER_TYPE_UNCOMMON_CONTENT,
       TailoredVerdict::SUSPICIOUS_ARCHIVE,
       DownloadUIModel::TailoredWarningType::kSuspiciousArchive,
       DownloadUIModel::DangerUiPattern::kSuspicious},
      {download::DOWNLOAD_DANGER_TYPE_DANGEROUS_URL,
       TailoredVerdict::COOKIE_THEFT,
       DownloadUIModel::TailoredWarningType::kNoTailoredWarning,
       // This is dangerous despite kNoTailoredWarning, because the base
       // danger_type is dangerous.
       DownloadUIModel::DangerUiPattern::kDangerous},
      {download::DOWNLOAD_DANGER_TYPE_POTENTIALLY_UNWANTED,
       TailoredVerdict::SUSPICIOUS_ARCHIVE,
       DownloadUIModel::TailoredWarningType::kNoTailoredWarning,
       DownloadUIModel::DangerUiPattern::kDangerous},
  };
  for (const auto& test_case : kShouldShowTailoredWarningTestCases) {
    SCOPED_TRACE(::testing::Message()
                 << "danger_type "
                 << GetDownloadDangerTypeString(test_case.danger_type));
    SetupTailoredWarningForItem(test_case.danger_type,
                                test_case.tailored_verdict_type,
                                /*adjustments=*/{});
    EXPECT_EQ(model().GetTailoredWarningType(),
              test_case.expected_warning_type);
    EXPECT_EQ(model().GetDangerUiPattern(), test_case.expected_danger_pattern);
  }

  SetupTailoredWarningForItem(
      download::DOWNLOAD_DANGER_TYPE_DANGEROUS_ACCOUNT_COMPROMISE,
      TailoredVerdict::COOKIE_THEFT,
      /*adjustments=*/{TailoredVerdict::ACCOUNT_INFO_STRING});
  EXPECT_EQ(model().GetTailoredWarningType(),
            DownloadUIModel::TailoredWarningType::kCookieTheftWithAccountInfo);
  EXPECT_EQ(model().GetDangerUiPattern(),
            DownloadUIModel::DangerUiPattern::kDangerous);
}

class DownloadItemModelTailoredWarningDisabledTest
    : public DownloadItemModelTailoredWarningTest {
 public:
  DownloadItemModelTailoredWarningDisabledTest() {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    scoped_feature_list_.InitAndDisableFeature(
        safe_browsing::kDownloadTailoredWarnings);
  }

  ~DownloadItemModelTailoredWarningDisabledTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(DownloadItemModelTailoredWarningDisabledTest,
       GetBubbleUIInfoForTailoredWarning_Disabled) {
  SetupDownloadItemDefaults();
  SetupTailoredWarningForItem(
      download::DOWNLOAD_DANGER_TYPE_DANGEROUS_ACCOUNT_COMPROMISE,
      TailoredVerdict::COOKIE_THEFT, /*adjustments=*/{});
  EXPECT_EQ(model().GetTailoredWarningType(),
            DownloadUIModel::TailoredWarningType::kNoTailoredWarning);
  // This is dangerous despite kNoTailoredWarning, because the base
  // danger_type is dangerous.
  EXPECT_EQ(model().GetDangerUiPattern(),
            DownloadUIModel::DangerUiPattern::kDangerous);
}

#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH)
