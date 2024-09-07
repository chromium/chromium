// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/file_manager/resource_loader.h"

#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_ui_data_source.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/webui/resource_path.h"
#include "url/url_util.h"

namespace ash {
namespace file_manager {

GURL GetURL(const std::string& path) {
  return GURL("chrome://test-file-manager-host/" + path);
}

class ResourceLoaderTest : public testing::Test {
 public:
  ResourceLoaderTest() = default;

  content::TestWebUIDataSource* source() { return source_.get(); }

 private:
  void SetUp() override {
    source_ = content::TestWebUIDataSource::Create("test-file-manager-host");
  }

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<content::TestWebUIDataSource> source_;
};

TEST_F(ResourceLoaderTest, AddFilesAppResources) {
  url::ScopedSchemeRegistryForTests scoped_registry;
  url::AddStandardScheme("chrome", url::SchemeType::SCHEME_WITH_HOST);

  const webui::ResourcePath kTestResources[] = {
      {"file_manager/images/icon192.png", 8},
      {"file_manager/untrusted_resources_files_img_content.css", 10},
      {"file_manager/untrusted_resources/files_img_content.css", 11},
  };

  AddFilesAppResources(source()->GetWebUIDataSource(), kTestResources);

  EXPECT_EQ(8, source()->URLToIdrOrDefault(GetURL("images/icon192.png")));
  EXPECT_EQ(10, source()->URLToIdrOrDefault(
                    GetURL("untrusted_resources_files_img_content.css")));
  EXPECT_EQ(-1, source()->URLToIdrOrDefault(
                    GetURL("untrusted_resources/files_img_content.css")));
}

}  // namespace file_manager
}  // namespace ash
