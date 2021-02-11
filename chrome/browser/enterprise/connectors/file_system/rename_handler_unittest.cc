// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <tuple>

#include "base/json/json_reader.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/enterprise/connectors/common.h"
#include "chrome/browser/enterprise/connectors/connectors_service.h"
#include "chrome/browser/enterprise/connectors/file_system/rename_handler.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "content/public/browser/download_item_utils.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/fake_download_item.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_connectors {

constexpr char kNormalSendDownloadToCloudPref[] = R"([
  {
    "service_provider": "box",
    "enterprise_id": "1234567890",
    "enable": [
      {
        "url_list": ["*"],
        "mime_types": ["text/plain", "image/png", "application/zip"]
      }
    ]
  }
])";

class RenameHandlerTest : public testing::Test,
                          public testing::WithParamInterface<bool> {
 public:
  RenameHandlerTest() : profile_manager_(TestingBrowserProcess::GetGlobal()) {
    if (enable_feature_flag()) {
      scoped_feature_list_.InitWithFeatures({kFileSystemConnectorEnabled}, {});
    } else {
      scoped_feature_list_.InitWithFeatures({}, {kFileSystemConnectorEnabled});
    }

    EXPECT_TRUE(profile_manager_.SetUp());
    profile_ = profile_manager_.CreateTestingProfile("test-user");

    // Make sure that from the connectors manager point of view the file system
    // connector should be enabled.  So that the only thing that controls
    // whether the rename handler is used or not is the feature flag.
    profile_->GetPrefs()->Set(
        ConnectorPref(FileSystemConnector::SEND_DOWNLOAD_TO_CLOUD),
        *base::JSONReader::Read(kNormalSendDownloadToCloudPref));
  }

  bool enable_feature_flag() const { return GetParam(); }

  Profile* profile() { return profile_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  TestingProfileManager profile_manager_;
  TestingProfile* profile_;
};

TEST_P(RenameHandlerTest, Test) {
  content::FakeDownloadItem item;
  item.SetURL(GURL("https://any.com"));
  content::DownloadItemUtils::AttachInfo(&item, profile(), nullptr);

  auto handler = FileSystemRenameHandler::CreateIfNeeded(&item);
  ASSERT_EQ(enable_feature_flag(), handler.get() != nullptr);
}

INSTANTIATE_TEST_CASE_P(, RenameHandlerTest, testing::Bool());

}  // namespace enterprise_connectors
