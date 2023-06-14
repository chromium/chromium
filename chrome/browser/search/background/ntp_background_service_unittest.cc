// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/background/ntp_background_service.h"

#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/search/background/ntp_background_data.h"
#include "components/search/ntp_features.h"
#include "components/version_info/version_info.h"
#include "content/public/test/browser_task_environment.h"
#include "services/network/public/cpp/data_element.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using testing::Eq;
using testing::StartsWith;

const char kTestImageUrl[] = "https://wallpapers.co/some_image";
const char kTestActionUrl[] = "https://wallpapers.co/some_image/learn_more";

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

  ~NtpBackgroundServiceTest() override {}

  void SetUpResponseWithNetworkSuccess(const GURL& load_url,
                                       const std::string& response = "") {
    test_url_loader_factory_.AddResponse(load_url.spec(), response);
  }

  void SetUpResponseWithData(const GURL& load_url,
                             const std::string& response) {
    test_url_loader_factory_.SetInterceptor(base::BindLambdaForTesting(
        [&](const network::ResourceRequest& request) {}));
    SetUpResponseWithNetworkSuccess(load_url, response);
  }

  void SetUpResponseWithNetworkError(const GURL& load_url) {
    test_url_loader_factory_.AddResponse(
        load_url, network::mojom::URLResponseHead::New(), std::string(),
        network::URLLoaderCompletionStatus(net::HTTP_NOT_FOUND));
  }

  NtpBackgroundService* service() {
    if (!service_) {
      service_ =
          std::make_unique<NtpBackgroundService>(test_shared_loader_factory_);
    }
    return service_.get();
  }

  network::TestURLLoaderFactory* test_url_loader_factory() {
    return &test_url_loader_factory_;
  }

  bool BackgroundImageErrorDetectionEnabled() const { return GetParam(); }

 private:
  // Required to run tests from UI and threads.
  content::BrowserTaskEnvironment task_environment_;

  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;

  std::unique_ptr<NtpBackgroundService> service_;

  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All, NtpBackgroundServiceTest, ::testing::Bool());

TEST_P(NtpBackgroundServiceTest, CorrectCollectionRequest) {
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
  EXPECT_EQ(3, collection_request.filtering_label_size());
  EXPECT_EQ("chrome_desktop_ntp", collection_request.filtering_label(0));
  EXPECT_EQ("chrome_desktop_ntp.M" + version_info::GetMajorVersionNumber(),
            collection_request.filtering_label(1));
  EXPECT_EQ("chrome_desktop_ntp.panorama",
            collection_request.filtering_label(2));
}

TEST_P(NtpBackgroundServiceTest, CollectionInfoNetworkError) {
  SetUpResponseWithNetworkError(service()->GetCollectionsLoadURLForTesting());

  ASSERT_TRUE(service()->collection_info().empty());

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

  ASSERT_TRUE(service()->collection_info().empty());

  service()->FetchCollectionInfo();
  base::RunLoop().RunUntilIdle();

  CollectionInfo collection_info;
  collection_info.collection_id = collection.collection_id();
  collection_info.collection_name = collection.collection_name();
  collection_info.preview_image_url = GURL(
      collection.preview(0).image_url() + GetThumbnailImageOptionsForTesting());

  EXPECT_FALSE(service()->collection_info().empty());
  EXPECT_THAT(service()->collection_info().at(0), Eq(collection_info));
  EXPECT_EQ(service()->collection_error_info().error_type, ErrorType::NONE);
}

TEST_P(NtpBackgroundServiceTest, CollectionImagesNetworkError) {
  SetUpResponseWithNetworkError(service()->GetImagesURLForTesting());

  ASSERT_TRUE(service()->collection_images().empty());

  service()->FetchCollectionImageInfo("shapes");
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(service()->collection_images().empty());
  EXPECT_EQ(service()->collection_images_error_info().error_type,
            ErrorType::NET_ERROR);
}

TEST_P(NtpBackgroundServiceTest, BadCollectionImagesResponse) {
  SetUpResponseWithData(service()->GetImagesURLForTesting(),
                        "bad serialized GetImagesInCollectionResponse");

  ASSERT_TRUE(service()->collection_images().empty());

  service()->FetchCollectionImageInfo("shapes");
  base::RunLoop().RunUntilIdle();

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
    SetUpResponseWithNetworkError(GURL(image.image_url()));
  }

  ASSERT_TRUE(service()->collection_images().empty());

  service()->FetchCollectionImageInfo("shapes");
  base::RunLoop().RunUntilIdle();

  CollectionImage collection_image;
  collection_image.collection_id = "shapes";
  collection_image.asset_id = image.asset_id();
  collection_image.thumbnail_image_url =
      GURL(image.image_url() + GetThumbnailImageOptionsForTesting());
  collection_image.image_url =
      GURL(image.image_url() + service()->GetImageOptionsForTesting());
  collection_image.attribution.push_back(image.attribution(0).text());
  collection_image.attribution_action_url = GURL(image.action_url());

  if (BackgroundImageErrorDetectionEnabled()) {
    EXPECT_TRUE(service()->collection_images().empty());
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
    SetUpResponseWithNetworkSuccess(GURL(image.image_url()));
  }

  ASSERT_TRUE(service()->collection_images().empty());

  service()->FetchCollectionImageInfo("shapes");
  base::RunLoop().RunUntilIdle();

  CollectionImage collection_image;
  collection_image.collection_id = "shapes";
  collection_image.asset_id = image.asset_id();
  collection_image.thumbnail_image_url =
      GURL(image.image_url() + GetThumbnailImageOptionsForTesting());
  collection_image.image_url =
      GURL(image.image_url() + service()->GetImageOptionsForTesting());
  collection_image.attribution.push_back(image.attribution(0).text());
  collection_image.attribution_action_url = GURL(image.action_url());

  EXPECT_FALSE(service()->collection_images().empty());
  EXPECT_THAT(service()->collection_images().at(0), Eq(collection_image));
  EXPECT_EQ(service()->collection_images_error_info().error_type,
            ErrorType::NONE);
}

TEST_P(NtpBackgroundServiceTest, MultipleRequests) {
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
    SetUpResponseWithNetworkSuccess(GURL(image.image_url()));
  }

  ASSERT_TRUE(service()->collection_info().empty());
  ASSERT_TRUE(service()->collection_images().empty());

  service()->FetchCollectionInfo();
  service()->FetchCollectionImageInfo("shapes");
  // Subsequent requests are ignored while the loader is in use.
  service()->FetchCollectionImageInfo("colors");
  base::RunLoop().RunUntilIdle();

  CollectionInfo collection_info;
  collection_info.collection_id = collection.collection_id();
  collection_info.collection_name = collection.collection_name();
  collection_info.preview_image_url = GURL(
      collection.preview(0).image_url() + GetThumbnailImageOptionsForTesting());

  CollectionImage collection_image;
  collection_image.collection_id = "shapes";
  collection_image.asset_id = image.asset_id();
  collection_image.thumbnail_image_url =
      GURL(image.image_url() + GetThumbnailImageOptionsForTesting());
  collection_image.image_url =
      GURL(image.image_url() + service()->GetImageOptionsForTesting());
  collection_image.attribution.push_back(image.attribution(0).text());

  EXPECT_FALSE(service()->collection_info().empty());
  EXPECT_THAT(service()->collection_info().at(0), Eq(collection_info));
  EXPECT_FALSE(service()->collection_images().empty());
  EXPECT_THAT(service()->collection_images().at(0), Eq(collection_image));
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
      GURL(image.image_url() + GetThumbnailImageOptionsForTesting());
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
  service()->FetchNextCollectionImage("shapes", absl::nullopt);
  // Subsequent requests are ignored while the loader is in use.
  service()->FetchNextCollectionImage("shapes", "resume0");
  base::RunLoop().RunUntilIdle();

  CollectionImage collection_image;
  collection_image.collection_id = "shapes";
  collection_image.asset_id = image.asset_id();
  collection_image.thumbnail_image_url =
      GURL(image.image_url() + GetThumbnailImageOptionsForTesting());
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
    SetUpResponseWithNetworkSuccess(GURL(image.image_url()));
  }

  ASSERT_TRUE(service()->collection_images().empty());

  service()->FetchCollectionImageInfo("shapes");
  base::RunLoop().RunUntilIdle();

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
