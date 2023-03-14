// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/background/ntp_custom_background_service.h"

#include <vector>

#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/mock_callback.h"
#include "base/test/simple_test_clock.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/search/background/ntp_custom_background_service_observer.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/search/instant_types.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/testing_profile.h"
#include "components/search/ntp_features.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/image/image.h"
#include "url/gurl.h"

namespace {

class MockNtpCustomBackgroundServiceObserver
    : public NtpCustomBackgroundServiceObserver {
 public:
  MOCK_METHOD0(OnCustomBackgroundImageUpdated, void());
  MOCK_METHOD0(OnNtpCustomBackgroundServiceShuttingDown, void());
};

base::Value::Dict GetBackgroundInfoAsDict(const GURL& background_url,
                                          const GURL& thumbnail_url) {
  base::Value::Dict background_info;
  background_info.Set("background_url", base::Value(background_url.spec()));
  background_info.Set("thumbnail_url", base::Value(thumbnail_url.spec()));
  background_info.Set("attribution_line_1", base::Value(std::string()));
  background_info.Set("attribution_line_2", base::Value(std::string()));
  background_info.Set("attribution_action_url", base::Value(std::string()));
  background_info.Set("collection_id", base::Value(std::string()));
  background_info.Set("resume_token", base::Value(std::string()));
  background_info.Set("refresh_timestamp", base::Value(0));
  return background_info;
}

base::Time GetReferenceTime() {
  base::Time::Exploded exploded_reference_time;
  exploded_reference_time.year = 2019;
  exploded_reference_time.month = 1;
  exploded_reference_time.day_of_month = 1;
  exploded_reference_time.day_of_week = 1;
  exploded_reference_time.hour = 0;
  exploded_reference_time.minute = 0;
  exploded_reference_time.second = 0;
  exploded_reference_time.millisecond = 0;

  base::Time out_time;
  EXPECT_TRUE(
      base::Time::FromLocalExploded(exploded_reference_time, &out_time));
  return out_time;
}

}  // namespace

class NtpCustomBackgroundServiceTest : public testing::Test {
 public:
  void SetUp() override {
    custom_background_service_ =
        std::make_unique<NtpCustomBackgroundService>(&profile_);
    custom_background_service_->AddObserver(&observer_);
  }

 protected:
  // NOTE: The initialization order of these members matters.
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  base::SimpleTestClock clock_;
  MockNtpCustomBackgroundServiceObserver observer_;
  std::unique_ptr<NtpCustomBackgroundService> custom_background_service_;
};

TEST_F(NtpCustomBackgroundServiceTest, SetCustomBackgroundURL) {
  EXPECT_CALL(observer_, OnCustomBackgroundImageUpdated).Times(1);

  ASSERT_FALSE(custom_background_service_->IsCustomBackgroundSet());
  const GURL kUrl("https://www.foo.com");

  custom_background_service_->AddValidBackdropUrlForTesting(kUrl);
  custom_background_service_->SetCustomBackgroundInfo(kUrl, GURL(), "", "",
                                                      GURL(), "");

  auto custom_background = custom_background_service_->GetCustomBackground();
  EXPECT_EQ(kUrl, custom_background->custom_background_url);
  EXPECT_FALSE(custom_background->is_uploaded_image);
  EXPECT_FALSE(custom_background->daily_refresh_enabled);
  EXPECT_TRUE(custom_background_service_->IsCustomBackgroundSet());
}

TEST_F(NtpCustomBackgroundServiceTest, SetCustomBackgroundURLInvalidURL) {
  EXPECT_CALL(observer_, OnCustomBackgroundImageUpdated).Times(2);

  ASSERT_FALSE(custom_background_service_->IsCustomBackgroundSet());
  const GURL kInvalidUrl("foo");
  const GURL kValidUrl("https://www.foo.com");
  custom_background_service_->AddValidBackdropUrlForTesting(kValidUrl);
  custom_background_service_->SetCustomBackgroundInfo(kValidUrl, GURL(), "", "",
                                                      GURL(), "");

  auto custom_background = custom_background_service_->GetCustomBackground();
  EXPECT_EQ(kValidUrl.spec(), custom_background->custom_background_url.spec());

  custom_background_service_->SetCustomBackgroundInfo(kInvalidUrl, GURL(), "",
                                                      "", GURL(), "");

  custom_background = custom_background_service_->GetCustomBackground();
  EXPECT_FALSE(custom_background.has_value());
  EXPECT_FALSE(custom_background_service_->IsCustomBackgroundSet());
}

TEST_F(NtpCustomBackgroundServiceTest, SetCustomBackgroundInfo) {
  EXPECT_CALL(observer_, OnCustomBackgroundImageUpdated).Times(1);

  ASSERT_FALSE(custom_background_service_->IsCustomBackgroundSet());
  const GURL kUrl("https://www.foo.com");
  const std::string kAttributionLine1 = "foo";
  const std::string kAttributionLine2 = "bar";
  const GURL kActionUrl("https://www.bar.com");
  custom_background_service_->AddValidBackdropUrlForTesting(kUrl);
  custom_background_service_->SetCustomBackgroundInfo(
      kUrl, GURL(), kAttributionLine1, kAttributionLine2, kActionUrl, "");

  auto custom_background = custom_background_service_->GetCustomBackground();
  EXPECT_EQ(kUrl, custom_background->custom_background_url);
  EXPECT_EQ(false, custom_background->is_uploaded_image);
  EXPECT_EQ(kAttributionLine1,
            custom_background->custom_background_attribution_line_1);
  EXPECT_EQ(kAttributionLine2,
            custom_background->custom_background_attribution_line_2);
  EXPECT_EQ(kActionUrl,
            custom_background->custom_background_attribution_action_url);
  EXPECT_TRUE(custom_background_service_->IsCustomBackgroundSet());
}

TEST_F(NtpCustomBackgroundServiceTest, LocalBackgroundImageCopyCreated) {
  EXPECT_CALL(observer_, OnCustomBackgroundImageUpdated).Times(1);
  ASSERT_FALSE(custom_background_service_->IsCustomBackgroundSet());

  base::FilePath profile_path = profile_.GetPath();
  base::FilePath path(profile_path.AppendASCII("test_file"));
  base::FilePath copy_path(profile_path.AppendASCII(
      chrome::kChromeUIUntrustedNewTabPageBackgroundFilename));

  base::WriteFile(path, "background_image");

  custom_background_service_->SelectLocalBackgroundImage(path);

  task_environment_.RunUntilIdle();

  bool file_exists = base::PathExists(copy_path);

  EXPECT_EQ(true, file_exists);
  EXPECT_EQ(true, profile_.GetTestingPrefService()->GetBoolean(
                      prefs::kNtpCustomBackgroundLocalToDevice));
  EXPECT_TRUE(custom_background_service_->IsCustomBackgroundSet());
  auto custom_background = custom_background_service_->GetCustomBackground();
  EXPECT_EQ(true, custom_background->is_uploaded_image);
}

TEST_F(NtpCustomBackgroundServiceTest,
       SettingUrlRemovesLocalBackgroundImageCopy) {
  EXPECT_CALL(observer_, OnCustomBackgroundImageUpdated).Times(1);
  ASSERT_FALSE(custom_background_service_->IsCustomBackgroundSet());
  const GURL kUrl("https://www.foo.com");

  base::FilePath profile_path = profile_.GetPath();
  base::FilePath path(profile_path.AppendASCII(
      chrome::kChromeUIUntrustedNewTabPageBackgroundFilename));

  base::WriteFile(path, "background_image");

  custom_background_service_->AddValidBackdropUrlForTesting(kUrl);
  custom_background_service_->SetCustomBackgroundInfo(kUrl, GURL(), "", "",
                                                      GURL(), "");

  task_environment_.RunUntilIdle();

  bool file_exists = base::PathExists(path);

  EXPECT_EQ(false, file_exists);
  EXPECT_EQ(false, profile_.GetTestingPrefService()->GetBoolean(
                       prefs::kNtpCustomBackgroundLocalToDevice));
  ASSERT_TRUE(custom_background_service_->IsCustomBackgroundSet());
  auto custom_background = custom_background_service_->GetCustomBackground();
  EXPECT_EQ(false, custom_background->is_uploaded_image);
}

TEST_F(NtpCustomBackgroundServiceTest, UpdatingPrefUpdatesNtpTheme) {
  EXPECT_CALL(observer_, OnCustomBackgroundImageUpdated).Times(2);
  ASSERT_FALSE(custom_background_service_->IsCustomBackgroundSet());
  const GURL kUrlFoo("https://www.foo.com");
  const GURL kUrlBar("https://www.bar.com");

  sync_preferences::TestingPrefServiceSyncable* pref_service =
      profile_.GetTestingPrefService();
  pref_service->SetUserPref(prefs::kNtpCustomBackgroundDict,
                            GetBackgroundInfoAsDict(kUrlFoo, GURL()));

  auto custom_background = custom_background_service_->GetCustomBackground();
  EXPECT_EQ(kUrlFoo, custom_background->custom_background_url);
  EXPECT_TRUE(custom_background_service_->IsCustomBackgroundSet());

  pref_service->SetUserPref(prefs::kNtpCustomBackgroundDict,
                            GetBackgroundInfoAsDict(kUrlBar, GURL()));

  custom_background = custom_background_service_->GetCustomBackground();
  EXPECT_EQ(kUrlBar, custom_background->custom_background_url);
  EXPECT_EQ(false, custom_background->is_uploaded_image);
  EXPECT_EQ(false,
            pref_service->GetBoolean(prefs::kNtpCustomBackgroundLocalToDevice));
  EXPECT_TRUE(custom_background_service_->IsCustomBackgroundSet());
}

TEST_F(NtpCustomBackgroundServiceTest, SetLocalImage) {
  EXPECT_CALL(observer_, OnCustomBackgroundImageUpdated).Times(1);
  ASSERT_FALSE(custom_background_service_->IsCustomBackgroundSet());

  sync_preferences::TestingPrefServiceSyncable* pref_service =
      profile_.GetTestingPrefService();

  base::FilePath profile_path = profile_.GetPath();
  base::FilePath path(profile_path.AppendASCII(
      chrome::kChromeUIUntrustedNewTabPageBackgroundFilename));
  base::WriteFile(path, "background_image");
  base::ThreadPoolInstance::Get()->FlushForTesting();

  custom_background_service_->SelectLocalBackgroundImage(path);
  task_environment_.RunUntilIdle();

  auto custom_background = custom_background_service_->GetCustomBackground();
  EXPECT_TRUE(
      base::StartsWith(custom_background->custom_background_url.spec(),
                       chrome::kChromeUIUntrustedNewTabPageBackgroundUrl,
                       base::CompareCase::SENSITIVE));
  EXPECT_TRUE(
      pref_service->GetBoolean(prefs::kNtpCustomBackgroundLocalToDevice));
  EXPECT_TRUE(custom_background_service_->IsCustomBackgroundSet());
  EXPECT_EQ(true, custom_background->is_uploaded_image);
}

TEST_F(NtpCustomBackgroundServiceTest, SyncPrefOverridesAndRemovesLocalImage) {
  EXPECT_CALL(observer_, OnCustomBackgroundImageUpdated).Times(2);
  ASSERT_FALSE(custom_background_service_->IsCustomBackgroundSet());
  const GURL kUrl("https://www.foo.com/");

  sync_preferences::TestingPrefServiceSyncable* pref_service =
      profile_.GetTestingPrefService();

  base::FilePath profile_path = profile_.GetPath();
  base::FilePath path(profile_path.AppendASCII(
      chrome::kChromeUIUntrustedNewTabPageBackgroundFilename));
  base::WriteFile(path, "background_image");
  base::ThreadPoolInstance::Get()->FlushForTesting();

  custom_background_service_->SelectLocalBackgroundImage(path);
  task_environment_.RunUntilIdle();

  EXPECT_TRUE(
      pref_service->GetBoolean(prefs::kNtpCustomBackgroundLocalToDevice));
  EXPECT_TRUE(base::PathExists(path));

  // Update custom_background info via Sync.
  pref_service->SetUserPref(prefs::kNtpCustomBackgroundDict,
                            GetBackgroundInfoAsDict(kUrl, GURL()));
  task_environment_.RunUntilIdle();

  auto custom_background = custom_background_service_->GetCustomBackground();
  EXPECT_EQ(kUrl, custom_background->custom_background_url);
  EXPECT_EQ(false, custom_background->is_uploaded_image);
  EXPECT_FALSE(
      pref_service->GetBoolean(prefs::kNtpCustomBackgroundLocalToDevice));
  EXPECT_FALSE(base::PathExists(path));
  EXPECT_TRUE(custom_background_service_->IsCustomBackgroundSet());
}

TEST_F(NtpCustomBackgroundServiceTest, ValidateBackdropUrls) {
  EXPECT_CALL(observer_, OnCustomBackgroundImageUpdated).Times(4);
  ASSERT_FALSE(custom_background_service_->IsCustomBackgroundSet());
  const GURL kBackdropUrl1("https://www.foo.com");
  const GURL kBackdropUrl2("https://www.bar.com");
  const GURL kNonBackdropUrl1("https://www.test.com");
  const GURL kNonBackdropUrl2("https://www.foo.com/path");

  custom_background_service_->AddValidBackdropUrlForTesting(kBackdropUrl1);
  custom_background_service_->AddValidBackdropUrlForTesting(kBackdropUrl2);

  custom_background_service_->SetCustomBackgroundInfo(kBackdropUrl1, GURL(), "",
                                                      "", GURL(), "");
  auto custom_background = custom_background_service_->GetCustomBackground();
  EXPECT_EQ(kBackdropUrl1, custom_background->custom_background_url);
  EXPECT_TRUE(custom_background_service_->IsCustomBackgroundSet());

  custom_background_service_->SetCustomBackgroundInfo(kNonBackdropUrl1, GURL(),
                                                      "", "", GURL(), "");
  custom_background = custom_background_service_->GetCustomBackground();
  EXPECT_FALSE(custom_background.has_value());
  EXPECT_FALSE(custom_background_service_->IsCustomBackgroundSet());

  custom_background_service_->SetCustomBackgroundInfo(kBackdropUrl2, GURL(), "",
                                                      "", GURL(), "");
  custom_background = custom_background_service_->GetCustomBackground();
  EXPECT_EQ(kBackdropUrl2, custom_background->custom_background_url);
  EXPECT_TRUE(custom_background_service_->IsCustomBackgroundSet());

  custom_background_service_->SetCustomBackgroundInfo(kNonBackdropUrl2, GURL(),
                                                      "", "", GURL(), "");
  custom_background = custom_background_service_->GetCustomBackground();
  EXPECT_FALSE(custom_background.has_value());
  EXPECT_FALSE(custom_background_service_->IsCustomBackgroundSet());
}

TEST_F(NtpCustomBackgroundServiceTest, LocalImageDoesNotHaveAttribution) {
  EXPECT_CALL(observer_, OnCustomBackgroundImageUpdated).Times(2);
  ASSERT_FALSE(custom_background_service_->IsCustomBackgroundSet());
  const GURL kUrl("https://www.foo.com");
  const std::string kAttributionLine1 = "foo";
  const std::string kAttributionLine2 = "bar";
  const GURL kActionUrl("https://www.bar.com");

  sync_preferences::TestingPrefServiceSyncable* pref_service =
      profile_.GetTestingPrefService();
  custom_background_service_->AddValidBackdropUrlForTesting(kUrl);
  custom_background_service_->SetCustomBackgroundInfo(
      kUrl, GURL(), kAttributionLine1, kAttributionLine2, kActionUrl, "");

  auto custom_background = custom_background_service_->GetCustomBackground();
  ASSERT_EQ(kAttributionLine1,
            custom_background->custom_background_attribution_line_1);
  ASSERT_EQ(kAttributionLine2,
            custom_background->custom_background_attribution_line_2);
  ASSERT_EQ(kActionUrl,
            custom_background->custom_background_attribution_action_url);
  ASSERT_TRUE(custom_background_service_->IsCustomBackgroundSet());

  base::FilePath profile_path = profile_.GetPath();
  base::FilePath path(profile_path.AppendASCII(
      chrome::kChromeUIUntrustedNewTabPageBackgroundFilename));
  base::WriteFile(path, "background_image");
  base::ThreadPoolInstance::Get()->FlushForTesting();

  custom_background_service_->SelectLocalBackgroundImage(path);
  task_environment_.RunUntilIdle();

  custom_background = custom_background_service_->GetCustomBackground();
  EXPECT_TRUE(
      base::StartsWith(custom_background->custom_background_url.spec(),
                       chrome::kChromeUIUntrustedNewTabPageBackgroundUrl,
                       base::CompareCase::SENSITIVE));
  EXPECT_TRUE(
      pref_service->GetBoolean(prefs::kNtpCustomBackgroundLocalToDevice));
  EXPECT_TRUE(custom_background_service_->IsCustomBackgroundSet());
  EXPECT_EQ(true, custom_background->is_uploaded_image);
  EXPECT_EQ("", custom_background->custom_background_attribution_line_1);
  EXPECT_EQ("", custom_background->custom_background_attribution_line_2);
  EXPECT_EQ(GURL(),
            custom_background->custom_background_attribution_action_url);
}

TEST_F(NtpCustomBackgroundServiceTest, SetCustomBackgroundCollectionId) {
  EXPECT_CALL(observer_, OnCustomBackgroundImageUpdated).Times(2);
  ASSERT_FALSE(custom_background_service_->IsCustomBackgroundSet());
  const std::string kInvalidId("aarrtt");
  const std::string kValidId("art");

  // A valid id should update the pref/background.
  CollectionImage image;
  image.collection_id = kValidId;
  image.image_url = GURL("https://www.test.com/");
  custom_background_service_->SetNextCollectionImageForTesting(image);

  custom_background_service_->AddValidBackdropCollectionForTesting(kValidId);
  custom_background_service_->SetCustomBackgroundInfo(GURL(), GURL(), "", "",
                                                      GURL(), kValidId);
  task_environment_.RunUntilIdle();

  auto custom_background = custom_background_service_->GetCustomBackground();
  EXPECT_EQ(kValidId, custom_background->collection_id);
  EXPECT_TRUE(custom_background->daily_refresh_enabled);
  EXPECT_TRUE(custom_background_service_->IsCustomBackgroundSet());

  // An invalid id should clear the pref/background.
  CollectionImage image2;
  custom_background_service_->SetNextCollectionImageForTesting(image2);
  custom_background_service_->SetCustomBackgroundInfo(GURL(), GURL(), "", "",
                                                      GURL(), kInvalidId);
  task_environment_.RunUntilIdle();

  custom_background = custom_background_service_->GetCustomBackground();
  EXPECT_FALSE(custom_background.has_value());
  EXPECT_FALSE(custom_background_service_->IsCustomBackgroundSet());
}

TEST_F(NtpCustomBackgroundServiceTest, RefreshesBackgroundAfter24Hours) {
  EXPECT_CALL(observer_, OnCustomBackgroundImageUpdated).Times(2);
  ASSERT_FALSE(custom_background_service_->IsCustomBackgroundSet());
  const std::string kValidId("art");
  const GURL kImageUrl1("https://www.test.com/1/");
  const GURL kImageUrl2("https://www.test.com/2/");

  custom_background_service_->SetClockForTesting(&clock_);
  clock_.SetNow(GetReferenceTime());

  // A valid id should update the pref/background.
  CollectionImage image;
  image.collection_id = kValidId;
  image.image_url = kImageUrl1;
  custom_background_service_->SetNextCollectionImageForTesting(image);

  custom_background_service_->AddValidBackdropCollectionForTesting(kValidId);
  custom_background_service_->SetCustomBackgroundInfo(GURL(), GURL(), "", "",
                                                      GURL(), kValidId);
  task_environment_.RunUntilIdle();

  auto custom_background = custom_background_service_->GetCustomBackground();
  EXPECT_EQ(kValidId, custom_background->collection_id);
  EXPECT_TRUE(custom_background_service_->IsCustomBackgroundSet());

  CollectionImage image2;
  image2.collection_id = kValidId;
  image2.image_url = kImageUrl2;
  custom_background_service_->SetNextCollectionImageForTesting(image2);

  // Should not refresh background.
  custom_background_service_->RefreshBackgroundIfNeeded();
  task_environment_.RunUntilIdle();
  custom_background = custom_background_service_->GetCustomBackground();
  EXPECT_EQ(kValidId, custom_background->collection_id);
  EXPECT_EQ(kImageUrl1, custom_background->custom_background_url);
  EXPECT_TRUE(custom_background_service_->IsCustomBackgroundSet());

  clock_.Advance(base::Hours(25));

  // Should refresh background after >24 hours.
  custom_background_service_->RefreshBackgroundIfNeeded();
  task_environment_.RunUntilIdle();
  custom_background = custom_background_service_->GetCustomBackground();
  EXPECT_EQ(kValidId, custom_background->collection_id);
  EXPECT_EQ(kImageUrl2, custom_background->custom_background_url);
  EXPECT_TRUE(custom_background->daily_refresh_enabled);
  EXPECT_TRUE(custom_background_service_->IsCustomBackgroundSet());
}

TEST_F(NtpCustomBackgroundServiceTest, RevertBackgroundChanges) {
  EXPECT_CALL(observer_, OnCustomBackgroundImageUpdated).Times(2);
  ASSERT_FALSE(custom_background_service_->IsCustomBackgroundSet());
  const GURL kBackdropUrl1("https://www.foo.com");

  custom_background_service_->AddValidBackdropUrlForTesting(kBackdropUrl1);

  custom_background_service_->SetCustomBackgroundInfo(kBackdropUrl1, GURL(), "",
                                                      "", GURL(), "");
  auto custom_background = custom_background_service_->GetCustomBackground();
  EXPECT_EQ(kBackdropUrl1, custom_background->custom_background_url);
  EXPECT_TRUE(custom_background_service_->IsCustomBackgroundSet());

  // Revert from background set using |kBackdropUrl1| to the starting state (no
  // background) since no background change was confirmed.
  custom_background_service_->RevertBackgroundChanges();
  custom_background = custom_background_service_->GetCustomBackground();
  EXPECT_FALSE(custom_background_service_->IsCustomBackgroundSet());
}

TEST_F(NtpCustomBackgroundServiceTest,
       RevertBackgroundChangesWithMultipleSelections) {
  EXPECT_CALL(observer_, OnCustomBackgroundImageUpdated).Times(3);
  ASSERT_FALSE(custom_background_service_->IsCustomBackgroundSet());
  const GURL kBackdropUrl1("https://www.foo.com");
  const GURL kBackdropUrl2("https://www.bar.com");

  custom_background_service_->AddValidBackdropUrlForTesting(kBackdropUrl1);
  custom_background_service_->AddValidBackdropUrlForTesting(kBackdropUrl2);

  custom_background_service_->SetCustomBackgroundInfo(kBackdropUrl1, GURL(), "",
                                                      "", GURL(), "");
  auto custom_background = custom_background_service_->GetCustomBackground();
  EXPECT_EQ(kBackdropUrl1, custom_background->custom_background_url);
  EXPECT_TRUE(custom_background_service_->IsCustomBackgroundSet());

  custom_background_service_->SetCustomBackgroundInfo(kBackdropUrl2, GURL(), "",
                                                      "", GURL(), "");

  // Revert from background set using |kBackdropUrl2| to the starting state (no
  // background) since no background change was confirmed.
  custom_background_service_->RevertBackgroundChanges();
  custom_background = custom_background_service_->GetCustomBackground();
  EXPECT_FALSE(custom_background_service_->IsCustomBackgroundSet());
}

TEST_F(NtpCustomBackgroundServiceTest, ConfirmBackgroundChanges) {
  EXPECT_CALL(observer_, OnCustomBackgroundImageUpdated).Times(3);
  ASSERT_FALSE(custom_background_service_->IsCustomBackgroundSet());
  const GURL kBackdropUrl1("https://www.foo.com");
  const GURL kBackdropUrl2("https://www.bar.com");

  custom_background_service_->AddValidBackdropUrlForTesting(kBackdropUrl1);
  custom_background_service_->AddValidBackdropUrlForTesting(kBackdropUrl2);

  custom_background_service_->SetCustomBackgroundInfo(kBackdropUrl1, GURL(), "",
                                                      "", GURL(), "");
  auto custom_background = custom_background_service_->GetCustomBackground();
  EXPECT_EQ(kBackdropUrl1, custom_background->custom_background_url);
  EXPECT_TRUE(custom_background_service_->IsCustomBackgroundSet());

  custom_background_service_->ConfirmBackgroundChanges();

  custom_background_service_->SetCustomBackgroundInfo(kBackdropUrl2, GURL(), "",
                                                      "", GURL(), "");
  custom_background = custom_background_service_->GetCustomBackground();
  EXPECT_EQ(kBackdropUrl2, custom_background->custom_background_url);
  EXPECT_TRUE(custom_background_service_->IsCustomBackgroundSet());

  // Revert from background set using |kBackdropUrl2| to the starting state
  // (background set using |kBackdropUrl1|) since it is the last confirmed
  // background change.
  custom_background_service_->RevertBackgroundChanges();
  custom_background = custom_background_service_->GetCustomBackground();
  EXPECT_EQ(kBackdropUrl1, custom_background->custom_background_url);
  EXPECT_TRUE(custom_background_service_->IsCustomBackgroundSet());
}

TEST_F(NtpCustomBackgroundServiceTest, TestUpdateCustomBackgroundColor) {
  // Turn on Color Extraction feature.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      ntp_features::kCustomizeChromeColorExtraction);

  EXPECT_CALL(observer_, OnCustomBackgroundImageUpdated).Times(2);
  SkBitmap bitmap;
  bitmap.allocN32Pixels(32, 32);
  bitmap.eraseColor(SK_ColorRED);
  gfx::Image image = gfx::Image::CreateFrom1xBitmap(bitmap);

  ASSERT_FALSE(custom_background_service_->IsCustomBackgroundSet());

  // Background color will not update if no background is set.
  custom_background_service_->UpdateCustomBackgroundColorAsync(
      GURL(), image, image_fetcher::RequestMetadata());
  task_environment_.RunUntilIdle();
  auto custom_background = custom_background_service_->GetCustomBackground();
  auto custom_background_main_color =
      custom_background ? custom_background->custom_background_main_color
                        : SK_ColorWHITE;
  EXPECT_NE(SK_ColorRED, custom_background_main_color.value_or(SK_ColorWHITE));

  const GURL kUrl("https://www.foo.com");
  const GURL kThumbnailUrl("https://www.thumbnail.com");
  const std::string kAttributionLine1 = "foo";
  const std::string kAttributionLine2 = "bar";
  const GURL kActionUrl("https://www.bar.com");

  custom_background_service_->AddValidBackdropUrlForTesting(kUrl);
  custom_background_service_->AddValidBackdropUrlForTesting(kThumbnailUrl);
  custom_background_service_->SetCustomBackgroundInfo(
      kUrl, kThumbnailUrl, kAttributionLine1, kAttributionLine2, kActionUrl,
      "");

  image_fetcher::RequestMetadata metadata = image_fetcher::RequestMetadata();

  // Background color will not update if metadata http code invalid.
  metadata.http_response_code =
      image_fetcher::RequestMetadata::ResponseCode::RESPONSE_CODE_INVALID;
  custom_background_service_->UpdateCustomBackgroundColorAsync(kUrl, image,
                                                               metadata);
  task_environment_.RunUntilIdle();
  custom_background = custom_background_service_->GetCustomBackground();
  EXPECT_NE(
      SK_ColorRED,
      custom_background->custom_background_main_color.value_or(SK_ColorWHITE));

  // Background color will not update if current background url changed.
  metadata.http_response_code = 200;
  custom_background_service_->UpdateCustomBackgroundColorAsync(
      GURL("different_url"), image, metadata);
  task_environment_.RunUntilIdle();
  custom_background = custom_background_service_->GetCustomBackground();
  EXPECT_NE(
      SK_ColorRED,
      custom_background->custom_background_main_color.value_or(SK_ColorWHITE));

  // Background color should update.
  custom_background_service_->UpdateCustomBackgroundColorAsync(kUrl, image,
                                                               metadata);
  task_environment_.RunUntilIdle();
  custom_background = custom_background_service_->GetCustomBackground();
  EXPECT_EQ(
      SK_ColorRED,
      custom_background->custom_background_main_color.value_or(SK_ColorWHITE));
}
