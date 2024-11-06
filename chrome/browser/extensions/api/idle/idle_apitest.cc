// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/api/idle/idle_manager_factory.h"
#include "extensions/browser/api/idle/test_idle_provider.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/extensions/extension_platform_apitest.h"
#else
#include "chrome/browser/extensions/extension_apitest.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace extensions {

#if BUILDFLAG(IS_ANDROID)
using IdleApiTestBase = ExtensionPlatformApiTest;
#else
using IdleApiTestBase = ExtensionApiTest;
#endif  // BUILDFLAG(IS_ANDROID)

class IdleApiTest : public IdleApiTestBase {
 public:
  IdleApiTest() = default;
  ~IdleApiTest() override = default;
  IdleApiTest(const IdleApiTest& other) = delete;
  IdleApiTest& operator=(const IdleApiTest& other) = delete;

 protected:
  void SetTestIdleProvider(int idle_time, bool locked) {
    auto idle_provider = std::make_unique<TestIdleProvider>();
    idle_provider->set_idle_time(idle_time);
    idle_provider->set_locked(locked);
    IdleManagerFactory::GetForBrowserContext(profile())
        ->SetIdleTimeProviderForTest(std::move(idle_provider));
  }
};

IN_PROC_BROWSER_TEST_F(IdleApiTest, QueryStateActive) {
  // Set up a test IdleProvider in the active state.
  SetTestIdleProvider(/*idle_time=*/0, /*locked=*/false);
  ASSERT_TRUE(
      RunExtensionTest("idle/query_state", {.custom_arg = "queryStateActive"}));
}

IN_PROC_BROWSER_TEST_F(IdleApiTest, QueryStateIdle) {
  // Set up a test IdleProvider in the idle state. The JS test uses the
  // value 15 for its "intervalInSeconds" value.
  SetTestIdleProvider(/*idle_time=*/15, /*locked=*/false);
  ASSERT_TRUE(
      RunExtensionTest("idle/query_state", {.custom_arg = "queryStateIdle"}));
}

IN_PROC_BROWSER_TEST_F(IdleApiTest, QueryStateAlmostIdle) {
  // Set up a test IdleProvider in the active state, just about to transition
  // to idle. The JS test uses the value 15 for its "intervalInSeconds" value.
  SetTestIdleProvider(/*idle_time=*/14, /*locked=*/false);
  ASSERT_TRUE(
      RunExtensionTest("idle/query_state", {.custom_arg = "queryStateActive"}));
}

IN_PROC_BROWSER_TEST_F(IdleApiTest, QueryStateLocked) {
  // Set up a test IdleProvider in the locked state.
  SetTestIdleProvider(/*idle_time=*/0, /*locked=*/true);
  ASSERT_TRUE(
      RunExtensionTest("idle/query_state", {.custom_arg = "queryStateLocked"}));
}

IN_PROC_BROWSER_TEST_F(IdleApiTest, SetDetectionInterval) {
  // The default value for this property is 60. 37 seems random enough for this
  // test.
  ASSERT_TRUE(
      RunExtensionTest("idle/set_detection_interval", {.custom_arg = "37"}));
  // The test should set the detection interval per the custom_arg value.
  EXPECT_EQ(37, IdleManagerFactory::GetForBrowserContext(profile())
                    ->GetThresholdForTest(last_loaded_extension_id()));
}

#if BUILDFLAG(IS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(IdleApiTest, IdleGetAutoLockDelay) {
  ASSERT_TRUE(RunExtensionTest("idle/get_auto_lock_delay")) << message_;
}
#else
IN_PROC_BROWSER_TEST_F(IdleApiTest, UnsupportedIdleGetAutoLockDelay) {
  ASSERT_TRUE(RunExtensionTest("idle/unsupported_get_auto_lock_delay"));
}
#endif
}  // namespace extensions
