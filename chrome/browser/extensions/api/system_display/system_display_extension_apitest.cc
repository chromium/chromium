// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <optional>
#include <string>

#include "base/debug/leak_annotations.h"
#include "base/functional/bind.h"
#include "base/test/run_until.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/api/system_display/system_display_api.h"
#include "extensions/browser/api_test_utils.h"
#include "extensions/browser/mock_display_info_provider.h"
#include "extensions/common/api/system_display.h"
#include "extensions/test/extension_test_message_listener.h"
#include "ui/display/display.h"

namespace extensions {

using ContextType = extensions::browser_test_util::ContextType;

class SystemDisplayExtensionApiTest
    : public ExtensionApiTest,
      public testing::WithParamInterface<ContextType> {
 public:
  SystemDisplayExtensionApiTest() : ExtensionApiTest(GetParam()) {}
  ~SystemDisplayExtensionApiTest() override = default;
  SystemDisplayExtensionApiTest(const SystemDisplayExtensionApiTest&) = delete;
  SystemDisplayExtensionApiTest& operator=(
      const SystemDisplayExtensionApiTest&) = delete;

  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();
    DisplayInfoProvider::InitializeForTesting(provider_.get());
  }

  void TearDownOnMainThread() override {
    ExtensionApiTest::TearDownOnMainThread();
  }

 protected:
  std::unique_ptr<MockDisplayInfoProvider> provider_ =
      std::make_unique<MockDisplayInfoProvider>();
};

// TODO(crbug.com/40779611): Revisit this after screen creation refactoring.
#if !BUILDFLAG(IS_WIN)

INSTANTIATE_TEST_SUITE_P(PersistentBackground,
                         SystemDisplayExtensionApiTest,
                         ::testing::Values(ContextType::kPersistentBackground));
INSTANTIATE_TEST_SUITE_P(ServiceWorker,
                         SystemDisplayExtensionApiTest,
                         ::testing::Values(ContextType::kServiceWorker));

IN_PROC_BROWSER_TEST_P(SystemDisplayExtensionApiTest, GetDisplayInfo) {
  ASSERT_TRUE(RunExtensionTest("system_display/info")) << message_;
}

class SystemDisplayExtensionApiEventTest
    : public SystemDisplayExtensionApiTest {
 public:
  SystemDisplayExtensionApiEventTest() = default;
  ~SystemDisplayExtensionApiEventTest() override = default;
  SystemDisplayExtensionApiEventTest(
      const SystemDisplayExtensionApiEventTest&) = delete;
  SystemDisplayExtensionApiEventTest& operator=(
      const SystemDisplayExtensionApiEventTest&) = delete;
};

INSTANTIATE_TEST_SUITE_P(ServiceWorker,
                         SystemDisplayExtensionApiEventTest,
                         ::testing::Values(ContextType::kServiceWorker));

IN_PROC_BROWSER_TEST_P(SystemDisplayExtensionApiEventTest,
                       OnDisplayChangedEvent) {
  ExtensionTestMessageListener listener_for_extension_ready("ready");
  const Extension* extension = LoadExtension(
      test_data_dir_.AppendASCII("system_display/on_display_changed"));
  ASSERT_TRUE(extension);
  ASSERT_TRUE(listener_for_extension_ready.WaitUntilSatisfied());
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return provider_->is_observing_for_testing(); }));

  ExtensionTestMessageListener listener_for_success("success");
  provider_->TriggerOnDisplayChangedForTesting();
  ASSERT_TRUE(listener_for_success.WaitUntilSatisfied());

  UnloadExtension(extension->id());
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return !provider_->is_observing_for_testing(); }));
}

#endif  // BUILDFLAG(IS_WIN)

#if !BUILDFLAG(IS_CHROMEOS)

using SystemDisplayExtensionApiFunctionTest = SystemDisplayExtensionApiTest;

INSTANTIATE_TEST_SUITE_P(PersistentBackground,
                         SystemDisplayExtensionApiFunctionTest,
                         ::testing::Values(ContextType::kPersistentBackground));

IN_PROC_BROWSER_TEST_P(SystemDisplayExtensionApiFunctionTest, SetDisplay) {
  scoped_refptr<SystemDisplaySetDisplayPropertiesFunction> set_info_function(
      new SystemDisplaySetDisplayPropertiesFunction());

  set_info_function->set_has_callback(true);

  EXPECT_EQ(SystemDisplayCrOSRestrictedFunction::kCrosOnlyError,
            api_test_utils::RunFunctionAndReturnError(
                set_info_function.get(), "[\"display_id\", {}]", profile()));

  std::optional<base::DictValue> set_info = provider_->GetSetInfoValue();
  EXPECT_FALSE(set_info);
}

#endif  // !BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_ANDROID)

class SystemDisplayExtensionApiAndroidProviderTest : public ExtensionApiTest {
 public:
  SystemDisplayExtensionApiAndroidProviderTest()
      : ExtensionApiTest(ContextType::kServiceWorker) {}

  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();
    DisplayInfoProvider::ResetForTesting();
  }
};

IN_PROC_BROWSER_TEST_F(SystemDisplayExtensionApiAndroidProviderTest,
                       GetDisplayInfoWithAndroidProvider) {
  ASSERT_TRUE(RunExtensionTest("system_display/info_android")) << message_;
}

#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace extensions
