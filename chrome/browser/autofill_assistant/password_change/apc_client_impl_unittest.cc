// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill_assistant/password_change/apc_client_impl.h"

#include "base/memory/raw_ptr.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_contents_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {
constexpr char kUrl1[] = "https://www.example.com";
constexpr char kUsername1[] = "Lori";
}  // namespace

class ApcClientImplTest : public testing::Test {
 public:
  ApcClientImplTest()
      : web_contents_(
            web_contents_factory_.CreateWebContents(&testing_profile_)) {
    apc_client_ = ApcClient::GetOrCreateForWebContents(web_contents());
  }

  ApcClient* apc_client() { return apc_client_; }
  content::WebContents* web_contents() { return web_contents_; }

 private:
  // Supporting members to create the testing environment.
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile testing_profile_;
  content::TestWebContentsFactory web_contents_factory_;
  raw_ptr<content::WebContents> web_contents_;

  // The ApcClientImpl that is tested.
  raw_ptr<ApcClient> apc_client_;
};

TEST_F(ApcClientImplTest, CreateAndStartApcFlow) {
  raw_ptr<ApcClient> client =
      ApcClient::GetOrCreateForWebContents(web_contents());

  // There is one client per WebContents.
  EXPECT_EQ(client, apc_client());

  // The |ApcClient| is paused.
  EXPECT_FALSE(client->IsRunning());

  EXPECT_TRUE(client->Start(GURL(kUrl1), kUsername1, /*skip_login=*/true));
  EXPECT_TRUE(client->IsRunning());

  // We cannot start a second flow.
  EXPECT_FALSE(client->Start(GURL(kUrl1), kUsername1, /*skip_login=*/true));

  client->Stop();
  EXPECT_FALSE(client->IsRunning());
}
