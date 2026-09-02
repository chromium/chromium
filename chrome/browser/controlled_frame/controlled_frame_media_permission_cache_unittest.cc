// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/controlled_frame/controlled_frame_media_permission_cache.h"

#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace controlled_frame {

class ControlledFrameMediaPermissionCacheTest : public testing::Test {
 protected:
  void SetUp() override {
    profile_ = std::make_unique<TestingProfile>();
    cache_ =
        std::make_unique<ControlledFrameMediaPermissionCache>(profile_.get());
  }

  void TearDown() override { cache_->Shutdown(); }

  TestingProfile* profile() { return profile_.get(); }
  ControlledFrameMediaPermissionCache* cache() { return cache_.get(); }

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<ControlledFrameMediaPermissionCache> cache_;
};

TEST_F(ControlledFrameMediaPermissionCacheTest, AddAndCheckPermission) {
  url::Origin embedder =
      url::Origin::Create(GURL("https://embedder.example.com"));
  url::Origin requesting =
      url::Origin::Create(GURL("https://requesting.example.com"));
  url::Origin other = url::Origin::Create(GURL("https://other.example.com"));

  EXPECT_FALSE(cache()->HasPermission(embedder, requesting));

  cache()->AddPermission(embedder, requesting);

  EXPECT_TRUE(cache()->HasPermission(embedder, requesting));
  EXPECT_FALSE(cache()->HasPermission(embedder, other));
  EXPECT_FALSE(cache()->HasPermission(other, requesting));
}

TEST_F(ControlledFrameMediaPermissionCacheTest, ClearsOnCameraSettingChanged) {
  url::Origin embedder =
      url::Origin::Create(GURL("https://embedder.example.com"));
  url::Origin requesting =
      url::Origin::Create(GURL("https://requesting.example.com"));

  cache()->AddPermission(embedder, requesting);
  EXPECT_TRUE(cache()->HasPermission(embedder, requesting));

  auto* hcsm = HostContentSettingsMapFactory::GetForProfile(profile());
  hcsm->SetContentSettingDefaultScope(embedder.GetURL(), embedder.GetURL(),
                                      ContentSettingsType::MEDIASTREAM_CAMERA,
                                      CONTENT_SETTING_BLOCK);

  // The cache should be cleared.
  EXPECT_FALSE(cache()->HasPermission(embedder, requesting));
}

TEST_F(ControlledFrameMediaPermissionCacheTest, ClearsOnMicSettingChanged) {
  url::Origin embedder =
      url::Origin::Create(GURL("https://embedder.example.com"));
  url::Origin requesting =
      url::Origin::Create(GURL("https://requesting.example.com"));

  cache()->AddPermission(embedder, requesting);
  EXPECT_TRUE(cache()->HasPermission(embedder, requesting));

  auto* hcsm = HostContentSettingsMapFactory::GetForProfile(profile());
  hcsm->SetContentSettingDefaultScope(embedder.GetURL(), embedder.GetURL(),
                                      ContentSettingsType::MEDIASTREAM_MIC,
                                      CONTENT_SETTING_BLOCK);

  // The cache should be cleared.
  EXPECT_FALSE(cache()->HasPermission(embedder, requesting));
}

TEST_F(ControlledFrameMediaPermissionCacheTest,
       DoesNotClearOnUnrelatedSettingChanged) {
  url::Origin embedder =
      url::Origin::Create(GURL("https://embedder.example.com"));
  url::Origin requesting =
      url::Origin::Create(GURL("https://requesting.example.com"));

  cache()->AddPermission(embedder, requesting);
  EXPECT_TRUE(cache()->HasPermission(embedder, requesting));

  auto* hcsm = HostContentSettingsMapFactory::GetForProfile(profile());
  hcsm->SetContentSettingDefaultScope(embedder.GetURL(), embedder.GetURL(),
                                      ContentSettingsType::COOKIES,
                                      CONTENT_SETTING_BLOCK);

  // The cache should NOT be cleared.
  EXPECT_TRUE(cache()->HasPermission(embedder, requesting));
}

}  // namespace controlled_frame
