// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/wallpaper_handlers/wallpaper_handlers.h"

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "ash/webui/personalization_app/mojom/personalization_app.mojom.h"
#include "base/json/json_reader.h"
#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/settings/scoped_cros_settings_test_helper.h"
#include "chrome/browser/ash/wallpaper_handlers/wallpaper_fetcher_delegate.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/icu/source/i18n/unicode/timezone.h"

namespace wallpaper_handlers {

namespace {

constexpr char kFakeTestEmail[] = "fakeemail@personalization";

constexpr char kGooglePhotosResumeTokenOnlyResponse[] =
    "{\"resumeToken\": \"token\"}";
constexpr char kGooglePhotosResumeToken[] = "token";

constexpr char kGooglePhotosPhotosFullResponse[] =
    "{"
    "   \"item\": [ {"
    "      \"itemId\": {"
    "         \"mediaKey\": \"photoId\""
    "      },"
    "      \"dedupKey\": \"dedupKey\","
    "      \"filename\": \"photoName.png\","
    "      \"creationTimestamp\": \"2021-12-31T07:07:07.000Z\","
    "      \"photo\": {"
    "         \"servingUrl\": \"https://www.google.com/\""
    "      },"
    "      \"locationFeature\": {"
    "         \"name\": [ {"
    "            \"text\": \"home\""
    "         } ]"
    "      }"
    "   } ],"
    "   \"resumeToken\": \"token\""
    "}";

constexpr char kGooglePhotosPhotosSingleItemResponse[] =
    "{"
    "   \"item\": {"
    "      \"itemId\": {"
    "         \"mediaKey\": \"photoId\""
    "      },"
    "      \"dedupKey\": \"dedupKey\","
    "      \"filename\": \"photoName.png\","
    "      \"creationTimestamp\": \"2021-12-31T07:07:07.000Z\","
    "      \"photo\": {"
    "         \"servingUrl\": \"https://www.google.com/\""
    "      },"
    "      \"locationFeature\": {"
    "         \"name\": [ {"
    "            \"text\": \"home\""
    "         } ]"
    "      }"
    "   }"
    "}";

constexpr char kGooglePhotosAlbumsFullResponse[] =
    "{"
    "   \"collection\": [ {"
    "      \"collectionId\": {"
    "         \"mediaKey\": \"albumId\""
    "      },"
    "      \"coverItemServingUrl\": \"https://www.google.com/\","
    "      \"name\": \"title\","
    "      \"numPhotos\": \"1\","
    "      \"latestModificationTimestamp\": \"2021-12-31T07:07:07.000Z\""
    "   } ],"
    "   \"resumeToken\": \"token\""
    "}";

// Parses `json` as a value dictionary. A test calling this function will fail
// if `json` is not appropriately formatted.
base::Value::Dict JsonToDict(std::string_view json) {
  std::optional<base::Value> parsed_json = base::JSONReader::Read(json);
  EXPECT_TRUE(parsed_json.has_value() && parsed_json->is_dict());
  return std::move(*parsed_json).TakeDict();
}

// Returns a non-null pointer to the photo in a hypothetical Google Photos
// photos query response. A test calling this function will fail if the response
// does not contain exactly one photo.
base::Value::Dict* GetPhotoFromGooglePhotosPhotosResponse(
    base::Value::Dict* response) {
  EXPECT_TRUE(response);
  auto* photos = response->FindList("item");
  EXPECT_TRUE(photos && photos->size() == 1);
  auto* photo = photos->front().GetIfDict();
  EXPECT_TRUE(photo);
  return photo;
}

// Returns a non-null pointer to the album in a hypothetical Google Photos
// albums query response. A test calling this function will fail if the response
// does not contain exactly one album.
base::Value::Dict* GetAlbumFromGooglePhotosAlbumsResponse(
    base::Value::Dict* response) {
  EXPECT_TRUE(response);
  auto* albums = response->FindList("collection");
  EXPECT_TRUE(albums && albums->size() == 1);
  auto* album = albums->front().GetIfDict();
  EXPECT_TRUE(album);
  return album;
}

using ash::personalization_app::mojom::FetchGooglePhotosAlbumsResponse;
using ash::personalization_app::mojom::FetchGooglePhotosPhotosResponse;
using ash::personalization_app::mojom::GooglePhotosAlbum;
using ash::personalization_app::mojom::GooglePhotosAlbumPtr;
using ash::personalization_app::mojom::GooglePhotosEnablementState;
using ash::personalization_app::mojom::GooglePhotosPhoto;
using ash::personalization_app::mojom::GooglePhotosPhotoPtr;

}  // namespace

class GooglePhotosFetcherTestBase : public testing::Test {
 public:
  GooglePhotosFetcherTestBase()
      : scoped_user_manager_(std::make_unique<ash::FakeChromeUserManager>()),
        profile_manager_(TestingBrowserProcess::GetGlobal()) {}

  GooglePhotosFetcherTestBase(const GooglePhotosFetcherTestBase&) = delete;
  GooglePhotosFetcherTestBase& operator=(const GooglePhotosFetcherTestBase&) =
      delete;
  ~GooglePhotosFetcherTestBase() override = default;

  TestingProfile* profile() { return profile_; }

 protected:
  // testing::Test:
  void SetUp() override {
    ASSERT_TRUE(profile_manager_.SetUp());
    profile_ = profile_manager_.CreateTestingProfile(kFakeTestEmail,
                                                     /*is_main_profile=*/true);
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  user_manager::ScopedUserManager scoped_user_manager_;
  TestingProfileManager profile_manager_;
  raw_ptr<TestingProfile> profile_;
};

class GooglePhotosEnabledFetcherTest : public GooglePhotosFetcherTestBase {
 public:
  GooglePhotosEnablementState ParseResponse(base::Value::Dict* response) {
    return google_photos_enabled_fetcher_->ParseResponse(response);
  }

  std::optional<size_t> GetResultCount(
      const GooglePhotosEnablementState& result) {
    return google_photos_enabled_fetcher_->GetResultCount(result);
  }

 protected:
  void SetUp() override {
    GooglePhotosFetcherTestBase::SetUp();
    google_photos_enabled_fetcher_ =
        std::make_unique<WallpaperFetcherDelegateImpl>()
            ->CreateGooglePhotosEnabledFetcher(profile());
  }

 private:
  std::unique_ptr<GooglePhotosEnabledFetcher> google_photos_enabled_fetcher_;
};

TEST_F(GooglePhotosEnabledFetcherTest, ParseGooglePhotosEnabled) {
  // Parse an absent response (simulating a fetching error).
  auto result = GooglePhotosEnablementState::kError;
  EXPECT_EQ(ParseResponse(nullptr), result);
  EXPECT_EQ(GetResultCount(result), std::nullopt);

  // Parse a response without an enabled state.
  base::Value::Dict response;
  EXPECT_EQ(ParseResponse(&response), result);
  EXPECT_EQ(GetResultCount(result), std::nullopt);

  // Parse a response with an unknown enabled state.
  response.SetByDottedPath("status.userState", "UNKNOWN_STATUS_STATE");
  EXPECT_EQ(ParseResponse(&response), result);
  EXPECT_EQ(GetResultCount(result), std::nullopt);

  // Parse a response indicating that the user cannot access Google Photos data.
  response.SetByDottedPath("status.userState", "USER_DASHER_DISABLED");
  result = GooglePhotosEnablementState::kDisabled;
  EXPECT_EQ(ParseResponse(&response), result);
  EXPECT_EQ(GetResultCount(result), std::make_optional<size_t>(1u));

  // Parse a response indicating that the user can access Google Photos data.
  response.SetByDottedPath("status.userState", "USER_PERMITTED");
  result = GooglePhotosEnablementState::kEnabled;
  EXPECT_EQ(ParseResponse(&response), result);
  EXPECT_EQ(GetResultCount(result), std::make_optional<size_t>(1u));
}

class GooglePhotosPhotosFetcherTest : public GooglePhotosFetcherTestBase {
 public:
  GooglePhotosPhotosCbkArgs ParseResponse(const base::Value::Dict* response) {
    return google_photos_photos_fetcher_->ParseResponse(response);
  }

  std::optional<size_t> GetResultCount(
      const GooglePhotosPhotosCbkArgs& result) {
    return google_photos_photos_fetcher_->GetResultCount(result);
  }

 protected:
  void SetUp() override {
    GooglePhotosFetcherTestBase::SetUp();
    google_photos_photos_fetcher_ =
        std::make_unique<WallpaperFetcherDelegateImpl>()
            ->CreateGooglePhotosPhotosFetcher(profile());
  }

 private:
  std::unique_ptr<GooglePhotosPhotosFetcher> google_photos_photos_fetcher_;
};

TEST_F(GooglePhotosPhotosFetcherTest, ParseGooglePhotosPhotosAbsentPhoto) {
  // Parse an absent response (simulating a fetching error).
  auto result = FetchGooglePhotosPhotosResponse::New();
  EXPECT_EQ(ParseResponse(nullptr), result);
  EXPECT_EQ(GetResultCount(result), std::nullopt);

  // Parse a response with no resume token or photos.
  base::Value::Dict empty_response;
  EXPECT_EQ(ParseResponse(&empty_response), result);
  EXPECT_EQ(GetResultCount(result), std::nullopt);

  // Parse a response with a resume token and no photos.
  auto response = JsonToDict(kGooglePhotosResumeTokenOnlyResponse);
  result = FetchGooglePhotosPhotosResponse::New(std::nullopt,
                                                kGooglePhotosResumeToken);
  EXPECT_EQ(ParseResponse(&response), result);
  EXPECT_EQ(GetResultCount(result), std::optional<size_t>());
}

TEST_F(GooglePhotosPhotosFetcherTest, ParsePhotosInvalidPhoto) {
  auto result = FetchGooglePhotosPhotosResponse::New(
      std::vector<GooglePhotosPhotoPtr>(), kGooglePhotosResumeToken);

  // Parse one-photo responses where one of the photo's fields is missing.
  for (const auto* const path : {"itemId.mediaKey", "filename",
                                 "creationTimestamp", "photo.servingUrl"}) {
    auto response = JsonToDict(kGooglePhotosPhotosFullResponse);
    auto* photo = GetPhotoFromGooglePhotosPhotosResponse(&response);
    photo->RemoveByDottedPath(path);
    EXPECT_EQ(ParseResponse(&response), result);
    EXPECT_EQ(GetResultCount(result), std::make_optional<size_t>(0u));
  }

  // Parse one-photo responses where one of the photo's fields has an invalid
  // value.
  std::vector<std::pair<std::string, std::string>> invalid_field_test_cases = {
      {"creationTimestamp", ""},
      {"creationTimestamp", "Bad timestamp"},
      {"creationTimestamp", "2021T07:07:07.000Z"},
      {"creationTimestamp", "31T07:07:07.000Z"},
      {"creationTimestamp", "12T07:07:07.000Z"},
      {"creationTimestamp", "12-31T07:07:07.000Z"},
      {"creationTimestamp", "2021-31T07:07:07.000Z"},
      {"creationTimestamp", "2021-12T07:07:07.000Z"},
      {"creationTimestamp", "-2021-12-31T07:07:07.000Z"}};
  for (const auto& kv : invalid_field_test_cases) {
    auto response = JsonToDict(kGooglePhotosPhotosFullResponse);
    auto* photo = GetPhotoFromGooglePhotosPhotosResponse(&response);
    photo->SetByDottedPath(kv.first, kv.second);
    EXPECT_EQ(ParseResponse(&response), result);
    EXPECT_EQ(GetResultCount(result), std::make_optional<size_t>(0u));
  }
}

TEST_F(GooglePhotosPhotosFetcherTest, ParsePhotosValidPhoto) {
  // Ensure that photo timestamps resolve to the same date on all testing
  // platforms.
  icu::TimeZone::adoptDefault(icu::TimeZone::createTimeZone("UTC"));

  // Parse a response with a valid photo and a resume token.
  auto valid_photos_vector = std::vector<GooglePhotosPhotoPtr>();
  valid_photos_vector.push_back(GooglePhotosPhoto::New(
      "photoId", "dedupKey", "photoName", u"Friday, December 31, 2021",
      GURL("https://www.google.com/"), "home"));
  auto response = JsonToDict(kGooglePhotosPhotosFullResponse);
  auto result = FetchGooglePhotosPhotosResponse::New(
      mojo::Clone(valid_photos_vector), kGooglePhotosResumeToken);
  EXPECT_EQ(ParseResponse(&response), result);
  EXPECT_EQ(GetResultCount(result),
            std::make_optional<size_t>(valid_photos_vector.size()));

  // Parse a response with a valid photo and no resume token.
  response.Remove("resumeToken");
  result = FetchGooglePhotosPhotosResponse::New(
      mojo::Clone(valid_photos_vector), std::nullopt);
  EXPECT_EQ(ParseResponse(&response), result);
  EXPECT_EQ(GetResultCount(result),
            std::make_optional<size_t>(valid_photos_vector.size()));

  // Parse a response with a single valid photo not in a list.
  response = JsonToDict(kGooglePhotosPhotosSingleItemResponse);
  EXPECT_EQ(ParseResponse(&response), result);
  EXPECT_EQ(GetResultCount(result),
            std::make_optional<size_t>(valid_photos_vector.size()));

  // Parse a response with a valid photo and no dedup key.
  auto valid_photos_vector_without_dedup_key =
      std::vector<GooglePhotosPhotoPtr>();
  valid_photos_vector_without_dedup_key.push_back(GooglePhotosPhoto::New(
      "photoId", std::nullopt, "photoName", u"Friday, December 31, 2021",
      GURL("https://www.google.com/"), "home"));
  response.RemoveByDottedPath("item.dedupKey");
  result = FetchGooglePhotosPhotosResponse::New(
      mojo::Clone(valid_photos_vector_without_dedup_key), std::nullopt);
  EXPECT_EQ(result, ParseResponse(&response));
  EXPECT_EQ(
      GetResultCount(result),
      std::make_optional<size_t>(valid_photos_vector_without_dedup_key.size()));

  // Parse a response with a valid photo and no location.
  auto valid_photos_vector_without_location =
      std::vector<GooglePhotosPhotoPtr>();
  valid_photos_vector_without_location.push_back(GooglePhotosPhoto::New(
      "photoId", std::nullopt, "photoName", u"Friday, December 31, 2021",
      GURL("https://www.google.com/"), std::nullopt));
  auto* name_list = response.FindListByDottedPath("item.locationFeature.name");
  EXPECT_FALSE(name_list->empty());
  name_list->clear();
  result = FetchGooglePhotosPhotosResponse::New(
      mojo::Clone(valid_photos_vector_without_location), std::nullopt);
  EXPECT_EQ(result, ParseResponse(&response));
  EXPECT_EQ(
      GetResultCount(result),
      std::make_optional<size_t>(valid_photos_vector_without_location.size()));
}

class GooglePhotosAlbumsFetcherTest : public GooglePhotosFetcherTestBase {
 public:
  GooglePhotosAlbumsCbkArgs ParseResponse(const base::Value::Dict* response) {
    return google_photos_albums_fetcher_->ParseResponse(response);
  }

  std::optional<size_t> GetResultCount(
      const GooglePhotosAlbumsCbkArgs& result) {
    return google_photos_albums_fetcher_->GetResultCount(result);
  }

 protected:
  void SetUp() override {
    GooglePhotosFetcherTestBase::SetUp();
    google_photos_albums_fetcher_ =
        std::make_unique<WallpaperFetcherDelegateImpl>()
            ->CreateGooglePhotosAlbumsFetcher(profile());
  }

 private:
  std::unique_ptr<GooglePhotosAlbumsFetcher> google_photos_albums_fetcher_;
};

TEST_F(GooglePhotosAlbumsFetcherTest, ParseAlbumsAbsentAlbum) {
  // Parse an absent response (simulating a fetching error).
  auto result = FetchGooglePhotosAlbumsResponse::New();
  EXPECT_EQ(ParseResponse(nullptr), result);
  EXPECT_EQ(GetResultCount(result), std::nullopt);

  // Parse a response with no resume token or albums.
  base::Value::Dict empty_response;
  EXPECT_EQ(ParseResponse(&empty_response), result);

  // Parse a response with a resume token and no albums.
  auto response = JsonToDict(kGooglePhotosResumeTokenOnlyResponse);
  result = FetchGooglePhotosAlbumsResponse::New(std::nullopt,
                                                kGooglePhotosResumeToken);
  EXPECT_EQ(ParseResponse(&response), result);
  EXPECT_EQ(GetResultCount(result), std::nullopt);
}

TEST_F(GooglePhotosAlbumsFetcherTest, ParseAlbumsInvalidAlbum) {
  auto result = FetchGooglePhotosAlbumsResponse::New(
      std::vector<GooglePhotosAlbumPtr>(), kGooglePhotosResumeToken);

  // Parse one-album responses where one of the album's fields is missing.
  for (const auto* const path :
       {"collectionId.mediaKey", "name", "numPhotos", "coverItemServingUrl"}) {
    auto response = JsonToDict(kGooglePhotosAlbumsFullResponse);
    auto* album = GetAlbumFromGooglePhotosAlbumsResponse(&response);
    album->RemoveByDottedPath(path);
    EXPECT_EQ(ParseResponse(&response), result);
    EXPECT_EQ(GetResultCount(result), std::make_optional<size_t>(0u));
  }

  // Parse one-album responses where one of the album's fields has an invalid
  // value.
  std::vector<std::pair<std::string, std::string>> invalid_field_test_cases = {
      {"numPhotos", ""},
      {"numPhotos", "NaN"},
      {"numPhotos", "-1"},
      {"numPhotos", "0"}};
  for (const auto& kv : invalid_field_test_cases) {
    auto response = JsonToDict(kGooglePhotosAlbumsFullResponse);
    auto* album = GetAlbumFromGooglePhotosAlbumsResponse(&response);
    album->SetByDottedPath(kv.first, kv.second);
    EXPECT_EQ(ParseResponse(&response), result);
    EXPECT_EQ(GetResultCount(result), std::make_optional<size_t>(0u));
  }
}

TEST_F(GooglePhotosAlbumsFetcherTest, ParseAlbumsValidAlbum) {
  // Parse a response with a valid album and a resume token.
  auto response = JsonToDict(kGooglePhotosAlbumsFullResponse);
  auto valid_albums_vector = std::vector<GooglePhotosAlbumPtr>();
  base::Time timestamp;
  EXPECT_TRUE(
      base::Time::FromUTCString("2021-12-31T07:07:07.000Z", &timestamp));
  valid_albums_vector.push_back(GooglePhotosAlbum::New(
      "albumId", "title", 1, GURL("https://www.google.com/"), timestamp,
      /*is_shared=*/false));
  auto result = FetchGooglePhotosAlbumsResponse::New(
      mojo::Clone(valid_albums_vector), kGooglePhotosResumeToken);
  EXPECT_EQ(ParseResponse(&response), result);
  EXPECT_EQ(GetResultCount(result),
            std::make_optional<size_t>(valid_albums_vector.size()));

  // Parse a response with a valid album and no resume token.
  response.Remove("resumeToken");
  result = FetchGooglePhotosAlbumsResponse::New(
      mojo::Clone(valid_albums_vector), std::nullopt);
  EXPECT_EQ(ParseResponse(&response), result);
  EXPECT_EQ(GetResultCount(result),
            std::make_optional<size_t>(valid_albums_vector.size()));
}

}  // namespace wallpaper_handlers
