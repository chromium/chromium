// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/scanner/scanner_session.h"

#include <memory>
#include <string_view>
#include <utility>
#include <vector>

#include "ash/scanner/fake_scanner_profile_scoped_delegate.h"
#include "ash/scanner/scanner_action_view_model.h"
#include "ash/scanner/scanner_metrics.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/manta/manta_status.h"
#include "components/manta/proto/scanner.pb.h"
#include "components/manta/scanner_provider.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_unittest_util.h"

namespace ash {
namespace {

using ::base::test::InvokeFuture;
using ::base::test::RunOnceCallback;
using ::base::test::ValueIs;
using ::testing::IsEmpty;
using ::testing::SizeIs;

constexpr std::string_view kScannerFeatureUserStateHistogram =
    "Ash.ScannerFeature.UserState";

using FetchActionsForImageFuture = base::test::TestFuture<
    scoped_refptr<base::RefCountedMemory>,
    manta::ScannerProvider::ScannerProtoResponseCallback>;

scoped_refptr<base::RefCountedMemory> MakeJpegBytes(int width = 100,
                                                    int height = 100) {
  gfx::ImageSkia img = gfx::test::CreateImageSkia(width, height);
  std::optional<std::vector<uint8_t>> data =
      gfx::JPEGCodec::Encode(*img.bitmap(), /*quality=*/90);
  CHECK(data.has_value());
  return base::MakeRefCounted<base::RefCountedBytes>(std::move(*data));
}

TEST(ScannerSessionTest, FetchActionsForImageReturnsErrorWhenDelegateErrors) {
  FakeScannerProfileScopedDelegate delegate;
  EXPECT_CALL(delegate, FetchActionsForImage)
      .WillOnce(RunOnceCallback<1>(
          nullptr, manta::MantaStatus{
                       .status_code = manta::MantaStatusCode::kInvalidInput}));
  ScannerSession session(&delegate);

  base::test::TestFuture<ScannerSession::FetchActionsResponse> future;
  session.FetchActionsForImage(nullptr, future.GetCallback());

  ScannerSession::FetchActionsResponse response = future.Take();
  EXPECT_EQ(response.error().error_message,
            l10n_util::GetStringUTF16(IDS_ASH_SCANNER_ERROR_GENERIC));
  EXPECT_TRUE(response.error().can_try_again);
}

TEST(ScannerSessionTest,
     FetchActionsForImageReturnsErrorForUnsupportedLanguage) {
  FakeScannerProfileScopedDelegate delegate;
  EXPECT_CALL(delegate, FetchActionsForImage)
      .WillOnce(RunOnceCallback<1>(
          nullptr,
          manta::MantaStatus{
              .status_code = manta::MantaStatusCode::kUnsupportedLanguage}));
  ScannerSession session(&delegate);

  base::test::TestFuture<ScannerSession::FetchActionsResponse> future;
  session.FetchActionsForImage(nullptr, future.GetCallback());

  ScannerSession::FetchActionsResponse response = future.Take();
  EXPECT_EQ(
      response.error().error_message,
      l10n_util::GetStringUTF16(IDS_ASH_SCANNER_ERROR_UNSUPPORTED_LANGUAGE));
  EXPECT_FALSE(response.error().can_try_again);
}

TEST(ScannerSessionTest,
     FetchActionsForImageReturnsEmptyWhenDelegateHasNoObjects) {
  FakeScannerProfileScopedDelegate delegate;
  EXPECT_CALL(delegate, FetchActionsForImage)
      .WillOnce(RunOnceCallback<1>(
          std::make_unique<manta::proto::ScannerOutput>(),
          manta::MantaStatus{.status_code = manta::MantaStatusCode::kOk}));
  ScannerSession session(&delegate);

  base::test::TestFuture<ScannerSession::FetchActionsResponse> future;
  session.FetchActionsForImage(nullptr, future.GetCallback());

  EXPECT_THAT(future.Take(), ValueIs(IsEmpty()));
}

TEST(ScannerSessionTest, FetchActionsForImageRecordsNumberOfActionsMetrics) {
  base::HistogramTester histogram_tester;
  FakeScannerProfileScopedDelegate delegate;
  auto output = std::make_unique<manta::proto::ScannerOutput>();
  manta::proto::ScannerObject& object_one = *output->add_objects();
  object_one.add_actions()->mutable_new_event();
  object_one.add_actions()->mutable_new_contact();
  manta::proto::ScannerObject& object_two = *output->add_objects();
  object_two.add_actions()->mutable_new_event();
  object_two.add_actions()->mutable_new_google_doc();

  EXPECT_CALL(delegate, FetchActionsForImage)
      .WillOnce(RunOnceCallback<1>(
          std::move(output),
          manta::MantaStatus{.status_code = manta::MantaStatusCode::kOk}));
  ScannerSession session(&delegate);
  session.FetchActionsForImage(nullptr, base::DoNothing());

  histogram_tester.ExpectBucketCount(
      kScannerFeatureUserStateHistogram,
      ScannerFeatureUserState::kNewCalendarEventActionDetected, 2);
  histogram_tester.ExpectBucketCount(
      kScannerFeatureUserStateHistogram,
      ScannerFeatureUserState::kNewContactActionDetected, 1);
  histogram_tester.ExpectBucketCount(
      kScannerFeatureUserStateHistogram,
      ScannerFeatureUserState::kNewGoogleDocActionDetected, 1);
  histogram_tester.ExpectBucketCount(
      kScannerFeatureUserStateHistogram,
      ScannerFeatureUserState::kNoActionsDetected, 0);
}

TEST(ScannerSessionTest, FetchActionsForImageNoActionRecordsMetrics) {
  base::HistogramTester histogram_tester;
  FakeScannerProfileScopedDelegate delegate;
  auto output = std::make_unique<manta::proto::ScannerOutput>();
  output->add_objects();
  output->add_objects();

  EXPECT_CALL(delegate, FetchActionsForImage)
      .WillOnce(RunOnceCallback<1>(
          std::move(output),
          manta::MantaStatus{.status_code = manta::MantaStatusCode::kOk}));
  ScannerSession session(&delegate);
  session.FetchActionsForImage(nullptr, base::DoNothing());

  histogram_tester.ExpectBucketCount(
      kScannerFeatureUserStateHistogram,
      ScannerFeatureUserState::kNoActionsDetected, 1);
}

TEST(ScannerSessionTest, FetchActionsForImageRecordsTimerMetric) {
  base::test::SingleThreadTaskEnvironment task_environment(
      base::test::SingleThreadTaskEnvironment::TimeSource::MOCK_TIME);
  base::HistogramTester histogram_tester;
  FetchActionsForImageFuture future;
  FakeScannerProfileScopedDelegate delegate;
  EXPECT_CALL(delegate, FetchActionsForImage).WillOnce(InvokeFuture(future));
  ScannerSession session(&delegate);
  session.FetchActionsForImage(nullptr, base::DoNothing());
  task_environment.FastForwardBy(base::Milliseconds(500));
  auto output = std::make_unique<manta::proto::ScannerOutput>();
  output->add_objects();
  auto [ignored, callback] = future.Take();
  std::move(callback).Run(std::move(output), manta::MantaStatus());

  histogram_tester.ExpectBucketCount(kScannerFeatureTimerFetchActionsForImage,
                                     500, 1);
}

TEST(ScannerSessionTest,
     FetchActionsForImageReturnsEqualNumberOfActionsAsProtoResponse) {
  FakeScannerProfileScopedDelegate delegate;
  manta::proto::NewEventAction event_action;
  event_action.set_title("Event");
  auto output = std::make_unique<manta::proto::ScannerOutput>();
  manta::proto::ScannerObject& object = *output->add_objects();
  *object.add_actions()->mutable_new_event() = event_action;
  *object.add_actions()->mutable_new_event() = event_action;
  *object.add_actions()->mutable_new_event() = event_action;
  EXPECT_CALL(delegate, FetchActionsForImage)
      .WillOnce(RunOnceCallback<1>(
          std::move(output),
          manta::MantaStatus{.status_code = manta::MantaStatusCode::kOk}));
  ScannerSession session(&delegate);

  base::test::TestFuture<ScannerSession::FetchActionsResponse> future;
  session.FetchActionsForImage(nullptr, future.GetCallback());

  EXPECT_THAT(future.Take(), ValueIs(SizeIs(3)));
}

TEST(ScannerSessionTest, ResizesImageHeightToMaxEdge) {
  FakeScannerProfileScopedDelegate delegate;
  FetchActionsForImageFuture future;
  EXPECT_CALL(delegate, FetchActionsForImage).WillOnce(InvokeFuture(future));
  ScannerSession session(&delegate);

  scoped_refptr<base::RefCountedMemory> bytes =
      MakeJpegBytes(/*width=*/2300, /*height=*/23000);
  session.FetchActionsForImage(bytes, base::DoNothing());

  auto processed_bytes = future.Get<scoped_refptr<base::RefCountedMemory>>();

  SkBitmap processed_bitmap = gfx::JPEGCodec::Decode(*processed_bytes);

  EXPECT_EQ(processed_bitmap.width(), 230);
  EXPECT_EQ(processed_bitmap.height(), 2300);
}

TEST(ScannerSessionTest, ResizesImageWidthToMaxEdge) {
  FakeScannerProfileScopedDelegate delegate;
  FetchActionsForImageFuture future;
  EXPECT_CALL(delegate, FetchActionsForImage).WillOnce(InvokeFuture(future));
  ScannerSession session(&delegate);

  scoped_refptr<base::RefCountedMemory> bytes =
      MakeJpegBytes(/*width=*/23000, /*height=*/2300);
  session.FetchActionsForImage(bytes, base::DoNothing());

  auto processed_bytes = future.Get<scoped_refptr<base::RefCountedMemory>>();

  SkBitmap processed_bitmap = gfx::JPEGCodec::Decode(*processed_bytes);

  EXPECT_EQ(processed_bitmap.width(), 2300);
  EXPECT_EQ(processed_bitmap.height(), 230);
}

TEST(ScannerSessionTest, NoResizeIfWithinLimit) {
  FakeScannerProfileScopedDelegate delegate;
  FetchActionsForImageFuture future;
  EXPECT_CALL(delegate, FetchActionsForImage).WillOnce(InvokeFuture(future));
  ScannerSession session(&delegate);

  scoped_refptr<base::RefCountedMemory> bytes =
      MakeJpegBytes(/*width=*/1000, /*height=*/1000);
  session.FetchActionsForImage(bytes, base::DoNothing());

  auto processed_bytes = future.Get<scoped_refptr<base::RefCountedMemory>>();

  EXPECT_EQ(bytes, processed_bytes);
}

TEST(ScannerSessionTest, DoesNotResizeIfTotalPixelSizeLowerThanMax) {
  FakeScannerProfileScopedDelegate delegate;
  FetchActionsForImageFuture future;
  EXPECT_CALL(delegate, FetchActionsForImage).WillOnce(InvokeFuture(future));
  ScannerSession session(&delegate);

  scoped_refptr<base::RefCountedMemory> bytes =
      MakeJpegBytes(/*width=*/4600, /*height=*/1100);
  session.FetchActionsForImage(bytes, base::DoNothing());

  auto processed_bytes = future.Get<scoped_refptr<base::RefCountedMemory>>();

  EXPECT_EQ(bytes, processed_bytes);
}

}  // namespace
}  // namespace ash
