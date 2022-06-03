// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_apitest.h"
#include "chromeos/cryptohome/system_salt_getter.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"

class WallPaperApiTest : public extensions::ExtensionApiTest {
 public:
  ~WallPaperApiTest() override = default;

  void SetUpOnMainThread() override {
    extensions::ExtensionApiTest::SetUpOnMainThread();
    host_resolver()->AddRule("a.com", "127.0.0.1");
  }
};

IN_PROC_BROWSER_TEST_F(WallPaperApiTest, Wallpaper) {
  chromeos::SystemSaltGetter::Get()->SetRawSaltForTesting(
      chromeos::SystemSaltGetter::RawSalt({1, 2, 3, 4, 5, 6, 7, 8}));
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunExtensionTest("wallpaper")) << message_;
}
