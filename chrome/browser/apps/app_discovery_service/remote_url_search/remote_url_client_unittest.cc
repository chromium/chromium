// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_discovery_service/remote_url_search/remote_url_client.h"

#include "base/values.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace apps {

// TODO(crbug.com/1244221): This test just exercises the unimplemented Fetch
// method as a stand-in for proper tests once the logic is implemented.
TEST(RemoteUrlClientTest, FetchReturnsOk) {
  RemoteUrlClient client(GURL("test.url"));
  client.Fetch(
      base::BindOnce([](RemoteUrlClient::Status status, base::Value value) {
        EXPECT_EQ(status, RemoteUrlClient::Status::kOk);
      }));
}

}  // namespace apps
