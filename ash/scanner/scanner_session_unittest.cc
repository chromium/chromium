// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/scanner/scanner_session.h"

#include <memory>
#include <string_view>
#include <utility>
#include <vector>

#include "ash/public/cpp/test/test_new_window_delegate.h"
#include "ash/scanner/fake_scanner_profile_scoped_delegate.h"
#include "ash/scanner/scanner_action_view_model.h"
#include "ash/scanner/scanner_metrics.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/protobuf_matchers.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/manta/manta_status.h"
#include "components/manta/proto/scanner.pb.h"
#include "components/manta/scanner_provider.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "url/gurl.h"

namespace ash {
namespace {

using ::base::test::EqualsProto;
using ::base::test::InvokeFuture;
using ::base::test::RunOnceCallback;
using ::testing::_;
using ::testing::AllOf;
using ::testing::DoAll;
using ::testing::IsEmpty;
using ::testing::Pointee;
using ::testing::Property;
using ::testing::ResultOf;
using ::testing::Return;
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

class MockNewWindowDelegate : public TestNewWindowDelegate {
 public:
  MOCK_METHOD(void,
              OpenUrl,
              (const GURL& url, OpenUrlFrom from, Disposition disposition),
              (override));
};

TEST(ScannerSessionTest, FetchActionsForImageReturnsEmptyWhenDelegateErrors) {
  FakeScannerProfileScopedDelegate delegate;
  EXPECT_CALL(delegate, FetchActionsForImage)
      .WillOnce(RunOnceCallback<1>(
          nullptr, manta::MantaStatus{
                       .status_code = manta::MantaStatusCode::kInvalidInput}));
  ScannerSession session(&delegate);

  base::test::TestFuture<std::vector<ScannerActionViewModel>> future;
  session.FetchActionsForImage(nullptr, future.GetCallback());

  EXPECT_THAT(future.Take(), IsEmpty());
}

TEST(ScannerSessionTest,
     FetchActionsForImageReturnsEmptyWhenDelegateHasNoObjects) {
  FakeScannerProfileScopedDelegate delegate;
  EXPECT_CALL(delegate, FetchActionsForImage)
      .WillOnce(RunOnceCallback<1>(
          std::make_unique<manta::proto::ScannerOutput>(),
          manta::MantaStatus{.status_code = manta::MantaStatusCode::kOk}));
  ScannerSession session(&delegate);

  base::test::TestFuture<std::vector<ScannerActionViewModel>> future;
  session.FetchActionsForImage(nullptr, future.GetCallback());

  EXPECT_THAT(future.Take(), IsEmpty());
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

  base::test::TestFuture<std::vector<ScannerActionViewModel>> future;
  session.FetchActionsForImage(nullptr, future.GetCallback());

  EXPECT_THAT(future.Take(), SizeIs(3));
}

TEST(ScannerSessionTest, RunningActionFailsIfActionDetailsFails) {
  FakeScannerProfileScopedDelegate delegate;
  auto unpopulated_output = std::make_unique<manta::proto::ScannerOutput>();
  unpopulated_output->add_objects()->add_actions()->mutable_new_event();
  EXPECT_CALL(delegate, FetchActionsForImage)
      .WillOnce(RunOnceCallback<1>(
          std::move(unpopulated_output),
          manta::MantaStatus{.status_code = manta::MantaStatusCode::kOk}));
  EXPECT_CALL(delegate, FetchActionDetailsForImage)
      .WillOnce(RunOnceCallback<2>(
          nullptr, manta::MantaStatus{
                       .status_code = manta::MantaStatusCode::kInvalidInput}));
  ScannerSession session(&delegate);

  base::test::TestFuture<std::vector<ScannerActionViewModel>> future;
  session.FetchActionsForImage(nullptr, future.GetCallback());
  std::vector<ScannerActionViewModel> actions = future.Take();
  ASSERT_THAT(actions, SizeIs(1));
  base::test::TestFuture<bool> action_finished_future;
  actions.front().ExecuteAction(action_finished_future.GetCallback());

  EXPECT_FALSE(action_finished_future.Get());
}

TEST(ScannerSessionTest, RunningActionFailsIfActionDetailsHaveMultipleObjects) {
  FakeScannerProfileScopedDelegate delegate;
  auto unpopulated_output = std::make_unique<manta::proto::ScannerOutput>();
  unpopulated_output->add_objects()->add_actions()->mutable_new_event();
  auto output_with_multiple_objects =
      std::make_unique<manta::proto::ScannerOutput>();
  unpopulated_output->add_objects()->add_actions()->mutable_new_event();
  unpopulated_output->add_objects()->add_actions()->mutable_new_event();
  EXPECT_CALL(delegate, FetchActionsForImage)
      .WillOnce(RunOnceCallback<1>(
          std::move(unpopulated_output),
          manta::MantaStatus{.status_code = manta::MantaStatusCode::kOk}));
  EXPECT_CALL(delegate, FetchActionDetailsForImage)
      .WillOnce(RunOnceCallback<2>(
          std::move(output_with_multiple_objects),
          manta::MantaStatus{.status_code = manta::MantaStatusCode::kOk}));
  ScannerSession session(&delegate);

  base::test::TestFuture<std::vector<ScannerActionViewModel>> future;
  session.FetchActionsForImage(nullptr, future.GetCallback());
  std::vector<ScannerActionViewModel> actions = future.Take();
  ASSERT_THAT(actions, SizeIs(1));
  base::test::TestFuture<bool> action_finished_future;
  actions.front().ExecuteAction(action_finished_future.GetCallback());

  EXPECT_FALSE(action_finished_future.Get());
}

TEST(ScannerSessionTest, RunningActionFailsIfActionDetailsHaveMultipleActions) {
  FakeScannerProfileScopedDelegate delegate;
  auto unpopulated_output = std::make_unique<manta::proto::ScannerOutput>();
  unpopulated_output->add_objects()->add_actions()->mutable_new_event();
  auto output_with_multiple_actions =
      std::make_unique<manta::proto::ScannerOutput>();
  manta::proto::ScannerObject& object = *unpopulated_output->add_objects();
  object.add_actions()->mutable_new_event();
  object.add_actions()->mutable_new_event();
  EXPECT_CALL(delegate, FetchActionsForImage)
      .WillOnce(RunOnceCallback<1>(
          std::move(unpopulated_output),
          manta::MantaStatus{.status_code = manta::MantaStatusCode::kOk}));
  EXPECT_CALL(delegate, FetchActionDetailsForImage)
      .WillOnce(RunOnceCallback<2>(
          std::move(output_with_multiple_actions),
          manta::MantaStatus{.status_code = manta::MantaStatusCode::kOk}));
  ScannerSession session(&delegate);

  base::test::TestFuture<std::vector<ScannerActionViewModel>> future;
  session.FetchActionsForImage(nullptr, future.GetCallback());
  std::vector<ScannerActionViewModel> actions = future.Take();
  ASSERT_THAT(actions, SizeIs(1));
  base::test::TestFuture<bool> action_finished_future;
  actions.front().ExecuteAction(action_finished_future.GetCallback());

  EXPECT_FALSE(action_finished_future.Get());
}

TEST(ScannerSessionTest,
     RunningActionFailsIfActionDetailsHaveDifferentActionCase) {
  FakeScannerProfileScopedDelegate delegate;
  auto unpopulated_output = std::make_unique<manta::proto::ScannerOutput>();
  unpopulated_output->add_objects()->add_actions()->mutable_new_event();
  auto output_with_different_action =
      std::make_unique<manta::proto::ScannerOutput>();
  manta::proto::ScannerObject& object = *unpopulated_output->add_objects();
  object.add_actions()->mutable_new_contact();
  EXPECT_CALL(delegate, FetchActionsForImage)
      .WillOnce(RunOnceCallback<1>(
          std::move(unpopulated_output),
          manta::MantaStatus{.status_code = manta::MantaStatusCode::kOk}));
  EXPECT_CALL(delegate, FetchActionDetailsForImage)
      .WillOnce(RunOnceCallback<2>(
          std::move(output_with_different_action),
          manta::MantaStatus{.status_code = manta::MantaStatusCode::kOk}));
  ScannerSession session(&delegate);

  base::test::TestFuture<std::vector<ScannerActionViewModel>> future;
  session.FetchActionsForImage(nullptr, future.GetCallback());
  std::vector<ScannerActionViewModel> actions = future.Take();
  ASSERT_THAT(actions, SizeIs(1));
  base::test::TestFuture<bool> action_finished_future;
  actions.front().ExecuteAction(action_finished_future.GetCallback());

  EXPECT_FALSE(action_finished_future.Get());
}

TEST(ScannerSessionTest,
     RunningActionsCallsFetchActionDetailsForImageWithResizedImage) {
  scoped_refptr<base::RefCountedMemory> jpeg_bytes =
      MakeJpegBytes(/*width=*/2300, /*height=*/23000);
  FakeScannerProfileScopedDelegate delegate;
  auto unpopulated_output = std::make_unique<manta::proto::ScannerOutput>();
  unpopulated_output->add_objects()->add_actions()->mutable_new_event();
  EXPECT_CALL(delegate, FetchActionsForImage)
      .WillOnce(RunOnceCallback<1>(
          std::move(unpopulated_output),
          manta::MantaStatus{.status_code = manta::MantaStatusCode::kOk}));
  EXPECT_CALL(delegate, FetchActionDetailsForImage(
                            Pointee(ResultOf(
                                "decoding JPEG",
                                [](const base::RefCountedMemory& bytes) {
                                  return gfx::JPEGCodec::Decode(bytes);
                                },
                                AllOf(Property(&SkBitmap::width, 230),
                                      Property(&SkBitmap::height, 2300)))),
                            _, _))
      .WillOnce(RunOnceCallback<2>(
          nullptr, manta::MantaStatus{
                       .status_code = manta::MantaStatusCode::kInvalidInput}));
  ScannerSession session(&delegate);

  base::test::TestFuture<std::vector<ScannerActionViewModel>> future;
  session.FetchActionsForImage(jpeg_bytes, future.GetCallback());
  std::vector<ScannerActionViewModel> actions = future.Take();
  ASSERT_THAT(actions, SizeIs(1));
  base::test::TestFuture<bool> action_finished_future;
  actions.front().ExecuteAction(action_finished_future.GetCallback());
  ASSERT_TRUE(action_finished_future.IsReady());
}

TEST(ScannerSessionTest,
     RunningActionsCallsFetchActionDetailsForImageWithUnpopulatedAction) {
  FakeScannerProfileScopedDelegate delegate;
  manta::proto::ScannerAction unpopulated_action;
  unpopulated_action.mutable_new_event()->set_title("Unpopulated event");
  auto unpopulated_output = std::make_unique<manta::proto::ScannerOutput>();
  *unpopulated_output->add_objects()->add_actions() = unpopulated_action;
  EXPECT_CALL(delegate, FetchActionsForImage)
      .WillOnce(RunOnceCallback<1>(
          std::move(unpopulated_output),
          manta::MantaStatus{.status_code = manta::MantaStatusCode::kOk}));
  EXPECT_CALL(delegate,
              FetchActionDetailsForImage(_, EqualsProto(unpopulated_action), _))
      .WillOnce(RunOnceCallback<2>(
          nullptr, manta::MantaStatus{
                       .status_code = manta::MantaStatusCode::kInvalidInput}));
  ScannerSession session(&delegate);

  base::test::TestFuture<std::vector<ScannerActionViewModel>> future;
  session.FetchActionsForImage(nullptr, future.GetCallback());
  std::vector<ScannerActionViewModel> actions = future.Take();
  ASSERT_THAT(actions, SizeIs(1));
  base::test::TestFuture<bool> action_finished_future;
  actions.front().ExecuteAction(action_finished_future.GetCallback());
  ASSERT_TRUE(action_finished_future.IsReady());
}

TEST(ScannerSessionTest, RunningNewEventActionOpensUrl) {
  MockNewWindowDelegate new_window_delegate;
  EXPECT_CALL(new_window_delegate,
              OpenUrl(Property("spec", &GURL::spec,
                               "https://calendar.google.com/calendar/render"
                               "?action=TEMPLATE"
                               "&text=%F0%9F%8C%8F"
                               "&details=formerly+%22Geo+Sync%22"
                               "&dates=20241014T160000%2F20241014T161500"
                               "&location=Wonderland"),
                      _, _))
      .Times(1);
  FakeScannerProfileScopedDelegate delegate;
  auto unpopulated_output = std::make_unique<manta::proto::ScannerOutput>();
  unpopulated_output->add_objects()->add_actions()->mutable_new_event();
  manta::proto::NewEventAction event_action;
  event_action.set_title("🌏");
  event_action.set_description("formerly \"Geo Sync\"");
  event_action.set_dates("20241014T160000/20241014T161500");
  event_action.set_location("Wonderland");
  auto populated_output = std::make_unique<manta::proto::ScannerOutput>();
  *populated_output->add_objects()->add_actions()->mutable_new_event() =
      std::move(event_action);
  EXPECT_CALL(delegate, FetchActionsForImage)
      .WillOnce(RunOnceCallback<1>(
          std::move(unpopulated_output),
          manta::MantaStatus{.status_code = manta::MantaStatusCode::kOk}));
  EXPECT_CALL(delegate, FetchActionDetailsForImage)
      .WillOnce(RunOnceCallback<2>(
          std::move(populated_output),
          manta::MantaStatus{.status_code = manta::MantaStatusCode::kOk}));
  ScannerSession session(&delegate);

  base::test::TestFuture<std::vector<ScannerActionViewModel>> future;
  session.FetchActionsForImage(nullptr, future.GetCallback());
  std::vector<ScannerActionViewModel> actions = future.Take();
  ASSERT_THAT(actions, SizeIs(1));
  base::test::TestFuture<bool> action_finished_future;
  actions.front().ExecuteAction(action_finished_future.GetCallback());

  EXPECT_TRUE(action_finished_future.Get());
}

TEST(ScannerSessionTest, RunningNewContactActionOpensUrl) {
  base::test::TaskEnvironment task_environment(
      base::test::TaskEnvironment::MainThreadType::IO);
  MockNewWindowDelegate new_window_delegate;
  EXPECT_CALL(new_window_delegate,
              OpenUrl(Property("spec", &GURL::spec,
                               "https://contacts.google.com/person/c1?edit=1"),
                      _, _))
      .Times(1);
  FakeScannerProfileScopedDelegate delegate;
  auto unpopulated_output = std::make_unique<manta::proto::ScannerOutput>();
  unpopulated_output->add_objects()->add_actions()->mutable_new_contact();
  manta::proto::NewContactAction contact_action;
  contact_action.set_given_name("André");
  contact_action.set_family_name("François");
  contact_action.set_email("afrancois@example.com");
  contact_action.set_phone("+61400000000");
  auto populated_output = std::make_unique<manta::proto::ScannerOutput>();
  *populated_output->add_objects()->add_actions()->mutable_new_contact() =
      std::move(contact_action);
  EXPECT_CALL(delegate, FetchActionsForImage)
      .WillOnce(RunOnceCallback<1>(
          std::move(unpopulated_output),
          manta::MantaStatus{.status_code = manta::MantaStatusCode::kOk}));
  EXPECT_CALL(delegate, FetchActionDetailsForImage)
      .WillOnce(RunOnceCallback<2>(
          std::move(populated_output),
          manta::MantaStatus{.status_code = manta::MantaStatusCode::kOk}));
  base::MockCallback<
      net::test_server::EmbeddedTestServer::HandleRequestCallback>
      request_callback;
  auto response = std::make_unique<net::test_server::BasicHttpResponse>();
  response->set_code(net::HttpStatusCode::HTTP_OK);
  response->set_content(R"json({"resourceName": "people/c1"})json");
  response->set_content_type("application/json");
  EXPECT_CALL(request_callback, Run).WillOnce(Return(std::move(response)));
  delegate.SetRequestCallback(request_callback.Get());
  ScannerSession session(&delegate);

  base::test::TestFuture<std::vector<ScannerActionViewModel>> future;
  session.FetchActionsForImage(nullptr, future.GetCallback());
  std::vector<ScannerActionViewModel> actions = future.Take();
  ASSERT_THAT(actions, SizeIs(1));
  base::test::TestFuture<bool> action_finished_future;
  actions.front().ExecuteAction(action_finished_future.GetCallback());

  EXPECT_TRUE(action_finished_future.Get());
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
