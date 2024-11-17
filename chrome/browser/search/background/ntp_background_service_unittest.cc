// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/background/ntp_background_service.h"

#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/search/background/ntp_background_data.h"
#include "chrome/browser/search/background/ntp_background_service_observer.h"
#include "components/search/ntp_features.h"
#include "components/version_info/version_info.h"
#include "content/public/test/browser_task_environment.h"
#include "services/network/public/cpp/data_element.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ui_base_features.h"
#include "url/gurl.h"

using testing::Eq;
using testing::StartsWith;

const char kTestImageUrl[] = "https://wallpapers.co/some_image";
const char kTestActionUrl[] = "https://wallpapers.co/some_image/learn_more";

namespace {

class MockNtpBackgroundServiceObserver : public NtpBackgroundServiceObserver {
 public:
  MOCK_METHOD0(OnCollectionInfoAvailable, void());
  MOCK_METHOD0(OnCollectionImagesAvailable, void());
  MOCK_METHOD0(OnNextCollectionImageAvailable, void());
};

}  // namespace

class NtpBackgroundServiceTest : public testing::Test {
 public:
  NtpBackgroundServiceTest()
      : task_environment_(content::BrowserTaskEnvironment::IO_MAINLOOP),
        test_shared_loader_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_)) {
  }

  void TearDown() override {
    if (service_) {
      service_->RemoveObserver(&observer_);
    }
  }

  void SetUpResponseWithNetworkSuccess(
      const GURL& load_url,
      const std::string& response = std::string()) {
    test_url_loader_factory_.AddResponse(load_url.spec(), response);
  }

  void SetUpResponseWithData(const GURL& load_url,
                             const std::string& response) {
    test_url_loader_factory_.SetInterceptor(base::BindLambdaForTesting(
        [&](const network::ResourceRequest& request) {}));
    SetUpResponseWithNetworkSuccess(load_url, response);
  }

  void SetUpResponseWithNetworkError(const GURL& load_url) {
    test_url_loader_factory_.AddResponse(load_url.spec(), std::string(),
                                         net::HTTP_NOT_FOUND);
  }

  NtpBackgroundService* service() {
    if (!service_) {
      service_ =
          std::make_unique<NtpBackgroundService>(test_shared_loader_factory_);
      service_->AddObserver(&observer_);
    }
    return service_.get();
  }

  network::TestURLLoaderFactory* test_url_loader_factory() {
    return &test_url_loader_factory_;
  }

 protected:
  // Required to run tests from UI and threads.
  content::BrowserTaskEnvironment task_environment_;

  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;
  MockNtpBackgroundServiceObserver observer_;
  base::test::ScopedFeatureList feature_list_;
  base::HistogramTester histogram_tester_;
  std::unique_ptr<NtpBackgroundService> service_;
};

TEST_F(NtpBackgroundServiceTest, CollectionRequest) {
  g_browser_process->SetApplicationLocale("foo");

  service()->FetchCollectionInfo();
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return test_url_loader_factory()->pending_requests()->size() == 1u;
  }));

  std::string request_body(test_url_loader_factory()
                               ->pending_requests()
                               ->at(0)
                               .request.request_body->elements()
                               ->at(0)
                               .As<network::DataElementBytes>()
                               .AsStringPiece());
  ntp::background::GetCollectionsRequest collection_request;
  EXPECT_TRUE(collection_request.ParseFromString(request_body));
  EXPECT_EQ("foo", collection_request.language());
  EXPECT_EQ(4, collection_request.filtering_label_size());
  EXPECT_EQ("chrome_desktop_ntp", collection_request.filtering_label(0));
  EXPECT_EQ("chrome_desktop_ntp.M" + version_info::GetMajorVersionNumber(),
            collection_request.filtering_label(1));
  EXPECT_EQ("chrome_desktop_ntp.panorama",
            collection_request.filtering_label(2));
  EXPECT_EQ("chrome_desktop_ntp.gm3", collection_request.filtering_label(3));
}

TEST_F(NtpBackgroundServiceTest,
       CollectionRequestWithImageErrorDetectionEnabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      ntp_features::kNtpBackgroundImageErrorDetection);
  g_browser_process->SetApplicationLocale("foo");

  service()->FetchCollectionInfo();
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return test_url_loader_factory()->pending_requests()->size() == 1u;
  }));

  std::string request_body(test_url_loader_factory()
                               ->pending_requests()
                               ->at(0)
                               .request.request_body->elements()
                               ->at(0)
                               .As<network::DataElementBytes>()
                               .AsStringPiece());
  ntp::background::GetCollectionsRequest collection_request;
  EXPECT_TRUE(collection_request.ParseFromString(request_body));
  EXPECT_EQ("foo", collection_request.language());
  EXPECT_EQ(5, collection_request.filtering_label_size());
  EXPECT_EQ("chrome_desktop_ntp.error_detection",
            collection_request.filtering_label(4));
  EXPECT_EQ("chrome_desktop_ntp.M" + version_info::GetMajorVersionNumber(),
            collection_request.filtering_label(1));
  EXPECT_EQ("chrome_desktop_ntp.panorama",
            collection_request.filtering_label(2));
  EXPECT_EQ("chrome_desktop_ntp.gm3", collection_request.filtering_label(3));
}

TEST_F(NtpBackgroundServiceTest, CollectionInfoNetworkError) {
  SetUpResponseWithNetworkError(service()->GetCollectionsLoadURLForTesting());

  ASSERT_TRUE(service()->collection_info().empty());

  EXPECT_CALL(observer_, OnCollectionInfoAvailable).Times(1);
  service()->FetchCollectionInfo();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(service()->collection_info().empty());
  EXPECT_EQ(service()->collection_error_info().error_type,
            ErrorType::NET_ERROR);
}

TEST_F(NtpBackgroundServiceTest, BadCollectionsResponse) {
  SetUpResponseWithData(service()->GetCollectionsLoadURLForTesting(),
                        "bad serialized GetCollectionsResponse");

  ASSERT_TRUE(service()->collection_info().empty());

  EXPECT_CALL(observer_, OnCollectionInfoAvailable).Times(1);
  service()->FetchCollectionInfo();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(service()->collection_info().empty());
  EXPECT_EQ(service()->collection_error_info().error_type,
            ErrorType::SERVICE_ERROR);
}

TEST_F(NtpBackgroundServiceTest, GoodCollectionsResponse) {
  ntp::background::Collection collection;
  collection.set_collection_id("shapes");
  collection.set_collection_name("Shapes");
  collection.add_preview()->set_image_url(kTestImageUrl);
  ntp::background::GetCollectionsResponse response;
  *response.add_collections() = collection;
  std::string response_string;
  response.SerializeToString(&response_string);

  SetUpResponseWithData(service()->GetCollectionsLoadURLForTesting(),
                        response_string);

  ASSERT_TRUE(service()->collection_info().empty());

  EXPECT_CALL(observer_, OnCollectionInfoAvailable).Times(1);
  service()->FetchCollectionInfo();
  base::RunLoop().RunUntilIdle();

  CollectionInfo collection_info;
  collection_info.collection_id = collection.collection_id();
  collection_info.collection_name = collection.collection_name();
  collection_info.preview_image_url =
      GURL(collection.preview(0).image_url() + GetThumbnailImageOptions());

  EXPECT_FALSE(service()->collection_info().empty());
  EXPECT_THAT(service()->collection_info().at(0), Eq(collection_info));
  EXPECT_EQ(service()->collection_error_info().error_type, ErrorType::NONE);
}

TEST_F(NtpBackgroundServiceTest, BrokenCollectionPreviewImageHasNoReplacement) {
  ntp::background::Collection collection;
  collection.set_collection_id("shapes");
  collection.set_collection_name("Shapes");

  // Add fake image to collection that produces a network error when accessed.
  ntp::background::Image image;
  image.set_asset_id(12345);
  image.set_image_url("https://wallpapers.co/some_other_image");
  image.add_attribution()->set_text("different attribution text");
  ntp::background::GetImagesInCollectionResponse image_response;
  *image_response.add_images() = image;
  std::string image_response_string;
  image_response.SerializeToString(&image_response_string);
  SetUpResponseWithData(service()->GetImagesURLForTesting(),
                        image_response_string);
  SetUpResponseWithNetworkError(
      GURL(image.image_url() + GetThumbnailImageOptions()));

  base::RunLoop run_loop;
  auto replacement_image_callback = base::BindLambdaForTesting(
      [&](const std::optional<GURL>& replacement_image_url) {
        EXPECT_FALSE(replacement_image_url.has_value());
        run_loop.Quit();
      });
  service()->FetchReplacementCollectionPreviewImage(
      collection.collection_id(), std::move(replacement_image_callback));
  run_loop.Run();
}

TEST_F(NtpBackgroundServiceTest, BrokenCollectionPreviewImageHasReplacement) {
  ntp::background::Collection collection;
  collection.set_collection_id("shapes");
  collection.set_collection_name("Shapes");

  // Add image to collection that produces a network success when accessed.
  ntp::background::Image image_1;
  image_1.set_asset_id(12345);
  image_1.set_image_url("https://wallpapers.co/some_image");
  image_1.add_attribution()->set_text("attribution text");
  ntp::background::GetImagesInCollectionResponse image_response;
  *image_response.add_images() = image_1;
  ntp::background::Image image_2;
  image_2.set_asset_id(23451);
  image_2.set_image_url("https://wallpapers.co/some_other_image");
  image_2.add_attribution()->set_text("different attribution text");
  *image_response.add_images() = image_2;
  std::string image_response_string;
  image_response.SerializeToString(&image_response_string);
    SetUpResponseWithData(service()->GetImagesURLForTesting(),
                          image_response_string);
    SetUpResponseWithNetworkError(
        GURL(image_1.image_url() + GetThumbnailImageOptions()));
    SetUpResponseWithNetworkSuccess(
        GURL(image_2.image_url() + GetThumbnailImageOptions()));

    base::RunLoop run_loop;
    auto replacement_image_callback = base::BindLambdaForTesting(
        [&](const std::optional<GURL>& replacement_image_url) {
          EXPECT_TRUE(replacement_image_url.has_value());
          EXPECT_EQ(replacement_image_url.value(),
                    GURL(image_2.image_url() + GetThumbnailImageOptions()));
          run_loop.Quit();
        });
    service()->FetchReplacementCollectionPreviewImage(
        collection.collection_id(), std::move(replacement_image_callback));
    run_loop.Run();
}

TEST_F(NtpBackgroundServiceTest, CollectionImagesNetworkError) {
  SetUpResponseWithNetworkError(service()->GetImagesURLForTesting());

  ASSERT_TRUE(service()->collection_images().empty());

  EXPECT_CALL(observer_, OnCollectionImagesAvailable).Times(1);
  service()->FetchCollectionImageInfo("shapes");
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(service()->collection_images().empty());
  EXPECT_EQ(service()->collection_images_error_info().error_type,
            ErrorType::NET_ERROR);
}

TEST_F(NtpBackgroundServiceTest, BadCollectionImagesResponse) {
  SetUpResponseWithData(service()->GetImagesURLForTesting(),
                        "bad serialized GetImagesInCollectionResponse");

  ASSERT_TRUE(service()->collection_images().empty());

  EXPECT_CALL(observer_, OnCollectionImagesAvailable).Times(1);
  service()->FetchCollectionImageInfo("shapes");
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(service()->collection_images().empty());
  EXPECT_EQ(service()->collection_images_error_info().error_type,
            ErrorType::SERVICE_ERROR);
}

TEST_F(NtpBackgroundServiceTest, ImageInCollectionHasNetworkError) {
  ntp::background::Image image;
  image.set_asset_id(12345);
  image.set_image_url(kTestImageUrl);
  image.add_attribution()->set_text("attribution text");
  image.set_action_url(kTestActionUrl);
  ntp::background::GetImagesInCollectionResponse response;
  *response.add_images() = image;
  std::string response_string;
  response.SerializeToString(&response_string);

  SetUpResponseWithData(service()->GetImagesURLForTesting(), response_string);

  ASSERT_TRUE(service()->collection_images().empty());

  EXPECT_CALL(observer_, OnCollectionImagesAvailable).Times(1);
  service()->FetchCollectionImageInfo("shapes");
  base::RunLoop().RunUntilIdle();

  CollectionImage collection_image;
  collection_image.collection_id = "shapes";
  collection_image.asset_id = image.asset_id();
  collection_image.thumbnail_image_url =
      GURL(image.image_url() + GetThumbnailImageOptions());
  collection_image.image_url =
      GURL(image.image_url() + service()->GetImageOptionsForTesting());
  collection_image.attribution.push_back(image.attribution(0).text());
  collection_image.attribution_action_url = GURL(image.action_url());

    EXPECT_FALSE(service()->collection_images().empty());
    EXPECT_THAT(service()->collection_images().at(0), Eq(collection_image));
    EXPECT_EQ(service()->collection_images_error_info().error_type,
              ErrorType::NONE);
}

TEST_F(NtpBackgroundServiceTest, GoodCollectionImagesResponse) {
  ntp::background::Image image;
  image.set_asset_id(12345);
  image.set_image_url(kTestImageUrl);
  image.add_attribution()->set_text("attribution text");
  image.set_action_url(kTestActionUrl);
  ntp::background::GetImagesInCollectionResponse response;
  *response.add_images() = image;
  std::string response_string;
  response.SerializeToString(&response_string);

  SetUpResponseWithData(service()->GetImagesURLForTesting(), response_string);

  ASSERT_TRUE(service()->collection_images().empty());

  EXPECT_CALL(observer_, OnCollectionImagesAvailable).Times(1);
  service()->FetchCollectionImageInfo("shapes");
  base::RunLoop().RunUntilIdle();

  CollectionImage collection_image;
  collection_image.collection_id = "shapes";
  collection_image.asset_id = image.asset_id();
  collection_image.thumbnail_image_url =
      GURL(image.image_url() + GetThumbnailImageOptions());
  collection_image.image_url =
      GURL(image.image_url() + service()->GetImageOptionsForTesting());
  collection_image.attribution.push_back(image.attribution(0).text());
  collection_image.attribution_action_url = GURL(image.action_url());

  EXPECT_FALSE(service()->collection_images().empty());
  EXPECT_THAT(service()->collection_images().at(0), Eq(collection_image));
  EXPECT_EQ(service()->collection_images_error_info().error_type,
            ErrorType::NONE);
}

TEST_F(NtpBackgroundServiceTest,
       CollectionImageInfoRequestsAreIgnoredIfAnotherIsInProgress) {
  ntp::background::Collection collection;
  collection.set_collection_id("shapes");
  collection.set_collection_name("Shapes");
  collection.add_preview()->set_image_url(kTestImageUrl);
  ntp::background::GetCollectionsResponse collection_response;
  *collection_response.add_collections() = collection;
  std::string collection_response_string;
  collection_response.SerializeToString(&collection_response_string);

  ntp::background::Image image;
  image.set_asset_id(12345);
  image.set_image_url(kTestImageUrl);
  image.add_attribution()->set_text("attribution text");
  ntp::background::GetImagesInCollectionResponse image_response;
  *image_response.add_images() = image;
  std::string image_response_string;
  image_response.SerializeToString(&image_response_string);

  SetUpResponseWithData(service()->GetCollectionsLoadURLForTesting(),
                        collection_response_string);
  SetUpResponseWithData(service()->GetImagesURLForTesting(),
                        image_response_string);

  ASSERT_TRUE(service()->collection_info().empty());
  ASSERT_TRUE(service()->collection_images().empty());

  EXPECT_CALL(observer_, OnCollectionInfoAvailable).Times(1);
  EXPECT_CALL(observer_, OnCollectionImagesAvailable).Times(1);
  service()->FetchCollectionInfo();
  service()->FetchCollectionImageInfo("shapes");
  // Subsequent requests are ignored while the loader is in use.
  service()->FetchCollectionImageInfo("colors");
  base::RunLoop().RunUntilIdle();

  CollectionInfo collection_info;
  collection_info.collection_id = collection.collection_id();
  collection_info.collection_name = collection.collection_name();
  collection_info.preview_image_url =
      GURL(collection.preview(0).image_url() + GetThumbnailImageOptions());

  CollectionImage collection_image;
  collection_image.collection_id = "shapes";
  collection_image.asset_id = image.asset_id();
  collection_image.thumbnail_image_url =
      GURL(image.image_url() + GetThumbnailImageOptions());
  collection_image.image_url =
      GURL(image.image_url() + service()->GetImageOptionsForTesting());
  collection_image.attribution.push_back(image.attribution(0).text());

  EXPECT_FALSE(service()->collection_info().empty());
  EXPECT_THAT(service()->collection_info().at(0), Eq(collection_info));
  EXPECT_FALSE(service()->collection_images().empty());
  EXPECT_THAT(service()->collection_images().at(0), Eq(collection_image));
}

TEST_F(NtpBackgroundServiceTest,
       CollectionImageInfoCanBeSuccessfullyFetchedMultipleTimes) {
  ntp::background::Image image;
  image.set_image_url(kTestImageUrl);
  ntp::background::GetImagesInCollectionResponse response;
  *response.add_images() = image;
  std::string response_string;
  response.SerializeToString(&response_string);

  SetUpResponseWithData(service()->GetImagesURLForTesting(), response_string);

  ASSERT_TRUE(service()->collection_images().empty());

  EXPECT_CALL(observer_, OnCollectionImagesAvailable).Times(2);
  service()->FetchCollectionImageInfo("shapes");
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(service()->collection_images().empty());
  EXPECT_THAT(service()->collection_images().at(0).collection_id, "shapes");

  service()->FetchCollectionImageInfo("colors");
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(service()->collection_images().at(0).collection_id, "colors");
}

TEST_F(NtpBackgroundServiceTest, NextImageNetworkError) {
  SetUpResponseWithNetworkError(service()->GetNextImageURLForTesting());

  service()->FetchNextCollectionImage("shapes", std::nullopt);
  base::RunLoop().RunUntilIdle();

  EXPECT_THAT(service()->next_image_error_info().error_type,
              Eq(ErrorType::NET_ERROR));
}

TEST_F(NtpBackgroundServiceTest, BadNextImageResponse) {
  SetUpResponseWithData(service()->GetNextImageURLForTesting(),
                        "bad serialized GetImageFromCollectionResponse");

  service()->FetchNextCollectionImage("shapes", std::nullopt);
  base::RunLoop().RunUntilIdle();

  EXPECT_THAT(service()->next_image_error_info().error_type,
              Eq(ErrorType::SERVICE_ERROR));
}

TEST_F(NtpBackgroundServiceTest, GoodNextImageResponse) {
  ntp::background::Image image;
  image.set_asset_id(12345);
  image.set_image_url(kTestImageUrl);
  image.add_attribution()->set_text("attribution text");
  image.set_action_url(kTestActionUrl);
  ntp::background::GetImageFromCollectionResponse response;
  *response.mutable_image() = image;
  response.set_resume_token("resume1");
  std::string response_string;
  response.SerializeToString(&response_string);

  SetUpResponseWithData(service()->GetNextImageURLForTesting(),
                        response_string);

  // NOTE: the effect of the resume token in the request (i.e. prevent images
  // from being repeated) cannot be verified in a unit test.
  service()->FetchNextCollectionImage("shapes", "resume0");
  base::RunLoop().RunUntilIdle();

  CollectionImage collection_image;
  collection_image.collection_id = "shapes";
  collection_image.asset_id = image.asset_id();
  collection_image.thumbnail_image_url =
      GURL(image.image_url() + GetThumbnailImageOptions());
  collection_image.image_url =
      GURL(image.image_url() + service()->GetImageOptionsForTesting());
  collection_image.attribution.push_back(image.attribution(0).text());
  collection_image.attribution_action_url = GURL(image.action_url());

  EXPECT_THAT(service()->next_image(), Eq(collection_image));
  EXPECT_THAT(service()->next_image_resume_token(), Eq("resume1"));

  EXPECT_THAT(service()->collection_images_error_info().error_type,
              Eq(ErrorType::NONE));
}

TEST_F(NtpBackgroundServiceTest, MultipleRequestsNextImage) {
  ntp::background::Image image;
  image.set_asset_id(12345);
  image.set_image_url(kTestImageUrl);
  image.add_attribution()->set_text("attribution text");
  image.set_action_url(kTestActionUrl);
  ntp::background::GetImageFromCollectionResponse response;
  *response.mutable_image() = image;
  response.set_resume_token("resume1");
  std::string response_string;
  response.SerializeToString(&response_string);

  SetUpResponseWithData(service()->GetNextImageURLForTesting(),
                        response_string);

  // NOTE: the effect of the resume token in the request (i.e. prevent images
  // from being repeated) cannot be verified in a unit test.
  EXPECT_CALL(observer_, OnNextCollectionImageAvailable).Times(1);
  service()->FetchNextCollectionImage("shapes", std::nullopt);
  // Subsequent requests are ignored while the loader is in use.
  service()->FetchNextCollectionImage("shapes", "resume0");
  base::RunLoop().RunUntilIdle();

  CollectionImage collection_image;
  collection_image.collection_id = "shapes";
  collection_image.asset_id = image.asset_id();
  collection_image.thumbnail_image_url =
      GURL(image.image_url() + GetThumbnailImageOptions());
  collection_image.image_url =
      GURL(image.image_url() + service()->GetImageOptionsForTesting());
  collection_image.attribution.push_back(image.attribution(0).text());
  collection_image.attribution_action_url = GURL(image.action_url());

  EXPECT_THAT(service()->next_image(), Eq(collection_image));
  EXPECT_THAT(service()->next_image_resume_token(), Eq("resume1"));

  EXPECT_THAT(service()->collection_images_error_info().error_type,
              Eq(ErrorType::NONE));
}

TEST_F(NtpBackgroundServiceTest, CheckValidAndInvalidBackdropUrls) {
  ntp::background::Image image;
  image.set_asset_id(12345);
  image.set_image_url(kTestImageUrl);
  image.add_attribution()->set_text("attribution text");
  image.set_action_url(kTestActionUrl);
  ntp::background::GetImagesInCollectionResponse response;
  *response.add_images() = image;
  std::string response_string;
  response.SerializeToString(&response_string);

  SetUpResponseWithData(service()->GetImagesURLForTesting(), response_string);

  ASSERT_TRUE(service()->collection_images().empty());

  EXPECT_CALL(observer_, OnCollectionImagesAvailable).Times(1);
  service()->FetchCollectionImageInfo("shapes");
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(service()->IsValidBackdropUrl(
      GURL(image.image_url() + service()->GetImageOptionsForTesting())));
  EXPECT_FALSE(service()->IsValidBackdropUrl(
      GURL("http://wallpapers.co/some_image=imageOptions")));
  EXPECT_FALSE(service()->IsValidBackdropUrl(
      GURL("https://wallpapers.co/another_image")));
}

TEST_F(NtpBackgroundServiceTest, GetThumbnailUrl) {
  const GURL kInvalidUrl("foo");
  const GURL kValidUrl("https://www.foo.com");
  const GURL kValidThumbnailUrl("https://www.foo.com/thumbnail");
  service()->AddValidBackdropUrlWithThumbnailForTesting(kValidUrl,
                                                        kValidThumbnailUrl);

  EXPECT_EQ(kValidThumbnailUrl, service()->GetThumbnailUrl(kValidUrl));
  EXPECT_EQ(GURL(), service()->GetThumbnailUrl(kInvalidUrl));
}

TEST_F(NtpBackgroundServiceTest, OverrideBaseUrl) {
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      "collections-base-url", "https://foo.com");
  service()->FetchCollectionInfo();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1u, test_url_loader_factory()->pending_requests()->size());
  EXPECT_EQ("https://foo.com/cast/chromecast/home/wallpaper/collections?rt=b",
            test_url_loader_factory()->pending_requests()->at(0).request.url);
}

TEST_F(NtpBackgroundServiceTest, VerifyURLMetricsWithNetworkSuccess) {
  SetUpResponseWithNetworkSuccess(GURL(kTestImageUrl));
  histogram_tester_.ExpectTotalCount(
      "NewTabPage.BackgroundService.Images.Headers.RequestLatency", 0);
  histogram_tester_.ExpectTotalCount(
      "NewTabPage.BackgroundService.Images.Headers.StatusCode", 0);

  base::OnceCallback<void(int)> image_url_headers_received_callback =
      base::BindLambdaForTesting([&](int headers_response_code) {
        EXPECT_EQ(headers_response_code, net::HTTP_OK);
      });
  service()->VerifyImageURL(GURL(kTestImageUrl),
                            std::move(image_url_headers_received_callback));
  base::RunLoop().RunUntilIdle();

  histogram_tester_.ExpectTotalCount(
      "NewTabPage.BackgroundService.Images.Headers.RequestLatency", 1);
  histogram_tester_.ExpectTotalCount(
      "NewTabPage.BackgroundService.Images.Headers.RequestLatency.Ok", 1);
  histogram_tester_.ExpectTotalCount(
      "NewTabPage.BackgroundService.Images.Headers.StatusCode", 1);
  ASSERT_EQ(1, histogram_tester_.GetBucketCount(
                   "NewTabPage.BackgroundService.Images.Headers.StatusCode",
                   net::HTTP_OK));
}

TEST_F(NtpBackgroundServiceTest, VerifyURLMetricsWithNetworkError) {
  SetUpResponseWithNetworkError(GURL(kTestImageUrl));
  histogram_tester_.ExpectTotalCount(
      "NewTabPage.BackgroundService.Images.Headers.RequestLatency", 0);
  histogram_tester_.ExpectTotalCount(
      "NewTabPage.BackgroundService.Images.Headers.StatusCode", 0);

  base::OnceCallback<void(int)> image_url_headers_received_callback =
      base::BindLambdaForTesting([&](int headers_response_code) {
        EXPECT_EQ(headers_response_code, net::HTTP_NOT_FOUND);
      });
  service()->VerifyImageURL(GURL(kTestImageUrl),
                            std::move(image_url_headers_received_callback));
  base::RunLoop().RunUntilIdle();

  histogram_tester_.ExpectTotalCount(
      "NewTabPage.BackgroundService.Images.Headers.RequestLatency", 1);
  histogram_tester_.ExpectTotalCount(
      "NewTabPage.BackgroundService.Images.Headers.RequestLatency.NotFound", 1);
  histogram_tester_.ExpectTotalCount(
      "NewTabPage.BackgroundService.Images.Headers.StatusCode", 1);
  ASSERT_EQ(1, histogram_tester_.GetBucketCount(
                   "NewTabPage.BackgroundService.Images.Headers.StatusCode",
                   net::HTTP_NOT_FOUND));
}
