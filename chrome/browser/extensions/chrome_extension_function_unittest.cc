// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/bind.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/test/base/testing_browser_process.h"
#include "extensions/browser/extension_function.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace {

void SuccessCallback(bool* did_respond,
                     ExtensionFunction::ResponseType type,
                     const base::ListValue& results,
                     const std::string& error) {
  EXPECT_EQ(ExtensionFunction::ResponseType::SUCCEEDED, type);
  *did_respond = true;
}

void FailCallback(bool* did_respond,
                  ExtensionFunction::ResponseType type,
                  const base::ListValue& results,
                  const std::string& error) {
  EXPECT_EQ(ExtensionFunction::ResponseType::FAILED, type);
  *did_respond = true;
}

class ValidationFunction : public ExtensionFunction {
 public:
  explicit ValidationFunction(bool should_succeed)
      : should_succeed_(should_succeed), did_respond_(false) {
    set_response_callback(base::Bind(
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

#if defined(OS_WIN) || defined(CHROMEOS)
#define MAYBE_SimpleFunctionTest DISABLED_SimpleFunctionTest
#else
#define MAYBE_SimpleFunctionTest SimpleFunctionTest
#endif
TEST_F(ChromeExtensionFunctionUnitTest, MAYBE_SimpleFunctionTest) {
  scoped_refptr<ValidationFunction> function(new ValidationFunction(true));
  function->RunWithValidation()->Execute();
  EXPECT_TRUE(function->did_respond());
}

TEST_F(ChromeExtensionFunctionUnitTest, BrowserShutdownValidationFunctionTest) {
  TestingBrowserProcess::GetGlobal()->SetShuttingDown(true);
  scoped_refptr<ValidationFunction> function(new ValidationFunction(false));
  function->RunWithValidation()->Execute();
  TestingBrowserProcess::GetGlobal()->SetShuttingDown(false);
  EXPECT_TRUE(function->did_respond());
}

}  // namespace extensions
