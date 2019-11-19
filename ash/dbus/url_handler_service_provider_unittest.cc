// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/dbus/url_handler_service_provider.h"

#include <memory>

#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace ash {

class UrlHandlerServiceProviderTest : public testing::Test {};

TEST_F(UrlHandlerServiceProviderTest, UrlAllowed) {
  std::unique_ptr<UrlHandlerServiceProvider> provider =
      std::make_unique<UrlHandlerServiceProvider>();
  EXPECT_TRUE(provider->UrlAllowed(GURL("data://test/data")));
  EXPECT_TRUE(provider->UrlAllowed(GURL("file:///test/file")));
  EXPECT_TRUE(provider->UrlAllowed(GURL("ftp://test/ftp")));
  EXPECT_TRUE(provider->UrlAllowed(GURL("http://test/http")));
  EXPECT_TRUE(provider->UrlAllowed(GURL("https://test/https")));
  EXPECT_TRUE(provider->UrlAllowed(GURL("mailto://test/mailto")));

  EXPECT_FALSE(provider->UrlAllowed(GURL("notvalid")));
  EXPECT_FALSE(provider->UrlAllowed(GURL("about://test/about")));
  EXPECT_FALSE(provider->UrlAllowed(GURL("blob://test/blob")));
  EXPECT_FALSE(provider->UrlAllowed(GURL("content://test/content")));
  EXPECT_FALSE(provider->UrlAllowed(GURL("cid://test/cid")));
  EXPECT_FALSE(provider->UrlAllowed(GURL("filesystem://test/filesystem")));
  EXPECT_FALSE(provider->UrlAllowed(GURL("gopher://test/gopher")));
  EXPECT_FALSE(provider->UrlAllowed(GURL("javascript://test/javascript")));
  EXPECT_FALSE(provider->UrlAllowed(GURL("ws://test/ws")));
  EXPECT_FALSE(provider->UrlAllowed(GURL("wss://test/wss")));
}

}  // namespace ash
