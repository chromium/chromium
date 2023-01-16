// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_deduplication_service/app_deduplication_mapper.h"
#include "base/logging.h"
#include "chrome/browser/apps/app_deduplication_service/proto/app_deduplication.pb.h"
#include "chrome/browser/apps/app_deduplication_service/proto/deduplication_data.pb.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace apps::deduplication {

class AppDeduplicationMapperTest : public testing::Test {
 protected:
  AppDeduplicationMapper mapper_;
};

TEST_F(AppDeduplicationMapperTest, TestDeduplicateResponseValid) {
  proto::DeduplicateResponse response;
  auto* app = response.add_app_group()->add_app();
  app->set_app_id("com.skype.raidar");
  app->set_platform("phonehub");

  absl::optional<proto::DeduplicateData> data =
      mapper_.ToDeduplicateData(response);
  ASSERT_TRUE(data.has_value());
  EXPECT_EQ(data->app_group_size(), 1);
  auto observed_app = data->app_group(0).app(0);
  EXPECT_EQ(observed_app.app_id(), "com.skype.raidar");
  EXPECT_EQ(observed_app.platform(), "phonehub");
}

TEST_F(AppDeduplicationMapperTest, TestDeduplicateResponseEmpty) {
  proto::DeduplicateResponse empty_response;
  ASSERT_FALSE(mapper_.ToDeduplicateData(empty_response).has_value());
}

TEST_F(AppDeduplicationMapperTest, TestDeduplicateResponseEmptyAppGroup) {
  proto::DeduplicateResponse response;
  response.add_app_group();

  ASSERT_FALSE(mapper_.ToDeduplicateData(response).has_value());
}

TEST_F(AppDeduplicationMapperTest, TestDeduplicateResponseEmptyPlatform) {
  proto::DeduplicateResponse response;
  auto* app = response.add_app_group()->add_app();
  app->set_app_id("com.skype.raidar");

  ASSERT_FALSE(mapper_.ToDeduplicateData(response).has_value());
}

TEST_F(AppDeduplicationMapperTest, TestDeduplicateResponseEmptyAppId) {
  proto::DeduplicateResponse response;
  auto* app = response.add_app_group()->add_app();
  app->set_platform("phonehub");

  ASSERT_FALSE(mapper_.ToDeduplicateData(response).has_value());
}

}  // namespace apps::deduplication
