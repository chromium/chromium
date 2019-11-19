// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_function_test_utils.h"

#include <string>
#include <utility>

#include "base/files/file_path.h"
#include "base/json/json_reader.h"
#include "base/macros.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/tabs/tabs_constants.h"
#include "chrome/browser/extensions/browser_extension_window_controller.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "components/crx_file/id_util.h"
#include "extensions/browser/api_test_utils.h"
#include "extensions/browser/extension_function.h"
#include "extensions/browser/extension_function_dispatcher.h"
#include "extensions/common/extension.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::WebContents;
using extensions::Extension;
using extensions::Manifest;
namespace keys = extensions::tabs_constants;

namespace {

class TestFunctionDispatcherDelegate
    : public extensions::ExtensionFunctionDispatcher::Delegate {
 public:
  explicit TestFunctionDispatcherDelegate(Browser* browser) :
      browser_(browser) {}
  ~TestFunctionDispatcherDelegate() override {}

 private:
  extensions::WindowController* GetExtensionWindowController() const override {
    return browser_->extension_window_controller();
  }

  WebContents* GetAssociatedWebContents() const override { return NULL; }

  Browser* browser_;
};

}  // namespace

namespace extension_function_test_utils {

base::ListValue* ParseList(const std::string& data) {
  std::unique_ptr<base::Value> result = base::JSONReader::ReadDeprecated(data);
  if (!result) {
    ADD_FAILURE() << "Failed to parse: " << data;
    return nullptr;
  }
  base::ListValue* list = NULL;
  result->GetAsList(&list);
  ignore_result(result.release());
  return list;
}

base::DictionaryValue* ToDictionary(base::Value* val) {
  EXPECT_TRUE(val);
  EXPECT_EQ(base::Value::Type::DICTIONARY, val->type());
  return static_cast<base::DictionaryValue*>(val);
}

base::ListValue* ToList(base::Value* val) {
  EXPECT_TRUE(val);
  EXPECT_EQ(base::Value::Type::LIST, val->type());
  return static_cast<base::ListValue*>(val);
}

bool HasAnyPrivacySensitiveFields(base::DictionaryValue* val) {
  std::string result;
  if (val->GetString(keys::kUrlKey, &result) ||
      val->GetString(keys::kTitleKey, &result) ||
      val->GetString(keys::kFaviconUrlKey, &result) ||
      val->GetString(keys::kPendingUrlKey, &result))
    return true;
  return false;
}

std::string RunFunctionAndReturnError(ExtensionFunction* function,
                                      const std::string& args,
                                      Browser* browser) {
  return RunFunctionAndReturnError(function, args, browser,
                                   extensions::api_test_utils::NONE);
}
std::string RunFunctionAndReturnError(
    ExtensionFunction* function,
    const std::string& args,
    Browser* browser,
    extensions::api_test_utils::RunFunctionFlags flags) {
  scoped_refptr<ExtensionFunction> function_owner(function);
  RunFunction(function, args, browser, flags);
  // When sending a response, the function will set an empty list value if there
  // is no specified result.
  const base::ListValue* results = function->GetResultList();
  CHECK(results);
  EXPECT_TRUE(results->empty()) << "Did not expect a result";
  CHECK(function->response_type());
  EXPECT_EQ(ExtensionFunction::FAILED, *function->response_type());
  return function->GetError();
}

base::Value* RunFunctionAndReturnSingleResult(ExtensionFunction* function,
                                              const std::string& args,
                                              Browser* browser) {
  return RunFunctionAndReturnSingleResult(function, args, browser,
                                          extensions::api_test_utils::NONE);
}
base::Value* RunFunctionAndReturnSingleResult(
    ExtensionFunction* function,
    const std::string& args,
    Browser* browser,
    extensions::api_test_utils::RunFunctionFlags flags) {
  scoped_refptr<ExtensionFunction> function_owner(function);
  RunFunction(function, args, browser, flags);
  EXPECT_TRUE(function->GetError().empty()) << "Unexpected error: "
      << function->GetError();
  const base::Value* single_result = NULL;
  if (function->GetResultList() != NULL &&
      function->GetResultList()->Get(0, &single_result)) {
    return single_result->DeepCopy();
  }
  return NULL;
}

bool RunFunction(ExtensionFunction* function,
                 const std::string& args,
                 Browser* browser,
                 extensions::api_test_utils::RunFunctionFlags flags) {
  std::unique_ptr<base::ListValue> parsed_args(ParseList(args));
  EXPECT_TRUE(parsed_args.get())
      << "Could not parse extension function arguments: " << args;
  return RunFunction(function, std::move(parsed_args), browser, flags);
}

bool RunFunction(ExtensionFunction* function,
                 std::unique_ptr<base::ListValue> args,
                 Browser* browser,
                 extensions::api_test_utils::RunFunctionFlags flags) {
  TestFunctionDispatcherDelegate dispatcher_delegate(browser);
  std::unique_ptr<extensions::ExtensionFunctionDispatcher> dispatcher(
      new extensions::ExtensionFunctionDispatcher(browser->profile()));
  dispatcher->set_delegate(&dispatcher_delegate);
  return extensions::api_test_utils::RunFunction(function, std::move(args),
                                                 browser->profile(),
                                                 std::move(dispatcher), flags);
}

} // namespace extension_function_test_utils
