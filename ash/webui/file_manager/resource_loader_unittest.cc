// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/file_manager/resource_loader.h"

#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_ui_data_source.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/webui/resource_path.h"

namespace ash {
namespace file_manager {

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
  const webui::ResourcePath kTestResources[] = {
      {"file_manager/images/icon192.png", 8},
      {"file_manager_fakes.js", 9},
      {"file_manager/untrusted_resources_files_img_content.css", 10},
      {"file_manager/untrusted_resources/files_img_content.css", 11},
  };

  const size_t kTestResourcesSize = std::size(kTestResources);

  AddFilesAppResources(source()->GetWebUIDataSource(), kTestResources,
                       kTestResourcesSize);

  EXPECT_EQ(8, source()->PathToIdrOrDefault("images/icon192.png"));
  EXPECT_EQ(-1, source()->PathToIdrOrDefault("file_manager_fakes.js"));
  EXPECT_EQ(10, source()->PathToIdrOrDefault(
                    "untrusted_resources_files_img_content.css"));
  EXPECT_EQ(-1, source()->PathToIdrOrDefault(
                    "untrusted_resources/files_img_content.css"));
}

}  // namespace file_manager
}  // namespace ash
