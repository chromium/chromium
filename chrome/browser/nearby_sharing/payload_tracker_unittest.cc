// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/payload_tracker.h"

#include <memory>

#include "base/files/file_path.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/nearby_sharing/constants.h"
#include "chrome/browser/nearby_sharing/transfer_metadata_builder.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
// Total number of attachments.
constexpr int kAttachmentCount = 5;

// Size of each attachment.
constexpr int kTotalSize = 10;

constexpr int kWifiCredentialsIdOk = 111;
constexpr int kWifiCredentialsIdBad = 112;
constexpr char kWifiSsidOk[] = "test_ssid1";
constexpr char kWifiSsidBad[] = "test_ssid2";
const WifiCredentialsAttachment::SecurityType kWifiSecurityType =
    ::sharing::mojom::WifiCredentialsMetadata::SecurityType::kWpaPsk;

}  // namespace

MATCHER_P(MetadataMatcher, expected_metadata, "") {
  return expected_metadata.status() == arg.status() &&
         expected_metadata.progress() == arg.progress();
}

class PayloadTrackerTest : public testing::Test {
 public:
  using MockUpdateCallback = base::MockCallback<
      base::RepeatingCallback<void(ShareTarget, TransferMetadata)>>;

  PayloadTrackerTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  ~PayloadTrackerTest() override = default;

  void SetUp() override {
    for (int i = 0; i < kAttachmentCount / 2; i++) {
      FileAttachment file(base::FilePath(FILE_PATH_LITERAL("file.jpg")));
      file.set_size(kTotalSize);

      AttachmentInfo info;
      info.payload_id = i;
      attachment_info_map_.emplace(file.id(), std::move(info));

      share_target_.file_attachments.push_back(std::move(file));
    }

    for (int i = kAttachmentCount / 2; i < kAttachmentCount; i++) {
      TextAttachment text(TextAttachment::Type::kText, "text body.",
                          /*title=*/std::nullopt, /*mime_type=*/std::nullopt);

      AttachmentInfo info;
      info.payload_id = i;
      attachment_info_map_.emplace(text.id(), std::move(info));

      share_target_.text_attachments.push_back(std::move(text));
    }

    WifiCredentialsAttachment wifi_credentials_ok(
        kWifiCredentialsIdOk, kWifiSecurityType, kWifiSsidOk);

    AttachmentInfo info;
    info.payload_id = 4;
    attachment_info_map_.emplace(wifi_credentials_ok.id(), std::move(info));

    share_target_.wifi_credentials_attachments.push_back(
        std::move(wifi_credentials_ok));

    // This attachment is not added to |attachment_info_map_|.
    WifiCredentialsAttachment wifi_credentials_bad(
        kWifiCredentialsIdBad, kWifiSecurityType, kWifiSsidBad);
    share_target_.wifi_credentials_attachments.push_back(
        std::move(wifi_credentials_bad));

    // This attachment is not added to |attachment_info_map_|.
    TextAttachment text(TextAttachment::Type::kText, "text body.",
                        /*title=*/std::nullopt, /*mime_type=*/std::nullopt);
    share_target_.text_attachments.push_back(std::move(text));

    payload_tracker_ = std::make_unique<PayloadTracker>(
        share_target_, attachment_info_map_, callback_.Get());
  }

  MockUpdateCallback& callback() { return callback_; }

  void MarkSuccessful(int payload_id) {
    UpdatePayloadStatus(payload_id,
                        nearby::connections::mojom::PayloadStatus::kSuccess);
  }

  void MarkCancelled(int payload_id) {
    UpdatePayloadStatus(payload_id,
                        nearby::connections::mojom::PayloadStatus::kCanceled);
  }

  void MarkFailure(int payload_id) {
    UpdatePayloadStatus(payload_id,
                        nearby::connections::mojom::PayloadStatus::kFailure);
  }

  void WaitBetweenUpdates() {
    task_environment_.FastForwardBy(kMinProgressUpdateFrequency);
  }

 private:
  void UpdatePayloadStatus(int payload_id,
                           nearby::connections::mojom::PayloadStatus status) {
    nearby::connections::mojom::PayloadTransferUpdatePtr payload =
        nearby::connections::mojom::PayloadTransferUpdate::New(
            payload_id, status,
            /*total_bytes=*/kTotalSize, /*bytes_transferred=*/kTotalSize);
    payload_tracker_->OnStatusUpdate(
        std::move(payload), nearby::connections::mojom::Medium::kWebRtc);
  }

  content::BrowserTaskEnvironment task_environment_;
  ShareTarget share_target_;
  base::flat_map<int64_t, AttachmentInfo> attachment_info_map_;
  MockUpdateCallback callback_;
  std::unique_ptr<PayloadTracker> payload_tracker_;
};

// Tests update callback when all payloads are completed.
TEST_F(PayloadTrackerTest, PayloadsComplete_Successful) {
  EXPECT_CALL(
      callback(),
      Run(testing::_,
          MetadataMatcher(TransferMetadataBuilder()
                              .set_status(TransferMetadata::Status::kInProgress)
                              .set_progress(20)
                              .build())));
  EXPECT_CALL(
      callback(),
      Run(testing::_,
          MetadataMatcher(TransferMetadataBuilder()
                              .set_status(TransferMetadata::Status::kInProgress)
                              .set_progress(40)
                              .build())));
  EXPECT_CALL(
      callback(),
      Run(testing::_,
          MetadataMatcher(TransferMetadataBuilder()
                              .set_status(TransferMetadata::Status::kInProgress)
                              .set_progress(60)
                              .build())));
  EXPECT_CALL(
      callback(),
      Run(testing::_,
          MetadataMatcher(TransferMetadataBuilder()
                              .set_status(TransferMetadata::Status::kInProgress)
                              .set_progress(80)
                              .build())));
  EXPECT_CALL(
      callback(),
      Run(testing::_,
          MetadataMatcher(TransferMetadataBuilder()
                              .set_status(TransferMetadata::Status::kComplete)
                              .set_progress(100)
                              .build())));

  for (int payload_id = 0; payload_id < kAttachmentCount; payload_id++) {
    MarkSuccessful(payload_id);
    WaitBetweenUpdates();
  }
}

// Tests update callback when one of the payload fails.
TEST_F(PayloadTrackerTest, PayloadsComplete_PartialFailure) {
  EXPECT_CALL(
      callback(),
      Run(testing::_,
          MetadataMatcher(TransferMetadataBuilder()
                              .set_status(TransferMetadata::Status::kFailed)
                              .build())));

  MarkFailure(/*payload_id=*/0);
}

// Tests update callback when one of the payload gets cancelled.
TEST_F(PayloadTrackerTest, PayloadsComplete_PartialCancelled) {
  EXPECT_CALL(
      callback(),
      Run(testing::_,
          MetadataMatcher(TransferMetadataBuilder()
                              .set_status(TransferMetadata::Status::kCancelled)
                              .build())));

  MarkCancelled(/*payload_id=*/1);
}

// Tests update callback when the payloads are still being received.
TEST_F(PayloadTrackerTest, PayloadsInProgress) {
  EXPECT_CALL(
      callback(),
      Run(testing::_,
          MetadataMatcher(TransferMetadataBuilder()
                              .set_status(TransferMetadata::Status::kInProgress)
                              .set_progress(20)
                              .build())));

  MarkSuccessful(/*payload_id=*/0);
}

// Tests update callback when a status update is received, but there is no
// change in the percentage of received data.
TEST_F(PayloadTrackerTest, MultipleInProgressUpdates_SamePercentage) {
  EXPECT_CALL(
      callback(),
      Run(testing::_,
          MetadataMatcher(TransferMetadataBuilder()
                              .set_status(TransferMetadata::Status::kInProgress)
                              .set_progress(20)
                              .build())));

  MarkSuccessful(/*payload_id=*/0);
  WaitBetweenUpdates();
  MarkSuccessful(/*payload_id=*/0);
}

// Tests update callback when a status update is received in almost the same
// time as the last update.
TEST_F(PayloadTrackerTest, MultipleInProgressUpdates_HighFrequency) {
  EXPECT_CALL(
      callback(),
      Run(testing::_,
          MetadataMatcher(TransferMetadataBuilder()
                              .set_status(TransferMetadata::Status::kInProgress)
                              .set_progress(20)
                              .build())));

  MarkSuccessful(/*payload_id=*/0);
  MarkSuccessful(/*payload_id=*/1);
}

// Tests update callback when a status update for an unknown payload.
TEST_F(PayloadTrackerTest, StatusUpdateForUnknownPayload) {
  EXPECT_CALL(callback(), Run(testing::_, testing::_)).Times(0);

  MarkSuccessful(/*payload_id=*/5);
}
