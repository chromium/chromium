// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/extensions/default_app_order.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <vector>

#include "ash/constants/ash_paths.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/scoped_path_override.h"
#include "components/app_constants/constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace {

const base::FilePath::CharType kTestFile[] =
    FILE_PATH_LITERAL("test_default_app_order.json");
}

class DefaultAppOrderTest : public testing::Test {
 public:
  DefaultAppOrderTest() {}

  DefaultAppOrderTest(const DefaultAppOrderTest&) = delete;
  DefaultAppOrderTest& operator=(const DefaultAppOrderTest&) = delete;

  ~DefaultAppOrderTest() override {}

  // testing::Test overrides:
  void SetUp() override { default_app_order::Get(&built_in_default_); }
  void TearDown() override {}

  bool IsBuiltInDefault(const std::vector<std::string>& apps) {
    if (apps.size() != built_in_default_.size())
      return false;

    for (size_t i = 0; i < built_in_default_.size(); ++i) {
      if (built_in_default_[i] != apps[i])
        return false;
    }

    return true;
  }

  void SetExternalFile(const base::FilePath& path) {
    path_override_ = std::make_unique<base::ScopedPathOverride>(
        ash::FILE_DEFAULT_APP_ORDER, path, /*is_absolute=*/true,
        /*create=*/false);
  }

  void CreateExternalOrderFile(const std::string& content) {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    base::FilePath external_file = temp_dir_.GetPath().Append(kTestFile);
    base::WriteFile(external_file, content);
    SetExternalFile(external_file);
  }

 private:
  std::vector<std::string> built_in_default_;

  base::ScopedTempDir temp_dir_;
  std::unique_ptr<base::ScopedPathOverride> path_override_;
};

// Tests that the built-in default order is returned when ExternalLoader is not
// created.
TEST_F(DefaultAppOrderTest, BuiltInDefault) {
  std::vector<std::string> apps;
  default_app_order::Get(&apps);
  EXPECT_TRUE(IsBuiltInDefault(apps));
}

// Tests external order file overrides built-in default.
TEST_F(DefaultAppOrderTest, ExternalOrder) {
  const char kExternalOrder[] =
      "[\"app1\",\"app2\",\"app3\","
      "{ \"oem_apps_folder\": true,\"localized_content\": {"
      "    \"default\": {\"name\": \"OEM name\"}}}]";
  CreateExternalOrderFile(std::string(kExternalOrder));

  std::unique_ptr<default_app_order::ExternalLoader> loader(
      new default_app_order::ExternalLoader(false));

  std::vector<std::string> apps;
  default_app_order::Get(&apps);
  EXPECT_EQ(3u, apps.size());
  EXPECT_EQ(std::string("app1"), apps[0]);
  EXPECT_EQ(std::string("app2"), apps[1]);
  EXPECT_EQ(std::string("app3"), apps[2]);
  EXPECT_EQ(std::string("OEM name"), default_app_order::GetOemAppsFolderName());
}

// Tests none-existent order file gives built-in default.
TEST_F(DefaultAppOrderTest, NoExternalFile) {
  base::ScopedTempDir scoped_tmp_dir;
  ASSERT_TRUE(scoped_tmp_dir.CreateUniqueTempDir());

  base::FilePath none_existent_file =
      scoped_tmp_dir.GetPath().AppendASCII("none_existent_file");
  ASSERT_FALSE(base::PathExists(none_existent_file));
  SetExternalFile(none_existent_file);

  std::unique_ptr<default_app_order::ExternalLoader> loader(
      new default_app_order::ExternalLoader(false));

  std::vector<std::string> apps;
  default_app_order::Get(&apps);
  EXPECT_TRUE(IsBuiltInDefault(apps));
}

// Tests bad json file gives built-in default.
TEST_F(DefaultAppOrderTest, BadExternalFile) {
  const char kExternalOrder[] = "This is not a valid json.";
  CreateExternalOrderFile(std::string(kExternalOrder));

  std::unique_ptr<default_app_order::ExternalLoader> loader(
      new default_app_order::ExternalLoader(false));

  std::vector<std::string> apps;
  default_app_order::Get(&apps);
  EXPECT_TRUE(IsBuiltInDefault(apps));
}

TEST_F(DefaultAppOrderTest, ImportDefault) {
  const char kExternalOrder[] =
      "[\"app1\","
      "{ \"import_default_order\": true }, \"app2\"]";
  CreateExternalOrderFile(std::string(kExternalOrder));

  std::unique_ptr<default_app_order::ExternalLoader> loader(
      new default_app_order::ExternalLoader(false));

  std::vector<std::string> apps;
  default_app_order::Get(&apps);
  EXPECT_EQ(default_app_order::DefaultAppCount() + 2, apps.size());
  EXPECT_EQ(std::string("app1"), apps[0]);
  EXPECT_EQ(app_constants::kChromeAppId, apps[1]);
  EXPECT_EQ(std::string("app2"),
            apps[default_app_order::DefaultAppCount() + 1]);
}

}  // namespace chromeos
