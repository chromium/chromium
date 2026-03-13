// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/platform_auth/entra_provider_android.h"

#include "base/test/test_future.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/origin.h"

class EntraProviderAndroidTest : public testing::Test {
 protected:
  enterprise_auth::EntraProviderAndroid provider_;
};

TEST_F(EntraProviderAndroidTest, SupportsOriginFiltering) {
  EXPECT_TRUE(provider_.SupportsOriginFiltering());
}

TEST_F(EntraProviderAndroidTest, ReturnsValidOrigins) {
  base::test::TestFuture<void> callback_called;
  enterprise_auth::EntraProviderAndroid::FetchOriginsCallback callback =
      base::BindOnce(
          [](base::OnceClosure future_callback,
             std::unique_ptr<std::vector<url::Origin>> origins) {
            ASSERT_TRUE(origins);
            ASSERT_FALSE(origins->empty());
            for (const auto& origin : *origins) {
              EXPECT_FALSE(origin.Serialize().empty());
            }
            std::move(future_callback).Run();
          },
          callback_called.GetCallback());
  provider_.FetchOrigins(std::move(callback));
  EXPECT_TRUE(callback_called.Wait());
}
