// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_item_model.h"

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

#include "base/check_op.h"
#include "base/cxx17_backports.h"
#include "base/i18n/rtl.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/download/public/common/mock_download_item.h"
#include "components/enterprise/common/proto/download_item_reroute_info.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/text/bytes_formatting.h"

using download::DownloadItem;
using safe_browsing::DownloadFileType;
using ::testing::Mock;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::ReturnRefOfCopy;
using ::testing::SetArgPointee;
using ::testing::_;

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
const size_t kInterruptReasonCount = base::size(kInterruptReasonCounter);

// Default target path for a mock download item in DownloadItemModelTest.
const base::FilePath::CharType kDefaultTargetFilePath[] =
    FILE_PATH_LITERAL("/foo/bar/foo.bar");

const base::FilePath::CharType kDefaultDisplayFileName[] =
    FILE_PATH_LITERAL("foo.bar");

// Default URL for a mock download item in DownloadItemModelTest.
const char kDefaultURL[] = "http://example.com/foo.bar";

// Constants and helper functions to test rerouted items.
using Provider = enterprise_connectors::FileSystemServiceProvider;
using RerouteInfo = enterprise_connectors::DownloadItemRerouteInfo;
const char kTestProviderDisplayName[] = "Box";
const char kTestProviderErrorMessage[] = "400 - \"item_name_invalid\"";
const char kTestProviderAdditionalMessage[] = "abcdefg";
const Provider kTestProvider = Provider::BOX;
RerouteInfo MakeTestRerouteInfo(Provider provider) {
  RerouteInfo info;
  info.set_service_provider(provider);
  switch (provider) {
    case (Provider::BOX):
      info.mutable_box()->set_error_message(kTestProviderErrorMessage);
      info.mutable_box()->set_additional_message(
          kTestProviderAdditionalMessage);
      break;
    default:
      NOTREACHED();
  }
  return info;
}
const RerouteInfo kTestRerouteInfo = MakeTestRerouteInfo(kTestProvider);

class DownloadItemModelTest : public testing::Test {
 public:
  DownloadItemModelTest()
      : model_(&item_) {}

  ~DownloadItemModelTest() override {}

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
    ON_CALL(item_, GetRerouteInfo())
        .WillByDefault(ReturnRefOfCopy(RerouteInfo()));
    ON_CALL(item_, GetTargetDisposition())
        .WillByDefault(
            Return(DownloadItem::TARGET_DISPOSITION_OVERWRITE));
    ON_CALL(item_, IsPaused()).WillByDefault(Return(false));
  }

  void SetupInterruptedDownloadItem(download::DownloadInterruptReason reason) {
    EXPECT_CALL(item_, GetLastReason()).WillRepeatedly(Return(reason));
    EXPECT_CALL(item_, GetState())
        .WillRepeatedly(
            Return((reason == download::DOWNLOAD_INTERRUPT_REASON_NONE)
                       ? DownloadItem::IN_PROGRESS
                       : DownloadItem::INTERRUPTED));
  }

  void SetupCompletedDownloadItem() {
    ON_CALL(item_, GetFileExternallyRemoved()).WillByDefault(Return(false));
    EXPECT_CALL(item_, GetState())
        .WillRepeatedly(Return(DownloadItem::COMPLETE));
  }

  download::MockDownloadItem& item() { return item_; }

  DownloadItemModel& model() {
    return model_;
  }

 private:
  NiceMock<download::MockDownloadItem> item_;
  DownloadItemModel model_;
};

}  // namespace

TEST_F(DownloadItemModelTest, InterruptedStatus) {
  // Test that we have the correct interrupt status message for downloads that
  // are in the INTERRUPTED state.
  const struct TestCase {
    // The reason.
    download::DownloadInterruptReason reason;

    // Expected status string. This will include the progress as well.
    const char* expected_status_msg;
  } kTestCases[] = {
      {download::DOWNLOAD_INTERRUPT_REASON_NONE, "1/2 B"},
      {download::DOWNLOAD_INTERRUPT_REASON_FILE_FAILED, "%s - Download error"},
      {download::DOWNLOAD_INTERRUPT_REASON_FILE_ACCESS_DENIED,
       "%s - Insufficient permissions"},
      {download::DOWNLOAD_INTERRUPT_REASON_FILE_NO_SPACE, "%s - Disk full"},
      {download::DOWNLOAD_INTERRUPT_REASON_FILE_NAME_TOO_LONG,
       "%s - Path too long"},
      {download::DOWNLOAD_INTERRUPT_REASON_FILE_TOO_LARGE,
       "%s - File too large"},
      {download::DOWNLOAD_INTERRUPT_REASON_FILE_VIRUS_INFECTED,
       "%s - Virus detected"},
      {download::DOWNLOAD_INTERRUPT_REASON_FILE_BLOCKED, "%s - Blocked"},
      {download::DOWNLOAD_INTERRUPT_REASON_FILE_SECURITY_CHECK_FAILED,
       "%s - Virus scan failed"},
      {download::DOWNLOAD_INTERRUPT_REASON_FILE_TOO_SHORT,
       "%s - File truncated"},
      {download::DOWNLOAD_INTERRUPT_REASON_FILE_SAME_AS_SOURCE,
       "%s - Already downloaded"},
      {download::DOWNLOAD_INTERRUPT_REASON_FILE_TRANSIENT_ERROR,
       "%s - System busy"},
      {download::DOWNLOAD_INTERRUPT_REASON_FILE_HASH_MISMATCH,
       "%s - Download error"},
      {download::DOWNLOAD_INTERRUPT_REASON_NETWORK_FAILED,
       "%s - Network error"},
      {download::DOWNLOAD_INTERRUPT_REASON_NETWORK_TIMEOUT,
       "%s - Network timeout"},
      {download::DOWNLOAD_INTERRUPT_REASON_NETWORK_DISCONNECTED,
       "%s - Network disconnected"},
      {download::DOWNLOAD_INTERRUPT_REASON_NETWORK_SERVER_DOWN,
       "%s - Server unavailable"},
      {download::DOWNLOAD_INTERRUPT_REASON_NETWORK_INVALID_REQUEST,
       "%s - Network error"},
      {download::DOWNLOAD_INTERRUPT_REASON_SERVER_FAILED,
       "%s - Server problem"},
      {download::DOWNLOAD_INTERRUPT_REASON_SERVER_NO_RANGE,
       "%s - Download error"},
      {download::DOWNLOAD_INTERRUPT_REASON_SERVER_BAD_CONTENT, "%s - No file"},
      {download::DOWNLOAD_INTERRUPT_REASON_SERVER_UNAUTHORIZED,
       "%s - Needs authorization"},
      {download::DOWNLOAD_INTERRUPT_REASON_SERVER_CERT_PROBLEM,
       "%s - Bad certificate"},
      {download::DOWNLOAD_INTERRUPT_REASON_SERVER_FORBIDDEN, "%s - Forbidden"},
      {download::DOWNLOAD_INTERRUPT_REASON_SERVER_UNREACHABLE,
       "%s - Server unreachable"},
      {download::DOWNLOAD_INTERRUPT_REASON_SERVER_CONTENT_LENGTH_MISMATCH,
       "%s - File incomplete"},
      {download::DOWNLOAD_INTERRUPT_REASON_SERVER_CROSS_ORIGIN_REDIRECT,
       "%s - Download error"},
      {download::DOWNLOAD_INTERRUPT_REASON_USER_CANCELED, "Canceled"},
      {download::DOWNLOAD_INTERRUPT_REASON_USER_SHUTDOWN, "%s - Shutdown"},
      {download::DOWNLOAD_INTERRUPT_REASON_CRASH, "%s - Crash"},
  };
  static_assert(kInterruptReasonCount == base::size(kTestCases),
                "interrupt reason mismatch");

  SetupDownloadItemDefaults();

  const char default_failed_msg[] = "Failed";
  for (const auto& test_case : kTestCases) {
    SetupInterruptedDownloadItem(test_case.reason);
    std::string expected_status_msg =
        base::StringPrintf(test_case.expected_status_msg, default_failed_msg);
    EXPECT_EQ(expected_status_msg, base::UTF16ToUTF8(model().GetStatusText()));
  }

  const std::string provider_failed_msg =
      base::StringPrintf("Failed to save to %s", kTestProviderDisplayName);
  for (const auto& test_case : kTestCases) {
    SetupInterruptedDownloadItem(test_case.reason);
    EXPECT_CALL(item(), GetRerouteInfo())
        .WillRepeatedly(ReturnRef(kTestRerouteInfo));
    std::string expected_status_msg, expected_history_page_text;
    if (test_case.reason == download::DOWNLOAD_INTERRUPT_REASON_SERVER_FAILED) {
      expected_status_msg =
          provider_failed_msg + " - " + kTestProviderErrorMessage;
      expected_history_page_text =
          expected_status_msg + " (" + kTestProviderAdditionalMessage + ")";
    } else {
      expected_status_msg = base::StringPrintf(test_case.expected_status_msg,
                                               provider_failed_msg.c_str());
      expected_history_page_text = expected_status_msg;
    }
    EXPECT_EQ(expected_status_msg, base::UTF16ToUTF8(model().GetStatusText()));
    EXPECT_EQ(expected_history_page_text,
              base::UTF16ToUTF8(model().GetHistoryPageStatusText()));
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
  static_assert(kInterruptReasonCount == base::size(kTestCases),
                "interrupt reason mismatch");

  SetupDownloadItemDefaults();
  for (const auto& test_case : kTestCases) {
    SetupInterruptedDownloadItem(test_case.reason);
    EXPECT_EQ(test_case.expected_tooltip,
              base::UTF16ToUTF8(model().GetTooltipText()));
  }
}

TEST_F(DownloadItemModelTest, InProgressStatus) {
  const std::string provider_sending_str =
      base::StringPrintf("Sending to %s", kTestProviderDisplayName);
  const char* reroute_status = provider_sending_str.c_str();
  const struct TestCase {
    int64_t received_bytes;             // Return value of GetReceivedBytes().
    int64_t total_bytes;                // Return value of GetTotalBytes().
    bool  time_remaining_known;         // If TimeRemaining() is known.
    bool  open_when_complete;           // GetOpenWhenComplete().
    bool  is_paused;                    // IsPaused().
    const RerouteInfo reroute_info;     // GetRerouteInfo().
    const char* expected_status_msg;    // Expected status text.
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
      //
      //         .-- .TimeRemaining() is known.
      //        |       .-- .GetOpenWhenComplete()
      //        |      |      .---- .IsPaused()
      //        |      |     |     .---- .GetRerouteInfo()
      {0, 0, false, false, false, {}, "Starting\xE2\x80\xA6"},
      {1, 0, false, false, false, {}, "1 B"},
      {0, 2, false, false, false, {}, "Starting\xE2\x80\xA6"},
      {1, 2, false, false, false, {}, "1/2 B"},
      {0, 2, true, false, false, {}, "0/2 B, 10 secs left"},
      {1, 2, true, false, false, {}, "1/2 B, 10 secs left"},
      {0, 0, false, true, false, {}, "Opening when complete"},
      {1, 0, false, true, false, {}, "Opening when complete"},
      {0, 2, false, true, false, {}, "Opening when complete"},
      {1, 2, false, true, false, {}, "Opening when complete"},
      {0, 2, true, true, false, {}, "Opening in 10 secs\xE2\x80\xA6"},
      {1, 2, true, true, false, {}, "Opening in 10 secs\xE2\x80\xA6"},
      {0, 0, false, false, true, {}, "0 B, Paused"},
      {1, 0, false, false, true, {}, "1 B, Paused"},
      {0, 2, false, false, true, {}, "0/2 B, Paused"},
      {1, 2, false, false, true, {}, "1/2 B, Paused"},
      {0, 2, true, false, true, {}, "0/2 B, Paused"},
      {1, 2, true, false, true, {}, "1/2 B, Paused"},
      {0, 0, false, true, true, {}, "0 B, Paused"},
      {1, 0, false, true, true, {}, "1 B, Paused"},
      {0, 2, false, true, true, {}, "0/2 B, Paused"},
      {1, 2, false, true, true, {}, "1/2 B, Paused"},
      {0, 2, true, true, true, {}, "0/2 B, Paused"},
      {1, 2, true, true, true, {}, "1/2 B, Paused"},
      {5, 5, true, true, false, kTestRerouteInfo, reroute_status}};

  SetupDownloadItemDefaults();

  for (const auto& test_case : kTestCases) {
    Mock::VerifyAndClearExpectations(&item());
    Mock::VerifyAndClearExpectations(&model());
    EXPECT_CALL(item(), GetReceivedBytes())
        .WillRepeatedly(Return(test_case.received_bytes));
    EXPECT_CALL(item(), GetTotalBytes())
        .WillRepeatedly(Return(test_case.total_bytes));
    EXPECT_CALL(item(), TimeRemaining(_))
        .WillRepeatedly(testing::DoAll(
            testing::SetArgPointee<0>(base::TimeDelta::FromSeconds(10)),
            Return(test_case.time_remaining_known)));
    EXPECT_CALL(item(), GetOpenWhenComplete())
        .WillRepeatedly(Return(test_case.open_when_complete));
    EXPECT_CALL(item(), IsPaused())
        .WillRepeatedly(Return(test_case.is_paused));
    EXPECT_CALL(item(), GetRerouteInfo())
        .WillRepeatedly(ReturnRef(test_case.reroute_info));

    EXPECT_EQ(test_case.expected_status_msg,
              base::UTF16ToUTF8(model().GetStatusText()));
  }
}

TEST_F(DownloadItemModelTest, CompletedStatus) {
  SetupDownloadItemDefaults();
  SetupCompletedDownloadItem();

  EXPECT_TRUE(model().GetStatusText().empty());
#if defined(OS_MAC)
  EXPECT_EQ("Show in Finder", base::UTF16ToUTF8(model().GetShowInFolderText()));
#else  // defined(OS_MAC)
  EXPECT_EQ("Show in folder", base::UTF16ToUTF8(model().GetShowInFolderText()));
#endif

  // Different texts for file rerouted:
  EXPECT_CALL(item(), GetRerouteInfo())
      .WillRepeatedly(ReturnRef(kTestRerouteInfo));
  // "Saved to <WEB_DRIVE>".
  std::string expected_status_msg =
      base::StringPrintf("Saved to %s", kTestProviderDisplayName);
  EXPECT_EQ(expected_status_msg, base::UTF16ToUTF8(model().GetStatusText()));
  // "Show in <WEB_DRIVE>".
  std::string expected_show_in_folder_text =
      base::StringPrintf("Show in %s", kTestProviderDisplayName);
  EXPECT_EQ(expected_show_in_folder_text,
            base::UTF16ToUTF8(model().GetShowInFolderText()));
}

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
  download::DownloadDangerType blocked_filetype =
      download::DOWNLOAD_DANGER_TYPE_BLOCKED_UNSUPPORTED_FILETYPE;

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
      {DownloadItem::COMPLETE,  blocked_filetype,  true,  false},
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
