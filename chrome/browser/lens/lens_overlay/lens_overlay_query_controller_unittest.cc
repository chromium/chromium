// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "lens_overlay_query_controller.h"

#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chrome/browser/lens/core/mojom/overlay_object.mojom.h"
#include "chrome/browser/lens/core/mojom/text.mojom.h"
#include "chrome/test/base/testing_profile.h"
#include "components/endpoint_fetcher/endpoint_fetcher.h"
#include "content/public/test/browser_task_environment.h"
#include "google_apis/common/api_error_codes.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/lens_server_proto/lens_overlay_server.pb.h"
#include "url/gurl.h"

namespace lens {

class FakeEndpointFetcher : public EndpointFetcher {
 public:
  explicit FakeEndpointFetcher(EndpointResponse response)
      : EndpointFetcher(
            net::DefineNetworkTrafficAnnotation("lens_overlay_mock_fetcher",
                                                R"()")),
        response_(response) {}

  void PerformRequest(EndpointFetcherCallback endpoint_fetcher_callback,
                      const char* key) override {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(endpoint_fetcher_callback),
                       std::make_unique<EndpointResponse>(response_)));
  }

 private:
  EndpointResponse response_;
};

class LensOverlayQueryControllerMock : public LensOverlayQueryController {
 public:
  explicit LensOverlayQueryControllerMock(
      LensOverlayFullImageResponseCallback full_image_callback,
      base::RepeatingCallback<void(lens::proto::LensOverlayUrlResponse)>
          url_callback,
      base::RepeatingCallback<void(lens::proto::LensOverlayInteractionResponse)>
          interaction_data_callback,
      Profile* profile)
      : LensOverlayQueryController(full_image_callback,
                                   url_callback,
                                   interaction_data_callback,
                                   profile) {}
  ~LensOverlayQueryControllerMock() override = default;

  lens::LensOverlayObjectsRequest sent_objects_request_;
  lens::LensOverlayInteractionRequest sent_interaction_request_;

 protected:
  std::unique_ptr<EndpointFetcher> CreateEndpointFetcher(
      lens::LensOverlayServerRequest request_data) override {
    EndpointResponse fake_endpoint_response;
    lens::LensOverlayServerResponse fake_server_response;
    fake_endpoint_response.response = fake_server_response.SerializeAsString();
    fake_endpoint_response.http_status_code =
        google_apis::ApiErrorCode::HTTP_SUCCESS;

    if (request_data.has_objects_request()) {
      sent_objects_request_.CopyFrom(request_data.objects_request());
      return std::make_unique<FakeEndpointFetcher>(fake_endpoint_response);
    }
    CHECK(request_data.has_interaction_request());
    sent_interaction_request_.CopyFrom(request_data.interaction_request());
    return std::make_unique<FakeEndpointFetcher>(fake_endpoint_response);
  }
};

class LensOverlayQueryControllerTest : public testing::Test {
 public:
  LensOverlayQueryControllerTest()
      : task_environment_(content::BrowserTaskEnvironment::IO_MAINLOOP,
                          base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  const SkBitmap CreateNonEmptyBitmap(int width, int height) {
    SkBitmap bitmap;
    bitmap.allocN32Pixels(width, height);
    bitmap.eraseColor(SK_ColorGREEN);
    return bitmap;
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;

  TestingProfile* profile() { return profile_.get(); }

 private:
  void SetUp() override {
    TestingProfile::Builder profile_builder;
    profile_ = profile_builder.Build();
  }
};

TEST_F(LensOverlayQueryControllerTest, FetchInitialQuery_ReturnsResponse) {
  task_environment_.RunUntilIdle();
  base::test::TestFuture<std::vector<lens::mojom::OverlayObjectPtr>,
                         lens::mojom::TextPtr>
      full_image_response_future;
  LensOverlayQueryControllerMock query_controller(
      full_image_response_future.GetRepeatingCallback(), base::NullCallback(),
      base::NullCallback(), profile());
  SkBitmap bitmap = CreateNonEmptyBitmap(100, 100);
  query_controller.StartQueryFlow(bitmap);

  task_environment_.RunUntilIdle();
  ASSERT_TRUE(full_image_response_future.IsReady());
  ASSERT_EQ(query_controller.sent_objects_request_.request_context()
                .request_id()
                .sequence_id(),
            1);
  ASSERT_EQ(query_controller.sent_objects_request_.image_data()
                .image_metadata()
                .width(),
            100);
  ASSERT_EQ(query_controller.sent_objects_request_.image_data()
                .image_metadata()
                .height(),
            100);
}

TEST_F(LensOverlayQueryControllerTest,
       FetchRegionSearchInteraction_ReturnsResponses) {
  task_environment_.RunUntilIdle();
  base::test::TestFuture<std::vector<lens::mojom::OverlayObjectPtr>,
                         lens::mojom::TextPtr>
      full_image_response_future;
  base::test::TestFuture<lens::proto::LensOverlayUrlResponse>
      url_response_future;
  base::test::TestFuture<lens::proto::LensOverlayInteractionResponse>
      interaction_data_response_future;
  LensOverlayQueryControllerMock query_controller(
      full_image_response_future.GetRepeatingCallback(),
      url_response_future.GetRepeatingCallback(),
      interaction_data_response_future.GetRepeatingCallback(), profile());
  SkBitmap bitmap = CreateNonEmptyBitmap(100, 100);
  query_controller.StartQueryFlow(bitmap);
  task_environment_.RunUntilIdle();

  auto region = lens::mojom::CenterRotatedBox::New();
  region->box = gfx::RectF(30, 40, 50, 60);
  region->coordinate_type =
      lens::mojom::CenterRotatedBox_CoordinateType::kImage;
  query_controller.SendRegionSearch(std::move(region));
  task_environment_.RunUntilIdle();

  ASSERT_TRUE(full_image_response_future.IsReady());
  ASSERT_EQ(query_controller.sent_objects_request_.image_data()
                .image_metadata()
                .width(),
            100);
  ASSERT_EQ(query_controller.sent_objects_request_.image_data()
                .image_metadata()
                .height(),
            100);
  ASSERT_EQ(url_response_future.Get().url(), "");
  ASSERT_EQ(interaction_data_response_future.Get().suggest_signals(), "");
  ASSERT_EQ(query_controller.sent_objects_request_.request_context()
                .request_id()
                .sequence_id(),
            1);
  ASSERT_EQ(query_controller.sent_interaction_request_.request_context()
                .request_id()
                .sequence_id(),
            2);
  ASSERT_EQ(
      query_controller.sent_interaction_request_.interaction_request_metadata()
          .type(),
      lens::LensOverlayInteractionRequestMetadata::REGION);
  ASSERT_EQ(
      query_controller.sent_interaction_request_.interaction_request_metadata()
          .selection_metadata()
          .region()
          .region()
          .center_x(),
      30);
  ASSERT_EQ(
      query_controller.sent_interaction_request_.interaction_request_metadata()
          .selection_metadata()
          .region()
          .region()
          .center_y(),
      40);
  ASSERT_EQ(query_controller.sent_interaction_request_.image_crop()
                .zoomed_crop()
                .crop()
                .center_x(),
            30);
  ASSERT_EQ(query_controller.sent_interaction_request_.image_crop()
                .zoomed_crop()
                .crop()
                .center_y(),
            40);
}

TEST_F(LensOverlayQueryControllerTest,
       FetchTextOnlyInteraction_ReturnsResponse) {
  task_environment_.RunUntilIdle();
  base::test::TestFuture<std::vector<lens::mojom::OverlayObjectPtr>,
                         lens::mojom::TextPtr>
      full_image_response_future;
  base::test::TestFuture<lens::proto::LensOverlayUrlResponse>
      url_response_future;
  base::test::TestFuture<lens::proto::LensOverlayInteractionResponse>
      interaction_data_response_future;
  LensOverlayQueryControllerMock query_controller(
      full_image_response_future.GetRepeatingCallback(),
      url_response_future.GetRepeatingCallback(),
      interaction_data_response_future.GetRepeatingCallback(), profile());
  SkBitmap bitmap = CreateNonEmptyBitmap(100, 100);
  query_controller.StartQueryFlow(bitmap);
  task_environment_.RunUntilIdle();

  query_controller.SendTextOnlyQuery("");
  task_environment_.RunUntilIdle();

  ASSERT_TRUE(full_image_response_future.IsReady());
  ASSERT_TRUE(url_response_future.IsReady());
  ASSERT_FALSE(interaction_data_response_future.IsReady());
}

}  // namespace lens
