// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/soda_language_pack_component_installer.h"

#include "base/files/file_path.h"
#include "base/values.h"
#include "base/version.h"
#include "components/soda/constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace component_updater {

class SodaLanguagePackComponentInstallerTest : public ::testing::Test {
 public:
  SodaLanguagePackComponentInstallerTest()
      : fake_install_dir_(FILE_PATH_LITERAL("base/install/dir/")),
        fake_version_("0.0.1") {}

 protected:
  base::FilePath fake_install_dir_;
  base::Version fake_version_;
};

TEST_F(SodaLanguagePackComponentInstallerTest, TestGetLanguageComponentConfig) {
  std::optional<speech::SodaLanguagePackComponentConfig> config_by_name =
      speech::GetLanguageComponentConfig("fr-FR");

  ASSERT_TRUE(config_by_name);
  ASSERT_EQ(config_by_name.value().language_code, speech::LanguageCode::kFrFr);

  std::optional<speech::SodaLanguagePackComponentConfig>
      config_by_language_code =
          speech::GetLanguageComponentConfig(speech::LanguageCode::kFrFr);

  ASSERT_TRUE(config_by_language_code);
  ASSERT_EQ(config_by_language_code.value().language_code,
            speech::LanguageCode::kFrFr);
}

}  // namespace component_updater
