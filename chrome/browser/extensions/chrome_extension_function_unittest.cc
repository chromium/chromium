// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/dcheck_is_on.h"
#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/test/base/testing_browser_process.h"
#include "extensions/browser/extension_function.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension_builder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace {

void SuccessCallback(bool* did_respond,
                     ExtensionFunction::ResponseType type,
                     base::Value::List results,
                     const std::string& error,
                     mojom::ExtraResponseDataPtr) {
  EXPECT_EQ(ExtensionFunction::ResponseType::SUCCEEDED, type);
  *did_respond = true;
}

void FailCallback(bool* did_respond,
                  ExtensionFunction::ResponseType type,
                  base::Value::List results,
                  const std::string& error,
                  mojom::ExtraResponseDataPtr) {
  EXPECT_EQ(ExtensionFunction::ResponseType::FAILED, type);
  *did_respond = true;
}

class ValidationFunction : public ExtensionFunction {
 public:
  explicit ValidationFunction(bool should_succeed)
      : should_succeed_(should_succeed), did_respond_(false) {
    set_response_callback(base::BindOnce(
        (should_succeed ? &SuccessCallback : &FailCallback), &did_respond_));
  }

  ResponseAction Run() override {
    EXPECT_TRUE(should_succeed_);
    return RespondNow(NoArguments());
  }

  bool did_respond() { return did_respond_; }

 private:
  ~ValidationFunction() override {}
  bool should_succeed_;
  bool did_respond_;
};
}  // namespace

using ChromeExtensionFunctionUnitTest = ExtensionServiceTestBase;

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS_ASH)
#define MAYBE_SimpleFunctionTest DISABLED_SimpleFunctionTest
#else
#define MAYBE_SimpleFunctionTest SimpleFunctionTest
#endif
TEST_F(ChromeExtensionFunctionUnitTest, MAYBE_SimpleFunctionTest) {
  scoped_refptr<ValidationFunction> function(new ValidationFunction(true));
  function->RunWithValidation().Execute();
  EXPECT_TRUE(function->did_respond());
}

TEST_F(ChromeExtensionFunctionUnitTest, BrowserShutdownValidationFunctionTest) {
  TestingBrowserProcess::GetGlobal()->SetShuttingDown(true);
  scoped_refptr<ValidationFunction> function(new ValidationFunction(false));
  function->RunWithValidation().Execute();
  TestingBrowserProcess::GetGlobal()->SetShuttingDown(false);
  EXPECT_TRUE(function->did_respond());
}

// Verifies that destroying the ExtensionFunction without responding is ok if
// the extension has been unloaded.
TEST_F(ChromeExtensionFunctionUnitTest, DestructionWithoutResponseOnUnload) {
  InitializeEmptyExtensionService();
  scoped_refptr<const Extension> extension = ExtensionBuilder("foo").Build();
  service()->AddExtension(extension.get());
  ASSERT_TRUE(registry()->enabled_extensions().Contains(extension->id()));

  auto function = base::MakeRefCounted<ValidationFunction>(false);
  function->set_extension(extension);
  function->SetBrowserContextForTesting(browser_context());

  service()->DisableExtension(extension->id(),
                              disable_reason::DISABLE_USER_ACTION);
  ASSERT_TRUE(registry()->disabled_extensions().Contains(extension->id()));

  // Destroying the extension function without responding if the extension has
  // been unloaded should not cause a crash.
  function.reset();
}

#if DCHECK_IS_ON()
using ChromeExtensionFunctionDeathTest = ChromeExtensionFunctionUnitTest;

// Verify that destroying the extension function without responding causes a
// DCHECK failure.
#if BUILDFLAG(IS_WIN)
#define MAYBE_DestructionWithoutResponse DISABLED_DestructionWithoutResponse
#else
#define MAYBE_DestructionWithoutResponse DestructionWithoutResponse
#endif
TEST_F(ChromeExtensionFunctionDeathTest, MAYBE_DestructionWithoutResponse) {
  ASSERT_DEATH(
      {
        InitializeEmptyExtensionService();
        scoped_refptr<const Extension> extension =
            ExtensionBuilder("foo").Build();
        service()->AddExtension(extension.get());

        ASSERT_TRUE(registry()->enabled_extensions().Contains(extension->id()));

        auto function = base::MakeRefCounted<ValidationFunction>(false);
        function->set_extension(extension);
        function.reset();
      },
      "");
}
#endif  // DCHECK_IS_ON()

}  // namespace extensions
