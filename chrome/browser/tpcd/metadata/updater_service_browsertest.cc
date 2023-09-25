// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tpcd/metadata/updater_service.h"

#include <string>
#include <vector>

#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tpcd/metadata/updater_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/tpcd/metadata/parser_test_helper.h"
#include "content/public/test/browser_test.h"
#include "net/base/features.h"

namespace tpcd::metadata {
namespace {
const base::FilePath::CharType kComponentFileName[] =
    FILE_PATH_LITERAL("metadata.pb");
}  // namespace

class UpdaterServiceBrowserTest : public PlatformBrowserTest {
 public:
  ~UpdaterServiceBrowserTest() override = default;

  UpdaterServiceBrowserTest() {
    CHECK(fake_install_dir_.CreateUniqueTempDir());
    CHECK(fake_install_dir_.IsValid());
    scoped_feature_list_.InitAndEnableFeature(
        net::features::kTpcdMetadataGrants);
  }

  UpdaterService* updater_service() {
    return UpdaterServiceFactory::GetForProfile(browser()->profile());
  }

  Parser* parser() { return tpcd::metadata::Parser::GetInstance(); }

  void MockComponentInstallation(Metadata metadata) {
    base::FilePath path =
        fake_install_dir_.GetPath().Append(kComponentFileName);
    CHECK(base::WriteFile(path, metadata.SerializeAsString()));

    CHECK(base::PathExists(path));
    std::string raw_metadata;
    CHECK(base::ReadFileToString(path, &raw_metadata));

    parser()->ParseMetadata(raw_metadata);
  }

  PrefService* GetPrefs(Profile* profile = nullptr) {
    return (profile ? profile : browser()->profile())->GetPrefs();
  }

  content_settings::CookieSettings* GetCookieSettings(
      Profile* profile = nullptr) {
    return CookieSettingsFactory::GetForProfile(profile ? profile
                                                        : browser()->profile())
        .get();
  }

 private:
  base::ScopedTempDir fake_install_dir_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(UpdaterServiceBrowserTest,
                       ContentSettingsForOneType_SuccessfullyUpdated) {
  base::ScopedAllowBlockingForTesting allow_blocking;

  const GURL kEmbedded = GURL("http://www.bar.com");
  const GURL kEmbedder = GURL("http://www.foo.com");

  ASSERT_EQ(GetCookieSettings()->GetTpcdMetadataGrantsForTesting().size(), 0u);
  EXPECT_EQ(
      ContentSetting::CONTENT_SETTING_BLOCK,
      GetCookieSettings()->GetContentSettingForTesting(
          kEmbedded, kEmbedder, ContentSettingsType::TPCD_METADATA_GRANTS));

  const std::string primary_pattern_spec = "[*.]bar.com";
  const std::string secondary_pattern_spec = "[*.]foo.com";

  std::vector<MetadataPair> metadata_pairs;
  metadata_pairs.emplace_back(primary_pattern_spec, secondary_pattern_spec);
  Metadata metadata = MakeMetadataProtoFromVectorOfPair(metadata_pairs);
  ASSERT_EQ(metadata.metadata_entries_size(), 1);

  MockComponentInstallation(metadata);

  ASSERT_EQ(GetCookieSettings()->GetTpcdMetadataGrantsForTesting().size(), 1u);
  EXPECT_EQ(
      ContentSetting::CONTENT_SETTING_ALLOW,
      GetCookieSettings()->GetContentSettingForTesting(
          kEmbedded, kEmbedder, ContentSettingsType::TPCD_METADATA_GRANTS));
}

IN_PROC_BROWSER_TEST_F(UpdaterServiceBrowserTest,
                       ContentSettingsForOneType_SuccessfullyCleared) {
  base::ScopedAllowBlockingForTesting allow_blocking;

  const GURL kEmbedded1 = GURL("http://www.bar.com");
  const GURL kEmbedder1 = GURL("http://www.foo.com");
  const GURL kEmbedded2 = GURL("http://www.baz.com");
  const GURL kEmbedder2 = GURL("http://www.daz.com");

  {
    const std::string primary_pattern_spec = "[*.]bar.com";
    const std::string secondary_pattern_spec = "[*.]foo.com";

    std::vector<MetadataPair> metadata_pairs;
    metadata_pairs.emplace_back(primary_pattern_spec, secondary_pattern_spec);
    Metadata metadata = MakeMetadataProtoFromVectorOfPair(metadata_pairs);
    ASSERT_EQ(metadata.metadata_entries_size(), 1);

    ASSERT_EQ(GetCookieSettings()->GetTpcdMetadataGrantsForTesting().size(),
              0u);
    EXPECT_EQ(

        GetCookieSettings()->GetContentSettingForTesting(
            kEmbedded1, kEmbedder1, ContentSettingsType::TPCD_METADATA_GRANTS),
        ContentSetting::CONTENT_SETTING_BLOCK);

    MockComponentInstallation(metadata);

    ASSERT_EQ(GetCookieSettings()->GetTpcdMetadataGrantsForTesting().size(),
              1u);
    EXPECT_EQ(
        GetCookieSettings()->GetContentSettingForTesting(
            kEmbedded1, kEmbedder1, ContentSettingsType::TPCD_METADATA_GRANTS),
        ContentSetting::CONTENT_SETTING_ALLOW);
  }

  {
    const std::string primary_pattern_spec = "[*.]baz.com";
    const std::string secondary_pattern_spec = "[*.]daz.com";

    std::vector<MetadataPair> metadata_pairs;
    metadata_pairs.emplace_back(primary_pattern_spec, secondary_pattern_spec);
    Metadata metadata = MakeMetadataProtoFromVectorOfPair(metadata_pairs);
    ASSERT_EQ(metadata.metadata_entries_size(), 1);

    ASSERT_EQ(GetCookieSettings()->GetTpcdMetadataGrantsForTesting().size(),
              1u);
    EXPECT_EQ(
        GetCookieSettings()->GetContentSettingForTesting(
            kEmbedded1, kEmbedder1, ContentSettingsType::TPCD_METADATA_GRANTS),
        ContentSetting::CONTENT_SETTING_ALLOW);
    EXPECT_EQ(
        GetCookieSettings()->GetContentSettingForTesting(
            kEmbedded2, kEmbedder2, ContentSettingsType::TPCD_METADATA_GRANTS),
        ContentSetting::CONTENT_SETTING_BLOCK);

    MockComponentInstallation(metadata);

    ASSERT_EQ(GetCookieSettings()->GetTpcdMetadataGrantsForTesting().size(),
              1u);
    EXPECT_EQ(
        GetCookieSettings()->GetContentSettingForTesting(
            kEmbedded1, kEmbedder1, ContentSettingsType::TPCD_METADATA_GRANTS),
        ContentSetting::CONTENT_SETTING_BLOCK);
    EXPECT_EQ(
        GetCookieSettings()->GetContentSettingForTesting(
            kEmbedded2, kEmbedder2, ContentSettingsType::TPCD_METADATA_GRANTS),
        ContentSetting::CONTENT_SETTING_ALLOW);
  }
}

}  // namespace tpcd::metadata
