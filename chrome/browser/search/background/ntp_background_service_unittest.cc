// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/background/ntp_background_service.h"

#include <utility>
#include <vector>

#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_task_environment.h"
#include "chrome/browser/search/background/ntp_background_data.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "services/identity/public/cpp/identity_test_environment.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using testing::Eq;
using testing::StartsWith;

namespace {

// The options to be added to end of an image URL, specifying resolution, etc.
constexpr char kImageOptions[] = "=imageOptions";

}  // namespace

class NtpBackgroundServiceTest : public testing::Test {
 public:
  NtpBackgroundServiceTest()
      : thread_bundle_(content::TestBrowserThreadBundle::IO_MAINLOOP),
        test_shared_loader_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_)) {
    identity_env_.MakePrimaryAccountAvailable("example@gmail.com");
    identity_env_.SetAutomaticIssueOfAccessTokens(true);
  }

  ~NtpBackgroundServiceTest() override {}

  void SetUp() override {
    testing::Test::SetUp();

    service_ = std::make_unique<NtpBackgroundService>(
        identity_env_.identity_manager(), test_shared_loader_factory_,
        base::nullopt, base::nullopt, base::nullopt, base::nullopt,
        kImageOptions);
  }

  void SetUpResponseWithData(const GURL& load_url,
                             const std::string& response) {
    test_url_loader_factory_.SetInterceptor(base::BindLambdaForTesting(
        [&](const network::ResourceRequest& request) {}));
    test_url_loader_factory_.AddResponse(load_url.spec(), response);
  }

  // This can be used to revoke a token issued by
  // SetAutomaticIssueOfAccessTokens above.
  void RemoveRefreshTokenForPrimaryAccount() {
    identity_env_.RemoveRefreshTokenForPrimaryAccount();
  }

  void SetUpResponseWithNetworkError(const GURL& load_url) {
    test_url_loader_factory_.AddResponse(
        load_url, network::ResourceResponseHead(), std::string(),
        network::URLLoaderCompletionStatus(net::HTTP_NOT_FOUND));
  }

  NtpBackgroundService* service() { return service_.get(); }

 private:
  // Required to run tests from UI and threads.
  content::TestBrowserThreadBundle thread_bundle_;

  identity::IdentityTestEnvironment identity_env_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;

  std::unique_ptr<NtpBackgroundService> service_;
};

TEST_F(NtpBackgroundServiceTest, CollectionInfoNetworkError) {
  SetUpResponseWithNetworkError(service()->GetCollectionsLoadURLForTesting());

  ASSERT_TRUE(service()->collection_info().empty());

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
  collection.add_preview()->set_image_url("https://wallpapers.co/some_image");
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

TEST_F(NtpBackgroundServiceTest, CollectionImagesNetworkError) {
  SetUpResponseWithNetworkError(service()->GetImagesURLForTesting());

  ASSERT_TRUE(service()->collection_images().empty());

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

  service()->FetchCollectionImageInfo("shapes");
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(service()->collection_images().empty());
  EXPECT_EQ(service()->collection_images_error_info().error_type,
            ErrorType::SERVICE_ERROR);
}

TEST_F(NtpBackgroundServiceTest, GoodCollectionImagesResponse) {
  ntp::background::Image image;
  image.set_asset_id(12345);
  image.set_image_url("https://wallpapers.co/some_image");
  image.add_attribution()->set_text("attribution text");
  image.set_action_url("https://wallpapers.co/some_image/learn_more");
  ntp::background::GetImagesInCollectionResponse response;
  *response.add_images() = image;
  std::string response_string;
  response.SerializeToString(&response_string);

  SetUpResponseWithData(service()->GetImagesURLForTesting(), response_string);

  ASSERT_TRUE(service()->collection_images().empty());

  service()->FetchCollectionImageInfo("shapes");
  base::RunLoop().RunUntilIdle();

  CollectionImage collection_image;
  collection_image.collection_id = "shapes";
  collection_image.asset_id = image.asset_id();
  collection_image.thumbnail_image_url =
      GURL(image.image_url() + GetThumbnailImageOptionsForTesting());
  collection_image.image_url = GURL(image.image_url() + kImageOptions);
  collection_image.attribution.push_back(image.attribution(0).text());
  collection_image.attribution_action_url = GURL(image.action_url());

  EXPECT_FALSE(service()->collection_images().empty());
  EXPECT_THAT(service()->collection_images().at(0), Eq(collection_image));
  EXPECT_EQ(service()->collection_images_error_info().error_type,
            ErrorType::NONE);
}

TEST_F(NtpBackgroundServiceTest, MultipleRequests) {
  ntp::background::Collection collection;
  collection.set_collection_id("shapes");
  collection.set_collection_name("Shapes");
  collection.add_preview()->set_image_url("https://wallpapers.co/some_image");
  ntp::background::GetCollectionsResponse collection_response;
  *collection_response.add_collections() = collection;
  std::string collection_response_string;
  collection_response.SerializeToString(&collection_response_string);

  ntp::background::Image image;
  image.set_asset_id(12345);
  image.set_image_url("https://wallpapers.co/some_image");
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
  collection_image.image_url = GURL(image.image_url() + kImageOptions);
  collection_image.attribution.push_back(image.attribution(0).text());

  EXPECT_FALSE(service()->collection_info().empty());
  EXPECT_THAT(service()->collection_info().at(0), Eq(collection_info));
  EXPECT_FALSE(service()->collection_images().empty());
  EXPECT_THAT(service()->collection_images().at(0), Eq(collection_image));
}

TEST_F(NtpBackgroundServiceTest, AlbumInfoNetworkError) {
  SetUpResponseWithNetworkError(service()->GetAlbumsURLForTesting());

  ASSERT_TRUE(service()->album_info().empty());

  service()->FetchAlbumInfo();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(service()->album_info().empty());
  EXPECT_EQ(service()->album_error_info().error_type, ErrorType::NET_ERROR);
}

TEST_F(NtpBackgroundServiceTest, AlbumInfoAuthError) {
  RemoveRefreshTokenForPrimaryAccount();

  ASSERT_TRUE(service()->album_info().empty());

  service()->FetchAlbumInfo();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(service()->album_info().empty());
}

TEST_F(NtpBackgroundServiceTest, AlbumInfoAuthErrorClearsCache) {
  ntp::background::AlbumMetaData album;
  album.set_album_id(12345);
  album.set_album_name("Travel");
  album.set_banner_image_url("https://wallpapers.co/some_image");
  album.set_photo_container_id("AnIdentifierForThePhotoContainer");
  ntp::background::PersonalAlbumsResponse response;
  *response.add_album_meta_data() = album;
  std::string response_string;
  response.SerializeToString(&response_string);

  SetUpResponseWithData(service()->GetAlbumsURLForTesting(), response_string);

  ASSERT_TRUE(service()->album_info().empty());

  service()->FetchAlbumInfo();
  base::RunLoop().RunUntilIdle();

  RemoveRefreshTokenForPrimaryAccount();

  // Stale data fetched with previous token.
  ASSERT_FALSE(service()->album_info().empty());

  service()->FetchAlbumInfo();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(service()->album_info().empty());
}

TEST_F(NtpBackgroundServiceTest, BadAlbumsResponse) {
  SetUpResponseWithData(service()->GetAlbumsURLForTesting(),
                        "bad serialized PersonalAlbumsResponse");

  ASSERT_TRUE(service()->album_info().empty());

  service()->FetchAlbumInfo();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(service()->album_info().empty());
  EXPECT_EQ(service()->album_error_info().error_type, ErrorType::SERVICE_ERROR);
}

TEST_F(NtpBackgroundServiceTest, GoodAlbumsResponse) {
  ntp::background::AlbumMetaData album;
  album.set_album_id(12345);
  album.set_album_name("Travel");
  album.set_banner_image_url("https://wallpapers.co/some_image");
  album.set_photo_container_id("AnIdentifierForThePhotoContainer");
  ntp::background::PersonalAlbumsResponse response;
  *response.add_album_meta_data() = album;
  std::string response_string;
  response.SerializeToString(&response_string);

  SetUpResponseWithData(service()->GetAlbumsURLForTesting(), response_string);

  ASSERT_TRUE(service()->album_info().empty());

  service()->FetchAlbumInfo();
  base::RunLoop().RunUntilIdle();

  AlbumInfo album_info;
  album_info.album_id = album.album_id();
  album_info.photo_container_id = album.photo_container_id();
  album_info.album_name = album.album_name();
  album_info.preview_image_url = GURL(album.banner_image_url());

  EXPECT_FALSE(service()->album_info().empty());
  EXPECT_THAT(service()->album_info().at(0), Eq(album_info));
  EXPECT_EQ(service()->album_error_info().error_type, ErrorType::NONE);
}

TEST_F(NtpBackgroundServiceTest, AlbumPhotosNetworkError) {
  SetUpResponseWithNetworkError(service()->GetAlbumPhotosApiUrlForTesting(
      "album_id", "photo_container_id"));

  ASSERT_TRUE(service()->album_photos().empty());

  service()->FetchAlbumPhotos("album_id", "photo_container_id");
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(service()->album_photos().empty());
  EXPECT_EQ(service()->album_photos_error_info().error_type,
            ErrorType::NET_ERROR);
}

TEST_F(NtpBackgroundServiceTest, AlbumPhotosAuthError) {
  RemoveRefreshTokenForPrimaryAccount();

  ASSERT_TRUE(service()->album_info().empty());

  service()->FetchAlbumPhotos("album_id", "photo_container_id");
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(service()->album_info().empty());
}

TEST_F(NtpBackgroundServiceTest, AlbumPhotosAuthErrorClearsCache) {
  ntp::background::SettingPreviewResponse::Preview preview;
  preview.set_preview_url("https://wallpapers.co/some_image");
  ntp::background::SettingPreviewResponse response;
  *response.add_preview() = preview;
  std::string response_string;
  response.SerializeToString(&response_string);

  SetUpResponseWithData(service()->GetAlbumPhotosApiUrlForTesting(
                            "album_id", "photo_container_id"),
                        response_string);

  ASSERT_TRUE(service()->album_photos().empty());

  service()->FetchAlbumPhotos("album_id", "photo_container_id");
  base::RunLoop().RunUntilIdle();

  RemoveRefreshTokenForPrimaryAccount();

  // Stale data fetched with previous token.
  ASSERT_FALSE(service()->album_photos().empty());

  service()->FetchAlbumPhotos("album_id", "photo_container_id");
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(service()->album_photos().empty());
}

TEST_F(NtpBackgroundServiceTest, BadAlbumPhotosResponse) {
  SetUpResponseWithData(service()->GetAlbumPhotosApiUrlForTesting(
                            "album_id", "photo_container_id"),
                        "bad serialized SettingPreviewResponse");

  ASSERT_TRUE(service()->album_photos().empty());

  service()->FetchAlbumPhotos("album_id", "photo_container_id");
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(service()->album_photos().empty());
  EXPECT_EQ(service()->album_photos_error_info().error_type,
            ErrorType::SERVICE_ERROR);
}

TEST_F(NtpBackgroundServiceTest, AlbumPhotoErrorResponse) {
  ntp::background::SettingPreviewResponse response;
  response.set_status(ntp::background::ErrorCode::SERVER_ERROR);
  response.set_error_msg("server error");
  std::string response_string;
  response.SerializeToString(&response_string);

  SetUpResponseWithData(service()->GetAlbumPhotosApiUrlForTesting(
                            "album_id", "photo_container_id"),
                        response_string);

  ASSERT_TRUE(service()->album_photos().empty());

  service()->FetchAlbumPhotos("album_id", "photo_container_id");
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(service()->album_photos().empty());
  EXPECT_EQ(service()->album_photos_error_info().error_type,
            ErrorType::SERVICE_ERROR);
}

TEST_F(NtpBackgroundServiceTest, GoodAlbumPhotosResponse) {
  ntp::background::SettingPreviewResponse::Preview preview;
  preview.set_preview_url("https://wallpapers.co/some_image");
  ntp::background::SettingPreviewResponse response;
  *response.add_preview() = preview;
  std::string response_string;
  response.SerializeToString(&response_string);

  SetUpResponseWithData(service()->GetAlbumPhotosApiUrlForTesting(
                            "album_id", "photo_container_id"),
                        response_string);

  ASSERT_TRUE(service()->album_photos().empty());

  service()->FetchAlbumPhotos("album_id", "photo_container_id");
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(service()->album_photos().empty());
  EXPECT_THAT(service()->album_photos().at(0).thumbnail_photo_url.spec(),
              StartsWith(preview.preview_url()));
  EXPECT_THAT(service()->album_photos().at(0).photo_url.spec(),
              StartsWith(preview.preview_url()));
  EXPECT_EQ(service()->album_photos_error_info().error_type, ErrorType::NONE);
}

TEST_F(NtpBackgroundServiceTest, CheckValidAndInvalidBackdropUrls) {
  ntp::background::Image image;
  image.set_asset_id(12345);
  image.set_image_url("https://wallpapers.co/some_image");
  image.add_attribution()->set_text("attribution text");
  image.set_action_url("https://wallpapers.co/some_image/learn_more");
  ntp::background::GetImagesInCollectionResponse response;
  *response.add_images() = image;
  std::string response_string;
  response.SerializeToString(&response_string);

  SetUpResponseWithData(service()->GetImagesURLForTesting(), response_string);

  ASSERT_TRUE(service()->collection_images().empty());

  service()->FetchCollectionImageInfo("shapes");
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(service()->IsValidBackdropUrl(
      GURL("https://wallpapers.co/some_image=imageOptions")));

  EXPECT_FALSE(service()->IsValidBackdropUrl(
      GURL("http://wallpapers.co/some_image=imageOptions")));
  EXPECT_FALSE(service()->IsValidBackdropUrl(
      GURL("https://wallpapers.co/another_image")));
}
