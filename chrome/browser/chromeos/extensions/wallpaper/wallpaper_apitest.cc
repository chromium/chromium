// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_apitest.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chromeos/ash/components/cryptohome/system_salt_getter.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/crosapi/mojom/wallpaper.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#endif

class WallPaperApiTest : public extensions::ExtensionApiTest {
 public:
  ~WallPaperApiTest() override = default;

  void SetUpOnMainThread() override {
    extensions::ExtensionApiTest::SetUpOnMainThread();
    host_resolver()->AddRule("a.com", "127.0.0.1");
  }
};

#if BUILDFLAG(IS_CHROMEOS_ASH)
IN_PROC_BROWSER_TEST_F(WallPaperApiTest, Wallpaper) {
  ash::SystemSaltGetter::Get()->SetRawSaltForTesting(
      ash::SystemSaltGetter::RawSalt({1, 2, 3, 4, 5, 6, 7, 8}));

  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunExtensionTest("wallpaper")) << message_;
}
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
IN_PROC_BROWSER_TEST_F(WallPaperApiTest, Wallpaper) {
  ASSERT_TRUE(StartEmbeddedTestServer());

  auto* lacros_service = chromeos::LacrosService::Get();
  if (lacros_service->IsAvailable<crosapi::mojom::Wallpaper>()) {
    ASSERT_TRUE(RunExtensionTest("wallpaper")) << message_;
  } else {
    ASSERT_TRUE(
        RunExtensionTest("wallpaper", {.custom_arg = "crosapi_unavailable"}))
        << message_;
  }
}
#endif
