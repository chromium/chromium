// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/offline_item_utils.h"

#include "components/download/public/common/download_utils.h"
#include "components/download/public/common/mock_download_item.h"
#include "testing/gtest/include/gtest/gtest.h"

using ContentId = offline_items_collection::ContentId;
using OfflineItem = offline_items_collection::OfflineItem;
using OfflineItemFilter = offline_items_collection::OfflineItemFilter;
using OfflineItemState = offline_items_collection::OfflineItemState;
using OfflineItemProgressUnit =
    offline_items_collection::OfflineItemProgressUnit;
using FailState = offline_items_collection::FailState;
using PendingState = offline_items_collection::PendingState;
using DownloadItem = download::DownloadItem;
using ::testing::_;
using ::testing::Return;
using ::testing::ReturnRefOfCopy;

namespace {
const GURL kTestUrl("http://www.example.com");
const GURL kTestOriginalUrl("http://www.exampleoriginalurl.com");
const char kNameSpace[] = "LEGACY_DOWNLOAD";

}  // namespace

class OfflineItemUtilsTest : public testing::Test {
 public:
  OfflineItemUtilsTest() = default;
  ~OfflineItemUtilsTest() override = default;

 protected:
  std::unique_ptr<download::MockDownloadItem> CreateDownloadItem(
      const std::string& guid,
      const base::FilePath& file_path,
      const base::FilePath& file_name,
      const std::string& mime_type,
      DownloadItem::DownloadState state,
      bool is_paused,
      bool is_dangerous,
      const base::Time& creation_time,
      const base::Time& last_accessed_time,
      int64_t received_bytes,
      int64_t total_bytes,
      download::DownloadInterruptReason interrupt_reason);

  std::unique_ptr<download::MockDownloadItem> CreateDownloadItem(
      DownloadItem::DownloadState state,
      bool is_paused,
      download::DownloadInterruptReason interrupt_reason);

  bool IsDownloadDone(DownloadItem* item) {
    return download::IsDownloadDone(item->GetURL(), item->GetState(),
                                    item->GetLastReason());
  }
};

std::unique_ptr<download::MockDownloadItem>
OfflineItemUtilsTest::CreateDownloadItem(
    const std::string& guid,
    const base::FilePath& file_path,
    const base::FilePath& file_name,
    const std::string& mime_type,
    DownloadItem::DownloadState state,
    bool is_paused,
    bool is_dangerous,
    const base::Time& creation_time,
    const base::Time& last_access_time,
    int64_t received_bytes,
    int64_t total_bytes,
    download::DownloadInterruptReason interrupt_reason) {
  std::unique_ptr<download::MockDownloadItem> item(
      new ::testing::NiceMock<download::MockDownloadItem>());
  ON_CALL(*item, GetURL()).WillByDefault(ReturnRefOfCopy(kTestUrl));
  ON_CALL(*item, GetTabUrl()).WillByDefault(ReturnRefOfCopy(kTestUrl));
  ON_CALL(*item, GetOriginalUrl())
      .WillByDefault(ReturnRefOfCopy(kTestOriginalUrl));
  ON_CALL(*item, GetDangerType())
      .WillByDefault(Return(download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS));
  ON_CALL(*item, GetId()).WillByDefault(Return(0));
  ON_CALL(*item, GetLastReason()).WillByDefault(Return(interrupt_reason));
  ON_CALL(*item, GetState()).WillByDefault(Return(state));
  ON_CALL(*item, GetTargetFilePath()).WillByDefault(ReturnRefOfCopy(file_path));
  ON_CALL(*item, GetFileNameToReportUser()).WillByDefault(Return(file_name));
  ON_CALL(*item, GetTransitionType())
      .WillByDefault(Return(ui::PAGE_TRANSITION_LINK));
  ON_CALL(*item, IsDangerous()).WillByDefault(Return(is_dangerous));
  ON_CALL(*item, IsPaused()).WillByDefault(Return(is_paused));
  ON_CALL(*item, GetGuid()).WillByDefault(ReturnRefOfCopy(guid));
  ON_CALL(*item, GetMimeType()).WillByDefault(Return(mime_type));
  ON_CALL(*item, GetStartTime()).WillByDefault(Return(creation_time));
  ON_CALL(*item, GetLastAccessTime()).WillByDefault(Return(last_access_time));
  ON_CALL(*item, GetReceivedBytes()).WillByDefault(Return(received_bytes));
  ON_CALL(*item, GetTotalBytes()).WillByDefault(Return(total_bytes));

  ON_CALL(*item, IsDone()).WillByDefault(Return(IsDownloadDone(item.get())));

  return item;
}

std::unique_ptr<download::MockDownloadItem>
OfflineItemUtilsTest::CreateDownloadItem(
    DownloadItem::DownloadState state,
    bool is_paused,
    download::DownloadInterruptReason interrupt_reason) {
  std::string guid = "test_guid";
  base::FilePath file_path(FILE_PATH_LITERAL("/tmp/example_file_path"));
  base::FilePath file_name(FILE_PATH_LITERAL("example_file_path"));
  std::string mime_type = "text/html";
  return CreateDownloadItem(guid, file_path, file_name, mime_type, state,
                            is_paused, false, base::Time(), base::Time(), 10,
                            100, interrupt_reason);
}

TEST_F(OfflineItemUtilsTest, BasicConversions) {
  std::string guid = "test_guid";
  base::FilePath file_path(FILE_PATH_LITERAL("/tmp/example_file_path"));
  base::FilePath file_name(FILE_PATH_LITERAL("image.png"));
  std::string mime_type = "image/png";
  base::Time creation_time = base::Time::Now();
  base::Time completion_time = base::Time::Now();
  base::Time last_access_time = base::Time::Now();
  download::DownloadInterruptReason interrupt_reason =
      download::DOWNLOAD_INTERRUPT_REASON_NONE;
  bool is_transient = true;
  bool is_accelerated = true;
  bool externally_removed = true;
  bool is_openable = true;
  bool is_resumable = true;
  bool allow_metered = true;
  int64_t time_remaining_ms = 10000;
  bool is_dangerous = true;
  int64_t total_bytes = 1000;
  int64_t received_bytes = 10;
  std::unique_ptr<download::MockDownloadItem> download = CreateDownloadItem(
      guid, file_path, file_name, mime_type, DownloadItem::COMPLETE, false,
      is_dangerous, creation_time, last_access_time, 0, 0, interrupt_reason);

  ON_CALL(*download, IsTransient()).WillByDefault(Return(is_transient));
  ON_CALL(*download, IsParallelDownload())
      .WillByDefault(Return(is_accelerated));
  ON_CALL(*download, GetFileExternallyRemoved())
      .WillByDefault(Return(externally_removed));
  ON_CALL(*download, CanOpenDownload()).WillByDefault(Return(is_openable));
  ON_CALL(*download, CanResume()).WillByDefault(Return(is_resumable));
  ON_CALL(*download, AllowMetered()).WillByDefault(Return(allow_metered));
  ON_CALL(*download, GetReceivedBytes()).WillByDefault(Return(received_bytes));
  ON_CALL(*download, GetTotalBytes()).WillByDefault(Return(total_bytes));
  ON_CALL(*download, GetEndTime()).WillByDefault(Return(completion_time));

  ON_CALL(*download, TimeRemaining(_))
      .WillByDefault(testing::DoAll(
          testing::SetArgPointee<0>(
              base::TimeDelta::FromMilliseconds(time_remaining_ms)),
          Return(true)));
  ON_CALL(*download, IsDangerous()).WillByDefault(Return(is_dangerous));

  OfflineItem offline_item =
      OfflineItemUtils::CreateOfflineItem(kNameSpace, download.get());

  EXPECT_EQ(ContentId(kNameSpace, guid), offline_item.id);
  EXPECT_EQ(file_name.AsUTF8Unsafe(), offline_item.title);
  EXPECT_EQ(file_name.AsUTF8Unsafe(), offline_item.description);
  EXPECT_EQ(OfflineItemFilter::FILTER_IMAGE, offline_item.filter);
  EXPECT_EQ(is_transient, offline_item.is_transient);
  EXPECT_FALSE(offline_item.is_suggested);
  EXPECT_EQ(is_accelerated, offline_item.is_accelerated);
  EXPECT_FALSE(offline_item.promote_origin);
  EXPECT_TRUE(offline_item.can_rename);

  EXPECT_EQ(total_bytes, offline_item.total_size_bytes);
  EXPECT_EQ(externally_removed, offline_item.externally_removed);
  EXPECT_EQ(creation_time, offline_item.creation_time);
  EXPECT_EQ(completion_time, offline_item.completion_time);
  EXPECT_EQ(last_access_time, offline_item.last_accessed_time);
  EXPECT_EQ(is_openable, offline_item.is_openable);
  EXPECT_EQ(file_path, offline_item.file_path);
  EXPECT_EQ(mime_type, offline_item.mime_type);

  EXPECT_EQ(kTestUrl, offline_item.page_url);
  EXPECT_EQ(kTestOriginalUrl, offline_item.original_url);
  EXPECT_FALSE(offline_item.is_off_the_record);
  EXPECT_EQ("", offline_item.attribution);

  EXPECT_EQ(OfflineItemState::COMPLETE, offline_item.state);
  EXPECT_EQ(FailState::NO_FAILURE, offline_item.fail_state);
  EXPECT_EQ(PendingState::NOT_PENDING, offline_item.pending_state);
  EXPECT_EQ(is_resumable, offline_item.is_resumable);
  EXPECT_EQ(allow_metered, offline_item.allow_metered);
  EXPECT_EQ(received_bytes, offline_item.received_bytes);
  EXPECT_EQ(received_bytes, offline_item.progress.value);
  EXPECT_TRUE(offline_item.progress.max.has_value());
  EXPECT_EQ(total_bytes, offline_item.progress.max.value());
  EXPECT_EQ(OfflineItemProgressUnit::BYTES, offline_item.progress.unit);
  EXPECT_EQ(time_remaining_ms, offline_item.time_remaining_ms);
  EXPECT_EQ(is_dangerous, offline_item.is_dangerous);
}

TEST_F(OfflineItemUtilsTest, StateConversions) {
  // in-progress
  std::unique_ptr<download::MockDownloadItem> download1 =
      CreateDownloadItem(DownloadItem::IN_PROGRESS, false,
                         download::DOWNLOAD_INTERRUPT_REASON_NONE);

  // cancelled
  std::unique_ptr<download::MockDownloadItem> download2 = CreateDownloadItem(
      DownloadItem::CANCELLED, false, download::DOWNLOAD_INTERRUPT_REASON_NONE);

  // complete
  std::unique_ptr<download::MockDownloadItem> download3 = CreateDownloadItem(
      DownloadItem::COMPLETE, false, download::DOWNLOAD_INTERRUPT_REASON_NONE);

  // interrupted, but auto-resumable
  std::unique_ptr<download::MockDownloadItem> download4 =
      CreateDownloadItem(DownloadItem::INTERRUPTED, false,
                         download::DOWNLOAD_INTERRUPT_REASON_NETWORK_TIMEOUT);

  // paused
  std::unique_ptr<download::MockDownloadItem> download5 =
      CreateDownloadItem(DownloadItem::IN_PROGRESS, true,
                         download::DOWNLOAD_INTERRUPT_REASON_NONE);

  // paused, but interrupted
  std::unique_ptr<download::MockDownloadItem> download6 =
      CreateDownloadItem(DownloadItem::INTERRUPTED, true,
                         download::DOWNLOAD_INTERRUPT_REASON_NETWORK_TIMEOUT);

  // interrupted, but invalid resumption mode
  std::unique_ptr<download::MockDownloadItem> download7 = CreateDownloadItem(
      DownloadItem::INTERRUPTED, false,
      download::DOWNLOAD_INTERRUPT_REASON_FILE_SAME_AS_SOURCE);

  // interrupted, not auto-resumable
  std::unique_ptr<download::MockDownloadItem> download8 =
      CreateDownloadItem(DownloadItem::INTERRUPTED, false,
                         download::DOWNLOAD_INTERRUPT_REASON_SERVER_NO_RANGE);

  // interrupted, should be auto-resumable, but max retry count reached
  std::unique_ptr<download::MockDownloadItem> download9 =
      CreateDownloadItem(DownloadItem::INTERRUPTED, false,
                         download::DOWNLOAD_INTERRUPT_REASON_SERVER_NO_RANGE);
  ON_CALL(*download9, GetAutoResumeCount()).WillByDefault(Return(10));

  // interrupted, should be auto-resumable, but dangerous
  std::unique_ptr<download::MockDownloadItem> download10 =
      CreateDownloadItem(DownloadItem::INTERRUPTED, false,
                         download::DOWNLOAD_INTERRUPT_REASON_NETWORK_TIMEOUT);
  ON_CALL(*download10, IsDangerous()).WillByDefault(Return(true));

  OfflineItem offline_item1 =
      OfflineItemUtils::CreateOfflineItem(kNameSpace, download1.get());
  EXPECT_EQ(OfflineItemState::IN_PROGRESS, offline_item1.state);

  OfflineItem offline_item2 =
      OfflineItemUtils::CreateOfflineItem(kNameSpace, download2.get());
  EXPECT_EQ(OfflineItemState::CANCELLED, offline_item2.state);

  OfflineItem offline_item3 =
      OfflineItemUtils::CreateOfflineItem(kNameSpace, download3.get());
  EXPECT_EQ(OfflineItemState::COMPLETE, offline_item3.state);

  OfflineItem offline_item4 =
      OfflineItemUtils::CreateOfflineItem(kNameSpace, download4.get());
  EXPECT_EQ(OfflineItemState::PENDING, offline_item4.state);

  OfflineItem offline_item5 =
      OfflineItemUtils::CreateOfflineItem(kNameSpace, download5.get());
  EXPECT_EQ(OfflineItemState::PAUSED, offline_item5.state);

  OfflineItem offline_item6 =
      OfflineItemUtils::CreateOfflineItem(kNameSpace, download6.get());
  EXPECT_EQ(OfflineItemState::PAUSED, offline_item6.state);

  OfflineItem offline_item7 =
      OfflineItemUtils::CreateOfflineItem(kNameSpace, download7.get());
  EXPECT_EQ(OfflineItemState::FAILED, offline_item7.state);

  OfflineItem offline_item8 =
      OfflineItemUtils::CreateOfflineItem(kNameSpace, download8.get());
  EXPECT_EQ(OfflineItemState::INTERRUPTED, offline_item8.state);

  OfflineItem offline_item9 =
      OfflineItemUtils::CreateOfflineItem(kNameSpace, download9.get());
  EXPECT_EQ(OfflineItemState::PAUSED, offline_item9.state);

  OfflineItem offline_item10 =
      OfflineItemUtils::CreateOfflineItem(kNameSpace, download10.get());
  EXPECT_EQ(OfflineItemState::INTERRUPTED, offline_item10.state);
}

TEST_F(OfflineItemUtilsTest, MimeTypeToFilterConversion) {
  std::string mime_type[5] = {"text/html", "image/png", "video/webm",
                              "audio/aac", "application/octet-stream"};
  OfflineItemFilter filter[5] = {
      OfflineItemFilter::FILTER_DOCUMENT, OfflineItemFilter::FILTER_IMAGE,
      OfflineItemFilter::FILTER_VIDEO, OfflineItemFilter::FILTER_AUDIO,
      OfflineItemFilter::FILTER_OTHER};

  for (int i = 0; i < 5; i++) {
    std::unique_ptr<download::MockDownloadItem> download =
        CreateDownloadItem(DownloadItem::COMPLETE, false,
                           download::DOWNLOAD_INTERRUPT_REASON_NONE);
    ON_CALL(*download, GetMimeType()).WillByDefault(Return(mime_type[i]));

    OfflineItem offline_item =
        OfflineItemUtils::CreateOfflineItem(kNameSpace, download.get());

    EXPECT_EQ(mime_type[i], offline_item.mime_type);
    EXPECT_EQ(filter[i], offline_item.filter);
  }
}

TEST_F(OfflineItemUtilsTest, PendingAndFailedStates) {
  // interrupted, but auto-resumable
  std::unique_ptr<download::MockDownloadItem> download1 =
      CreateDownloadItem(DownloadItem::INTERRUPTED, false,
                         download::DOWNLOAD_INTERRUPT_REASON_NETWORK_TIMEOUT);
  OfflineItem offline_item1 =
      OfflineItemUtils::CreateOfflineItem(kNameSpace, download1.get());
  EXPECT_EQ(OfflineItemState::PENDING, offline_item1.state);
  EXPECT_EQ(FailState::NETWORK_TIMEOUT, offline_item1.fail_state);
  EXPECT_EQ(PendingState::PENDING_NETWORK, offline_item1.pending_state);

  // failed download: interrupted, but invalid resumption mode
  std::unique_ptr<download::MockDownloadItem> download2 = CreateDownloadItem(
      DownloadItem::INTERRUPTED, false,
      download::DOWNLOAD_INTERRUPT_REASON_FILE_SAME_AS_SOURCE);
  OfflineItem offline_item2 =
      OfflineItemUtils::CreateOfflineItem(kNameSpace, download2.get());
  EXPECT_EQ(OfflineItemState::FAILED, offline_item2.state);
  EXPECT_EQ(FailState::FILE_SAME_AS_SOURCE, offline_item2.fail_state);
  EXPECT_EQ(PendingState::NOT_PENDING, offline_item2.pending_state);

  // interrupted, not auto-resumable
  std::unique_ptr<download::MockDownloadItem> download3 =
      CreateDownloadItem(DownloadItem::INTERRUPTED, false,
                         download::DOWNLOAD_INTERRUPT_REASON_SERVER_NO_RANGE);
  OfflineItem offline_item3 =
      OfflineItemUtils::CreateOfflineItem(kNameSpace, download3.get());
  EXPECT_EQ(OfflineItemState::INTERRUPTED, offline_item3.state);
  EXPECT_EQ(FailState::SERVER_NO_RANGE, offline_item3.fail_state);
  EXPECT_EQ(PendingState::NOT_PENDING, offline_item3.pending_state);
}
