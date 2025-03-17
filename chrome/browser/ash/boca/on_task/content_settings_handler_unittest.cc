// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/boca_ui/provider/content_settings_handler.h"

#include "ash/webui/boca_ui/mojom/boca.mojom.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::boca {
namespace {

struct PermissionSettings {
  mojom::PermissionSetting input;
  ContentSetting expected_output;
  std::string description;
};

constexpr char kTestUrl[] = "https://www.test.com";

class ContentSettingsHandlerTest
    : public ::testing::TestWithParam<PermissionSettings> {
 protected:
  TestingProfile* profile() { return &profile_; }

  ContentSettingsHandler* content_settings_handler() {
    return &content_settings_handler_;
  }
  HostContentSettingsMap* settings_map() {
    return HostContentSettingsMapFactory::GetForProfile(profile());
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  ContentSettingsHandler content_settings_handler_{&profile_};
};

TEST_P(ContentSettingsHandlerTest, SetSiteMicrophonePermission) {
  PermissionSettings params = GetParam();

  bool success = content_settings_handler()->SetContentSettingForOrigin(
      kTestUrl, mojom::Permission::kMicrophone, params.input);
  ASSERT_TRUE(success);
  EXPECT_EQ(
      settings_map()->GetContentSetting(GURL(kTestUrl), GURL(kTestUrl),
                                        ContentSettingsType::MEDIASTREAM_MIC),
      params.expected_output);
}

TEST_P(ContentSettingsHandlerTest, SetSiteCameraPermission) {
  PermissionSettings params = GetParam();

  bool success = content_settings_handler()->SetContentSettingForOrigin(
      kTestUrl, mojom::Permission::kCamera, params.input);
  ASSERT_TRUE(success);
  EXPECT_EQ(settings_map()->GetContentSetting(
                GURL(kTestUrl), GURL(kTestUrl),
                ContentSettingsType::MEDIASTREAM_CAMERA),
            params.expected_output);
}

INSTANTIATE_TEST_SUITE_P(
    PermissionSettings,
    ContentSettingsHandlerTest,
    testing::Values(
        PermissionSettings{mojom::PermissionSetting::kAllow,
                           ContentSetting::CONTENT_SETTING_ALLOW, "Allow"},
        PermissionSettings{mojom::PermissionSetting::kAsk,
                           ContentSetting::CONTENT_SETTING_ASK, "Ask"},
        PermissionSettings{mojom::PermissionSetting::kBlock,
                           ContentSetting::CONTENT_SETTING_BLOCK, "Block"}),
    [](const testing::TestParamInfo<PermissionSettings>& info) {
      return info.param.description;
    });

}  // namespace
}  // namespace ash::boca
