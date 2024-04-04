// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "lens_overlay_query_controller.h"

#include <memory>
#include <string>
#include <vector>

#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/endpoint_fetcher/endpoint_fetcher.h"
#include "content/public/test/browser_task_environment.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "testing/gtest/include/gtest/gtest.h"
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
      base::RepeatingCallback<void(lens::proto::LensOverlayFullImageResponse)>
          full_image_callback,
      base::RepeatingCallback<void(lens::proto::LensOverlayUrlResponse)>
          url_callback,
      base::RepeatingCallback<void(lens::proto::LensOverlayInteractionResponse)>
          interaction_data_callback)
      : LensOverlayQueryController(full_image_callback,
                                   url_callback,
                                   interaction_data_callback) {}
  ~LensOverlayQueryControllerMock() override = default;
  EndpointResponse fake_endpoint_response_;

 protected:
  std::unique_ptr<EndpointFetcher> CreateEndpointFetcher(
      lens::LensOverlayServerRequest request_data) override {
    return std::make_unique<FakeEndpointFetcher>(fake_endpoint_response_);
  }
};

class LensOverlayQueryControllerTest : public testing::Test {
 public:
  LensOverlayQueryControllerTest()
      : task_environment_(content::BrowserTaskEnvironment::IO_MAINLOOP,
                          base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

 protected:
  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(LensOverlayQueryControllerTest, FetchInitialQuery_ReturnsResponse) {
  task_environment_.RunUntilIdle();
  base::test::TestFuture<lens::proto::LensOverlayFullImageResponse>
      full_image_response_future;
  LensOverlayQueryControllerMock query_controller(
      full_image_response_future.GetRepeatingCallback(), base::NullCallback(),
      base::NullCallback());
  SkBitmap bitmap;
  query_controller.StartQueryFlow(bitmap);

  task_environment_.RunUntilIdle();
  ASSERT_TRUE(full_image_response_future.IsReady());
}

TEST_F(LensOverlayQueryControllerTest, FetchInteraction_ReturnsResponses) {
  task_environment_.RunUntilIdle();
  base::test::TestFuture<lens::proto::LensOverlayFullImageResponse>
      full_image_response_future;
  base::test::TestFuture<lens::proto::LensOverlayUrlResponse>
      url_response_future;
  base::test::TestFuture<lens::proto::LensOverlayInteractionResponse>
      interaction_data_response_future;
  LensOverlayQueryControllerMock query_controller(
      full_image_response_future.GetRepeatingCallback(),
      url_response_future.GetRepeatingCallback(),
      interaction_data_response_future.GetRepeatingCallback());
  SkBitmap bitmap;
  query_controller.StartQueryFlow(bitmap);
  lens::proto::LensOverlayRequest interaction_request;
  query_controller.SendInteraction(interaction_request);

  task_environment_.RunUntilIdle();
  ASSERT_TRUE(full_image_response_future.IsReady());
  ASSERT_EQ(url_response_future.Get().url(), "");
  ASSERT_EQ(interaction_data_response_future.Get().suggest_signals(), "");
}

}  // namespace lens
