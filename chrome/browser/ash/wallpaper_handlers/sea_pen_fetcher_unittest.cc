// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/wallpaper_handlers/sea_pen_fetcher.h"

#include <memory>
#include <optional>
#include <string_view>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/wallpaper/sea_pen_image.h"
#include "ash/webui/common/mojom/sea_pen.mojom.h"
#include "base/memory/raw_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/protobuf_matchers.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "chrome/browser/ash/wallpaper_handlers/sea_pen_utils.h"
#include "components/manta/features.h"
#include "components/manta/manta_service_callbacks.h"
#include "components/manta/manta_status.h"
#include "components/manta/proto/manta.pb.h"
#include "components/manta/snapper_provider.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/test/test_screen.h"

namespace ash {}
namespace wallpaper_handlers {

namespace {

constexpr uint32_t kFakeGenerationSeed = 5;

constexpr std::string_view kFakeJpgBytes = "fake_jpg_bytes";

constexpr std::string_view kThumbnailsLatencyMetric =
    "Ash.SeaPen.Api.Thumbnails.Latency";
constexpr std::string_view kThumbnailsStatusCodeMetric =
    "Ash.SeaPen.Api.Thumbnails.MantaStatusCode";
constexpr std::string_view kThumbnailsTimeoutMetric =
    "Ash.SeaPen.Api.Thumbnails.Timeout";
constexpr std::string_view kThumbnailsCountMetric =
    "Ash.SeaPen.Api.Thumbnails.Count";

constexpr std::string_view kWallpaperLatencyMetric =
    "Ash.SeaPen.Api.Wallpaper.Latency";
constexpr std::string_view kWallpaperStatusCodeMetric =
    "Ash.SeaPen.Api.Wallpaper.MantaStatusCode";
constexpr std::string_view kWallpaperTimeoutMetric =
    "Ash.SeaPen.Api.Wallpaper.Timeout";
constexpr std::string_view kWallpaperHasImageMetric =
    "Ash.SeaPen.Api.Wallpaper.HasImage";

std::unique_ptr<manta::proto::Response> CreateMantaResponse(
    size_t output_data_length) {
  auto response = std::make_unique<manta::proto::Response>();
  for (size_t i = 0; i < output_data_length; i++) {
    auto* output_data = response->add_output_data();
    output_data->set_generation_seed(kFakeGenerationSeed + i);
    output_data->mutable_image()->set_serialized_bytes(
        std::string(kFakeJpgBytes));
  }
  return response;
}

testing::Matcher<ash::SeaPenImage> MatchesSeaPenImage(
    const std::string_view expected_jpg_bytes,
    const uint32_t expected_id) {
  return testing::AllOf(
      testing::Field(&ash::SeaPenImage::id, expected_id),
      testing::Field(&ash::SeaPenImage::jpg_bytes, expected_jpg_bytes));
}

class MockSnapperProvider : virtual public manta::SnapperProvider {
 public:
  MockSnapperProvider() : manta::SnapperProvider(nullptr, nullptr) {}

  MockSnapperProvider(const MockSnapperProvider&) = delete;
  MockSnapperProvider& operator=(const MockSnapperProvider&) = delete;

  ~MockSnapperProvider() override = default;

  MOCK_METHOD(void,
              Call,
              (const manta::proto::Request& request,
               net::NetworkTrafficAnnotationTag traffic_annotation,
               manta::MantaProtoResponseCallback done_callback),
              (override));
};

}  // namespace

class SeaPenFetcherTest : public testing::Test {
 public:
  SeaPenFetcherTest() {
    scoped_feature_list_.InitWithFeatures(
        {
            ash::features::kSeaPen,
            ash::features::kFeatureManagementSeaPen,
            manta::features::kMantaService,
        },
        {});
  }

  SeaPenFetcherTest(const SeaPenFetcherTest&) = delete;
  SeaPenFetcherTest& operator=(const SeaPenFetcherTest&) = delete;

  ~SeaPenFetcherTest() override = default;

  void SetUp() override {
    testing::Test::SetUp();
    display::Screen::SetScreenInstance(&test_screen_);
    auto mock_snapper_provider = std::make_unique<MockSnapperProvider>();
    mock_snapper_provider_ =
        static_cast<testing::StrictMock<MockSnapperProvider>*>(
            mock_snapper_provider.get());
    sea_pen_fetcher_ =
        SeaPenFetcher::MakeSeaPenFetcher(std::move(mock_snapper_provider));
  }

  void TearDown() override {
    testing::Test::TearDown();
    display::Screen::SetScreenInstance(nullptr);
    mock_snapper_provider_ = nullptr;
  }

  SeaPenFetcher* sea_pen_fetcher() { return sea_pen_fetcher_.get(); }

  base::HistogramTester& histogram_tester() { return histogram_tester_; }

  testing::StrictMock<MockSnapperProvider>& snapper_provider() {
    return *mock_snapper_provider_;
  }

  void FastForwardBy(const base::TimeDelta delta) {
    task_environment_.FastForwardBy(delta);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::HistogramTester histogram_tester_;
  display::test::TestScreen test_screen_;
  raw_ptr<testing::StrictMock<MockSnapperProvider>> mock_snapper_provider_;
  std::unique_ptr<SeaPenFetcher> sea_pen_fetcher_;
};

TEST_F(SeaPenFetcherTest, ThumbnailsCallsSnapperProvider) {
  auto query =
      ash::personalization_app::mojom ::SeaPenQuery::NewTextQuery("test query");

  EXPECT_CALL(
      snapper_provider(),
      Call(base::test::EqualsProto(CreateMantaRequest(
               query, /*generation_seed=*/std::nullopt,
               /*num_outputs=*/SeaPenFetcher::kNumThumbnailsRequested,
               {880, 440}, manta::proto::FeatureName::CHROMEOS_WALLPAPER)),
           testing::_, testing::_))
      .WillOnce([](const manta::proto::Request& request,
                   net::NetworkTrafficAnnotationTag traffic_annotation,
                   manta::MantaProtoResponseCallback done_callback) {
        base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE,
            base::BindOnce(
                [](manta::MantaProtoResponseCallback delayed_callback) {
                  std::move(delayed_callback)
                      .Run(CreateMantaResponse(
                               SeaPenFetcher::kNumThumbnailsRequested),
                           {.status_code = manta::MantaStatusCode::kOk,
                            .message = std::string()});
                },
                std::move(done_callback)));
      });

  base::test::TestFuture<std::optional<std::vector<ash::SeaPenImage>>,
                         manta::MantaStatusCode>
      fetch_thumbnails_future;

  sea_pen_fetcher()->FetchThumbnails(
      manta::proto::FeatureName::CHROMEOS_WALLPAPER, query,
      fetch_thumbnails_future.GetCallback());

  EXPECT_EQ(manta::MantaStatusCode::kOk,
            fetch_thumbnails_future.Get<manta::MantaStatusCode>());

  std::vector<testing::Matcher<ash::SeaPenImage>> matchers;
  for (size_t i = 0; i < SeaPenFetcher::kNumThumbnailsRequested; i++) {
    matchers.push_back(
        MatchesSeaPenImage(kFakeJpgBytes, kFakeGenerationSeed + i));
  }
  EXPECT_THAT(fetch_thumbnails_future
                  .Get<std::optional<std::vector<ash::SeaPenImage>>>()
                  .value(),
              testing::ElementsAreArray(matchers));

  histogram_tester().ExpectTotalCount(kThumbnailsLatencyMetric, 1);
  histogram_tester().ExpectUniqueSample(kThumbnailsStatusCodeMetric,
                                        manta::MantaStatusCode::kOk, 1);
  histogram_tester().ExpectUniqueSample(kThumbnailsTimeoutMetric, false, 1);
  histogram_tester().ExpectUniqueSample(
      kThumbnailsCountMetric, SeaPenFetcher::kNumThumbnailsRequested, 1);
}

TEST_F(SeaPenFetcherTest, ThumbnailsEmptyReturnsError) {
  EXPECT_CALL(snapper_provider(), Call(testing::_, testing::_, testing::_))
      .WillOnce([](const manta::proto::Request& request,
                   net::NetworkTrafficAnnotationTag traffic_annotation,
                   manta::MantaProtoResponseCallback done_callback) {
        base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE,
            base::BindOnce(
                [](manta::MantaProtoResponseCallback delayed_callback) {
                  std::move(delayed_callback)
                      .Run(CreateMantaResponse(0),
                           {.status_code = manta::MantaStatusCode::kOk,
                            .message = std::string()});
                },
                std::move(done_callback)));
      });

  base::test::TestFuture<std::optional<std::vector<ash::SeaPenImage>>,
                         manta::MantaStatusCode>
      fetch_thumbnails_future;
  sea_pen_fetcher()->FetchThumbnails(
      manta::proto::FeatureName::CHROMEOS_WALLPAPER,
      ash::personalization_app::mojom ::SeaPenQuery::NewTextQuery("test query"),
      fetch_thumbnails_future.GetCallback());

  EXPECT_EQ(manta::MantaStatusCode::kGenericError,
            fetch_thumbnails_future.Get<manta::MantaStatusCode>());
  EXPECT_EQ(std::nullopt,
            fetch_thumbnails_future
                .Get<std::optional<std::vector<ash::SeaPenImage>>>());

  // Recorded an entry in the "0" thumbnail count bucket 1 time.
  histogram_tester().ExpectUniqueSample(kThumbnailsCountMetric, 0, 1);
  histogram_tester().ExpectUniqueSample(kThumbnailsStatusCodeMetric,
                                        manta::MantaStatusCode::kOk, 1);
  histogram_tester().ExpectTotalCount(kThumbnailsLatencyMetric, 1);
  histogram_tester().ExpectUniqueSample(kThumbnailsTimeoutMetric, false, 1);
}

TEST_F(SeaPenFetcherTest, ThumbnailsTimeoutHandled) {
  EXPECT_CALL(snapper_provider(), Call(testing::_, testing::_, testing::_))
      .WillOnce([](const manta::proto::Request& request,
                   net::NetworkTrafficAnnotationTag traffic_annotation,
                   manta::MantaProtoResponseCallback done_callback) {
        // Run `done_callback` but one second too late.
        base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
            FROM_HERE,
            base::BindOnce(
                [](manta::MantaProtoResponseCallback delayed_callback) {
                  std::move(delayed_callback)
                      .Run(CreateMantaResponse(
                               SeaPenFetcher::kNumThumbnailsRequested),
                           {.status_code = manta::MantaStatusCode::kOk,
                            .message = std::string()});
                },
                std::move(done_callback)),
            SeaPenFetcher::kRequestTimeout + base::Seconds(1));
      });

  base::test::TestFuture<std::optional<std::vector<ash::SeaPenImage>>,
                         manta::MantaStatusCode>
      fetch_thumbnails_future;
  sea_pen_fetcher()->FetchThumbnails(
      manta::proto::FeatureName::CHROMEOS_WALLPAPER,
      ash::personalization_app::mojom ::SeaPenQuery::NewTextQuery("test query"),
      fetch_thumbnails_future.GetCallback());

  // Trigger the timeout.
  FastForwardBy(SeaPenFetcher::kRequestTimeout + base::Milliseconds(1));

  EXPECT_EQ(manta::MantaStatusCode::kGenericError,
            fetch_thumbnails_future.Get<manta::MantaStatusCode>());
  EXPECT_EQ(std::nullopt,
            fetch_thumbnails_future
                .Get<std::optional<std::vector<ash::SeaPenImage>>>());

  // Recorded 1 timeout.
  histogram_tester().ExpectUniqueSample(kThumbnailsTimeoutMetric, true, 1);

  // Does not record following metrics on timeout.
  histogram_tester().ExpectTotalCount(kThumbnailsLatencyMetric, 0);
  histogram_tester().ExpectTotalCount(kThumbnailsStatusCodeMetric, 0);
  histogram_tester().ExpectTotalCount(kThumbnailsCountMetric, 0);
}

TEST_F(SeaPenFetcherTest, WallpaperCallsSnapperProvider) {
  auto query =
      ash::personalization_app::mojom ::SeaPenQuery::NewTextQuery("test query");

  EXPECT_CALL(snapper_provider(),
              Call(base::test::EqualsProto(CreateMantaRequest(
                       query, /*generation_seed=*/kFakeGenerationSeed,
                       /*num_outputs=*/1, GetLargestDisplaySizeLandscape(),
                       manta::proto::FeatureName::CHROMEOS_WALLPAPER)),
                   testing::_, testing::_))
      .WillOnce([](const manta::proto::Request& request,
                   net::NetworkTrafficAnnotationTag traffic_annotation,
                   manta::MantaProtoResponseCallback done_callback) {
        base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE,
            base::BindOnce(
                [](manta::MantaProtoResponseCallback delayed_callback) {
                  std::move(delayed_callback)
                      .Run(CreateMantaResponse(1),
                           {.status_code = manta::MantaStatusCode::kOk,
                            .message = std::string()});
                },
                std::move(done_callback)));
      });

  base::test::TestFuture<std::optional<ash::SeaPenImage>>
      fetch_wallpaper_future;
  sea_pen_fetcher()->FetchWallpaper(
      manta::proto::FeatureName::CHROMEOS_WALLPAPER,
      ash::SeaPenImage(std::string(kFakeJpgBytes), kFakeGenerationSeed), query,
      fetch_wallpaper_future.GetCallback());

  EXPECT_THAT(fetch_wallpaper_future.Get().value(),
              MatchesSeaPenImage(kFakeJpgBytes, kFakeGenerationSeed));

  histogram_tester().ExpectTotalCount(kWallpaperLatencyMetric, 1);
  histogram_tester().ExpectUniqueSample(kWallpaperStatusCodeMetric,
                                        manta::MantaStatusCode::kOk, 1);
  histogram_tester().ExpectUniqueSample(kWallpaperTimeoutMetric, false, 1);
  histogram_tester().ExpectUniqueSample(kWallpaperHasImageMetric, true, 1);
}

TEST_F(SeaPenFetcherTest, WallpaperHandlesEmptyImage) {
  EXPECT_CALL(snapper_provider(), Call(testing::_, testing::_, testing::_))
      .WillOnce([](const manta::proto::Request& request,
                   net::NetworkTrafficAnnotationTag traffic_annotation,
                   manta::MantaProtoResponseCallback done_callback) {
        base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE,
            base::BindOnce(
                [](manta::MantaProtoResponseCallback delayed_callback) {
                  std::move(delayed_callback)
                      .Run(CreateMantaResponse(0),
                           {.status_code = manta::MantaStatusCode::kOk,
                            .message = std::string()});
                },
                std::move(done_callback)));
      });

  base::test::TestFuture<std::optional<ash::SeaPenImage>>
      fetch_wallpaper_future;
  sea_pen_fetcher()->FetchWallpaper(
      manta::proto::FeatureName::CHROMEOS_WALLPAPER,
      ash::SeaPenImage(std::string(kFakeJpgBytes), kFakeGenerationSeed),
      ash::personalization_app::mojom ::SeaPenQuery::NewTextQuery("test query"),
      fetch_wallpaper_future.GetCallback());

  EXPECT_FALSE(fetch_wallpaper_future.Get().has_value());

  histogram_tester().ExpectTotalCount(kWallpaperLatencyMetric, 1);
  histogram_tester().ExpectUniqueSample(kWallpaperStatusCodeMetric,
                                        manta::MantaStatusCode::kOk, 1);
  histogram_tester().ExpectUniqueSample(kWallpaperTimeoutMetric, false, 1);
  histogram_tester().ExpectUniqueSample(kWallpaperHasImageMetric, false, 1);
}

TEST_F(SeaPenFetcherTest, WallpaperHandlesTimeout) {
  EXPECT_CALL(snapper_provider(), Call(testing::_, testing::_, testing::_))
      .WillOnce([](const manta::proto::Request& request,
                   net::NetworkTrafficAnnotationTag traffic_annotation,
                   manta::MantaProtoResponseCallback done_callback) {
        base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
            FROM_HERE,
            base::BindOnce(
                [](manta::MantaProtoResponseCallback delayed_callback) {
                  std::move(delayed_callback)
                      .Run(CreateMantaResponse(1),
                           {.status_code = manta::MantaStatusCode::kOk,
                            .message = std::string()});
                },
                std::move(done_callback)),
            SeaPenFetcher::kRequestTimeout + base::Seconds(1));
      });

  base::test::TestFuture<std::optional<ash::SeaPenImage>>
      fetch_wallpaper_future;
  sea_pen_fetcher()->FetchWallpaper(
      manta::proto::FeatureName::CHROMEOS_WALLPAPER,
      ash::SeaPenImage(std::string(kFakeJpgBytes), kFakeGenerationSeed),
      ash::personalization_app::mojom ::SeaPenQuery::NewTextQuery("test query"),
      fetch_wallpaper_future.GetCallback());

  FastForwardBy(SeaPenFetcher::kRequestTimeout + base::Milliseconds(1));

  EXPECT_FALSE(fetch_wallpaper_future.Get().has_value());

  // Timeout metric records true.
  histogram_tester().ExpectUniqueSample(kWallpaperTimeoutMetric, true, 1);

  // No other metrics recorded for timeout.
  histogram_tester().ExpectTotalCount(kWallpaperLatencyMetric, 0);
  histogram_tester().ExpectTotalCount(kWallpaperStatusCodeMetric, 0);
  histogram_tester().ExpectTotalCount(kWallpaperHasImageMetric, 0);
}

}  // namespace wallpaper_handlers
