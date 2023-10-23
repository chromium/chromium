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

class NtpBackgroundServiceTest : public testing::Test,
                                 public ::testing::WithParamInterface<bool> {
 public:
  NtpBackgroundServiceTest()
      : task_environment_(content::BrowserTaskEnvironment::IO_MAINLOOP),
        test_shared_loader_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_)) {
    feature_list_.InitWithFeatureState(
        std::move(ntp_features::kNtpBackgroundImageErrorDetection),
        BackgroundImageErrorDetectionEnabled());
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

  bool BackgroundImageErrorDetectionEnabled() const { return GetParam(); }

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

INSTANTIATE_TEST_SUITE_P(All, NtpBackgroundServiceTest, ::testing::Bool());

TEST_P(NtpBackgroundServiceTest, CollectionRequest) {
  g_browser_process->SetApplicationLocale("foo");
  service()->FetchCollectionInfo();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1u, test_url_loader_factory()->pending_requests()->size());
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
  if (BackgroundImageErrorDetectionEnabled()) {
    EXPECT_EQ(4, collection_request.filtering_label_size());
    EXPECT_EQ("chrome_desktop_ntp.error_detection",
              collection_request.filtering_label(3));
  } else {
    EXPECT_EQ(3, collection_request.filtering_label_size());
  }
  EXPECT_EQ("chrome_desktop_ntp", collection_request.filtering_label(0));
  EXPECT_EQ("chrome_desktop_ntp.M" + version_info::GetMajorVersionNumber(),
            collection_request.filtering_label(1));
  EXPECT_EQ("chrome_desktop_ntp.panorama",
            collection_request.filtering_label(2));
}

TEST_P(NtpBackgroundServiceTest, CollectionRequestWithGM3Enabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {features::kChromeRefresh2023, features::kChromeWebuiRefresh2023}, {});

  g_browser_process->SetApplicationLocale("foo");
  service()->FetchCollectionInfo();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1u, test_url_loader_factory()->pending_requests()->size());
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
  if (BackgroundImageErrorDetectionEnabled()) {
    EXPECT_EQ(5, collection_request.filtering_label_size());
    EXPECT_EQ("chrome_desktop_ntp.error_detection",
              collection_request.filtering_label(4));
  } else {
    EXPECT_EQ(4, collection_request.filtering_label_size());
  }
  EXPECT_EQ("chrome_desktop_ntp", collection_request.filtering_label(0));
  EXPECT_EQ("chrome_desktop_ntp.M" + version_info::GetMajorVersionNumber(),
            collection_request.filtering_label(1));
  EXPECT_EQ("chrome_desktop_ntp.panorama",
            collection_request.filtering_label(2));
  EXPECT_EQ("chrome_desktop_ntp.gm3", collection_request.filtering_label(3));
}

TEST_P(NtpBackgroundServiceTest, CollectionInfoNetworkError) {
  SetUpResponseWithNetworkError(service()->GetCollectionsLoadURLForTesting());

  ASSERT_TRUE(service()->collection_info().empty());

  EXPECT_CALL(observer_, OnCollectionInfoAvailable).Times(1);
  service()->FetchCollectionInfo();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(service()->collection_info().empty());
  EXPECT_EQ(service()->collection_error_info().error_type,
            ErrorType::NET_ERROR);
}

TEST_P(NtpBackgroundServiceTest, BadCollectionsResponse) {
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

TEST_P(NtpBackgroundServiceTest, GoodCollectionsResponse) {
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
  if (BackgroundImageErrorDetectionEnabled()) {
    SetUpResponseWithNetworkSuccess(
        GURL(collection.preview(0).image_url() + GetThumbnailImageOptions()));
  }

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

TEST_P(NtpBackgroundServiceTest, BrokenCollectionPreviewImageHasNoReplacement) {
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
  // Set up for when BackgroundImageErrorDetectionEnabled is true.
  ntp::background::Image image;
  image.set_asset_id(12345);
  image.set_image_url("https://wallpapers.co/some_other_image");
  image.add_attribution()->set_text("different attribution text");
  ntp::background::GetImagesInCollectionResponse image_response;
  *image_response.add_images() = image;
  std::string image_response_string;
  image_response.SerializeToString(&image_response_string);
  if (BackgroundImageErrorDetectionEnabled()) {
    SetUpResponseWithData(service()->GetImagesURLForTesting(),
                          image_response_string);
    SetUpResponseWithNetworkError(
        GURL(collection.preview(0).image_url() + GetThumbnailImageOptions()));
    SetUpResponseWithNetworkError(
        GURL(image.image_url() + GetThumbnailImageOptions()));
  }

  ASSERT_TRUE(service()->collection_info().empty());

  EXPECT_CALL(observer_, OnCollectionInfoAvailable).Times(1);
  service()->FetchCollectionInfo();
  base::RunLoop().RunUntilIdle();

  CollectionInfo collection_info;
  collection_info.collection_id = collection.collection_id();
  collection_info.collection_name = collection.collection_name();
  collection_info.preview_image_url =
      GURL(collection.preview(0).image_url() + GetThumbnailImageOptions());
  if (BackgroundImageErrorDetectionEnabled()) {
    EXPECT_TRUE(service()->collection_info().empty());
  } else {
    EXPECT_FALSE(service()->collection_info().empty());
    EXPECT_THAT(service()->collection_info().at(0), Eq(collection_info));
    EXPECT_EQ(service()->collection_error_info().error_type, ErrorType::NONE);
  }
}

TEST_P(NtpBackgroundServiceTest, BrokenCollectionPreviewImageHasReplacement) {
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

  // Set up for when BackgroundImageErrorDetectionEnabled is true.
  ntp::background::Image image;
  image.set_asset_id(12345);
  image.set_image_url("https://wallpapers.co/some_other_image");
  image.add_attribution()->set_text("different attribution text");
  ntp::background::GetImagesInCollectionResponse image_response;
  *image_response.add_images() = image;
  std::string image_response_string;
  image_response.SerializeToString(&image_response_string);
  if (BackgroundImageErrorDetectionEnabled()) {
    SetUpResponseWithData(service()->GetImagesURLForTesting(),
                          image_response_string);
    SetUpResponseWithNetworkError(
        GURL(collection.preview(0).image_url() + GetThumbnailImageOptions()));
    SetUpResponseWithNetworkSuccess(
        GURL(image.image_url() + GetThumbnailImageOptions()));
  }

  ASSERT_TRUE(service()->collection_info().empty());

  EXPECT_CALL(observer_, OnCollectionInfoAvailable).Times(1);
  service()->FetchCollectionInfo();
  base::RunLoop().RunUntilIdle();

  CollectionInfo collection_info;
  collection_info.collection_id = collection.collection_id();
  collection_info.collection_name = collection.collection_name();
  collection_info.preview_image_url =
      GURL(collection.preview(0).image_url() + GetThumbnailImageOptions());

  CollectionInfo updated_collection_info;
  updated_collection_info = collection_info;
  updated_collection_info.preview_image_url =
      GURL(image.image_url() + GetThumbnailImageOptions());

  EXPECT_FALSE(service()->collection_info().empty());
  EXPECT_EQ(service()->collection_error_info().error_type, ErrorType::NONE);

  if (BackgroundImageErrorDetectionEnabled()) {
    EXPECT_FALSE(service()->collection_info().empty());
    EXPECT_THAT(service()->collection_info().at(0),
                Eq(updated_collection_info));
  } else {
    EXPECT_FALSE(service()->collection_info().empty());
    EXPECT_THAT(service()->collection_info().at(0), Eq(collection_info));
  }
}

TEST_P(NtpBackgroundServiceTest, CollectionHasNoPreviewImage) {
  ntp::background::Collection shapes_collection;
  shapes_collection.set_collection_id("shapes");
  shapes_collection.set_collection_name("Shapes");
  shapes_collection.add_preview()->set_image_url(kTestImageUrl);

  ntp::background::Collection colors_collection;
  colors_collection.set_collection_id("colors");
  colors_collection.set_collection_name("Colors");

  ntp::background::GetCollectionsResponse response;
  *response.add_collections() = shapes_collection;
  *response.add_collections() = colors_collection;
  std::string response_string;
  response.SerializeToString(&response_string);

  SetUpResponseWithData(service()->GetCollectionsLoadURLForTesting(),
                        response_string);
  if (BackgroundImageErrorDetectionEnabled()) {
    SetUpResponseWithNetworkSuccess(GURL(
        shapes_collection.preview(0).image_url() + GetThumbnailImageOptions()));
  }

  ASSERT_TRUE(service()->collection_info().empty());

  EXPECT_CALL(observer_, OnCollectionInfoAvailable).Times(1);
  service()->FetchCollectionInfo();
  base::RunLoop().RunUntilIdle();

  CollectionInfo shapes_collection_info;
  shapes_collection_info.collection_id = shapes_collection.collection_id();
  shapes_collection_info.collection_name = shapes_collection.collection_name();
  shapes_collection_info.preview_image_url = GURL(
      shapes_collection.preview(0).image_url() + GetThumbnailImageOptions());

  CollectionInfo colors_collection_info;
  colors_collection_info.collection_id = colors_collection.collection_id();
  colors_collection_info.collection_name = colors_collection.collection_name();

  if (BackgroundImageErrorDetectionEnabled()) {
    EXPECT_EQ(1u, service()->collection_info().size());
    EXPECT_THAT(service()->collection_info().at(0), Eq(shapes_collection_info));
  } else {
    EXPECT_EQ(2u, service()->collection_info().size());
    EXPECT_THAT(service()->collection_info().at(0), Eq(shapes_collection_info));
    EXPECT_THAT(service()->collection_info().at(1), Eq(colors_collection_info));
  }
}

TEST_P(NtpBackgroundServiceTest, CollectionImagesNetworkError) {
  SetUpResponseWithNetworkError(service()->GetImagesURLForTesting());

  ASSERT_TRUE(service()->collection_images().empty());

  EXPECT_CALL(observer_, OnCollectionImagesAvailable).Times(1);
  service()->FetchCollectionImageInfo("shapes");
  base::RunLoop().RunUntilIdle();

  if (BackgroundImageErrorDetectionEnabled()) {
    histogram_tester_.ExpectTotalCount(
        "NewTabPage.BackgroundService.Images.Headers.ErrorDetected", 0);
  }
  EXPECT_TRUE(service()->collection_images().empty());
  EXPECT_EQ(service()->collection_images_error_info().error_type,
            ErrorType::NET_ERROR);
}

TEST_P(NtpBackgroundServiceTest, BadCollectionImagesResponse) {
  SetUpResponseWithData(service()->GetImagesURLForTesting(),
                        "bad serialized GetImagesInCollectionResponse");

  ASSERT_TRUE(service()->collection_images().empty());

  EXPECT_CALL(observer_, OnCollectionImagesAvailable).Times(1);
  service()->FetchCollectionImageInfo("shapes");
  base::RunLoop().RunUntilIdle();

  if (BackgroundImageErrorDetectionEnabled()) {
    histogram_tester_.ExpectTotalCount(
        "NewTabPage.BackgroundService.Images.Headers.ErrorDetected", 0);
  }
  EXPECT_TRUE(service()->collection_images().empty());
  EXPECT_EQ(service()->collection_images_error_info().error_type,
            ErrorType::SERVICE_ERROR);
}

TEST_P(NtpBackgroundServiceTest, ImageInCollectionHasNetworkError) {
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
  if (BackgroundImageErrorDetectionEnabled()) {
    SetUpResponseWithNetworkError(
        GURL(image.image_url() + GetThumbnailImageOptions()));
    histogram_tester_.ExpectTotalCount(
        "NewTabPage.BackgroundService.URLHeaders.RequestLatency", 0);
    histogram_tester_.ExpectTotalCount(
        "NewTabPage.BackgroundService.URLHeadersHttpResponseCode", 0);
  }

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

  if (BackgroundImageErrorDetectionEnabled()) {
    EXPECT_TRUE(service()->collection_images().empty());
    histogram_tester_.ExpectTotalCount(
        "NewTabPage.BackgroundService.Images.Headers.ErrorDetected", 1);
    ASSERT_EQ(1,
              histogram_tester_.GetBucketCount(
                  "NewTabPage.BackgroundService.Images.Headers.ErrorDetected",
                  NtpImageType::kCollectionImages));
    ASSERT_EQ(1, histogram_tester_.GetBucketCount(
                     "NewTabPage.BackgroundService.Images.Headers.StatusCode",
                     net::HTTP_NOT_FOUND));
  } else {
    EXPECT_FALSE(service()->collection_images().empty());
    EXPECT_THAT(service()->collection_images().at(0), Eq(collection_image));
    EXPECT_EQ(service()->collection_images_error_info().error_type,
              ErrorType::NONE);
  }
}

TEST_P(NtpBackgroundServiceTest, GoodCollectionImagesResponse) {
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
  if (BackgroundImageErrorDetectionEnabled()) {
    SetUpResponseWithNetworkSuccess(
        GURL(image.image_url() + GetThumbnailImageOptions()));
  }

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

  if (BackgroundImageErrorDetectionEnabled()) {
    histogram_tester_.ExpectTotalCount(
        "NewTabPage.BackgroundService.Images.Headers.ErrorDetected", 0);
  }
  EXPECT_FALSE(service()->collection_images().empty());
  EXPECT_THAT(service()->collection_images().at(0), Eq(collection_image));
  EXPECT_EQ(service()->collection_images_error_info().error_type,
            ErrorType::NONE);
}

TEST_P(NtpBackgroundServiceTest,
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
  if (BackgroundImageErrorDetectionEnabled()) {
    SetUpResponseWithNetworkSuccess(
        GURL(collection.preview(0).image_url() + GetThumbnailImageOptions()));
    SetUpResponseWithNetworkSuccess(
        GURL(image.image_url() + GetThumbnailImageOptions()));
  }

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

  if (BackgroundImageErrorDetectionEnabled()) {
    histogram_tester_.ExpectTotalCount(
        "NewTabPage.BackgroundService.Images.Headers.ErrorDetected", 0);
  }
  EXPECT_FALSE(service()->collection_info().empty());
  EXPECT_THAT(service()->collection_info().at(0), Eq(collection_info));
  EXPECT_FALSE(service()->collection_images().empty());
  EXPECT_THAT(service()->collection_images().at(0), Eq(collection_image));
}

TEST_P(NtpBackgroundServiceTest,
       CollectionImageInfoCanBeSuccessfullyFetchedMultipleTimes) {
  ntp::background::Image image;
  image.set_image_url(kTestImageUrl);
  ntp::background::GetImagesInCollectionResponse response;
  *response.add_images() = image;
  std::string response_string;
  response.SerializeToString(&response_string);

  SetUpResponseWithData(service()->GetImagesURLForTesting(), response_string);
  if (BackgroundImageErrorDetectionEnabled()) {
    SetUpResponseWithNetworkSuccess(
        GURL(image.image_url() + GetThumbnailImageOptions()));
  }

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

TEST_P(NtpBackgroundServiceTest, NextImageNetworkError) {
  SetUpResponseWithNetworkError(service()->GetNextImageURLForTesting());

  service()->FetchNextCollectionImage("shapes", absl::nullopt);
  base::RunLoop().RunUntilIdle();

  EXPECT_THAT(service()->next_image_error_info().error_type,
              Eq(ErrorType::NET_ERROR));
}

TEST_P(NtpBackgroundServiceTest, BadNextImageResponse) {
  SetUpResponseWithData(service()->GetNextImageURLForTesting(),
                        "bad serialized GetImageFromCollectionResponse");

  service()->FetchNextCollectionImage("shapes", absl::nullopt);
  base::RunLoop().RunUntilIdle();

  EXPECT_THAT(service()->next_image_error_info().error_type,
              Eq(ErrorType::SERVICE_ERROR));
}

TEST_P(NtpBackgroundServiceTest, GoodNextImageResponse) {
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

TEST_P(NtpBackgroundServiceTest, MultipleRequestsNextImage) {
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
  service()->FetchNextCollectionImage("shapes", absl::nullopt);
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

TEST_P(NtpBackgroundServiceTest, CheckValidAndInvalidBackdropUrls) {
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
  if (BackgroundImageErrorDetectionEnabled()) {
    SetUpResponseWithNetworkSuccess(
        GURL(image.image_url() + GetThumbnailImageOptions()));
  }

  ASSERT_TRUE(service()->collection_images().empty());

  EXPECT_CALL(observer_, OnCollectionImagesAvailable).Times(1);
  service()->FetchCollectionImageInfo("shapes");
  base::RunLoop().RunUntilIdle();

  if (BackgroundImageErrorDetectionEnabled()) {
    histogram_tester_.ExpectTotalCount(
        "NewTabPage.BackgroundService.Images.Headers.ErrorDetected", 0);
  }
  EXPECT_TRUE(service()->IsValidBackdropUrl(
      GURL(image.image_url() + service()->GetImageOptionsForTesting())));
  EXPECT_FALSE(service()->IsValidBackdropUrl(
      GURL("http://wallpapers.co/some_image=imageOptions")));
  EXPECT_FALSE(service()->IsValidBackdropUrl(
      GURL("https://wallpapers.co/another_image")));
}

TEST_P(NtpBackgroundServiceTest, GetThumbnailUrl) {
  const GURL kInvalidUrl("foo");
  const GURL kValidUrl("https://www.foo.com");
  const GURL kValidThumbnailUrl("https://www.foo.com/thumbnail");
  service()->AddValidBackdropUrlWithThumbnailForTesting(kValidUrl,
                                                        kValidThumbnailUrl);

  EXPECT_EQ(kValidThumbnailUrl, service()->GetThumbnailUrl(kValidUrl));
  EXPECT_EQ(GURL::EmptyGURL(), service()->GetThumbnailUrl(kInvalidUrl));
}

TEST_P(NtpBackgroundServiceTest, OverrideBaseUrl) {
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      "collections-base-url", "https://foo.com");
  service()->FetchCollectionInfo();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1u, test_url_loader_factory()->pending_requests()->size());
  EXPECT_EQ("https://foo.com/cast/chromecast/home/wallpaper/collections?rt=b",
            test_url_loader_factory()->pending_requests()->at(0).request.url);
}

TEST_P(NtpBackgroundServiceTest, VerifyURLMetricsWithNetworkSuccess) {
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

TEST_P(NtpBackgroundServiceTest, VerifyURLMetricsWithNetworkError) {
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
