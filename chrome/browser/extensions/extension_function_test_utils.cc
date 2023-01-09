// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_function_test_utils.h"

#include <string>
#include <utility>

#include "base/files/file_path.h"
#include "base/json/json_reader.h"
#include "base/memory/raw_ptr.h"
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
#include "third_party/abseil-cpp/absl/types/optional.h"

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

  WebContents* GetAssociatedWebContents() const override { return nullptr; }

  raw_ptr<Browser, DanglingUntriaged> browser_;
};

}  // namespace

namespace extension_function_test_utils {

absl::optional<base::Value::List> ParseList(const std::string& data) {
  absl::optional<base::Value> result = base::JSONReader::Read(data);
  if (!result) {
    ADD_FAILURE() << "Failed to parse: " << data;
    return absl::nullopt;
  }
  if (!result->is_list())
    return absl::nullopt;
  return std::move(*result).TakeList();
}

base::Value::Dict ToDictionary(std::unique_ptr<base::Value> val) {
  if (!val || !val->is_dict()) {
    ADD_FAILURE() << "val is nullptr or is not a dictonary.";
    return base::Value::Dict();
  }
  return std::move(*val).TakeDict();
}

base::Value::Dict ToDictionary(const base::Value& val) {
  EXPECT_TRUE(val.is_dict());
  if (!val.is_dict())
    return base::Value::Dict();
  return val.GetDict().Clone();
}

base::Value::List ToList(std::unique_ptr<base::Value> val) {
  if (!val || !val->is_list()) {
    ADD_FAILURE() << "val is nullptr or is not a list.";
    return base::Value::List();
  }
  return std::move(*val).TakeList();
}

bool HasAnyPrivacySensitiveFields(const base::Value::Dict& dict) {
  constexpr std::array privacySensitiveKeys{keys::kUrlKey, keys::kTitleKey,
                                            keys::kFaviconUrlKey,
                                            keys::kPendingUrlKey};
  for (auto* key : privacySensitiveKeys) {
    if (dict.contains(key))
      return true;
  }
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
  const base::Value::List* results = function->GetResultListForTest();
  CHECK(results);
  EXPECT_TRUE(results->empty()) << "Did not expect a result";
  CHECK(function->response_type());
  EXPECT_EQ(ExtensionFunction::FAILED, *function->response_type());
  return function->GetError();
}

std::unique_ptr<base::Value> RunFunctionAndReturnSingleResult(
    ExtensionFunction* function,
    const std::string& args,
    Browser* browser) {
  return RunFunctionAndReturnSingleResult(function, args, browser,
                                          extensions::api_test_utils::NONE);
}

std::unique_ptr<base::Value> RunFunctionAndReturnSingleResult(
    ExtensionFunction* function,
    const std::string& args,
    Browser* browser,
    extensions::api_test_utils::RunFunctionFlags flags) {
  scoped_refptr<ExtensionFunction> function_owner(function);
  function->preserve_results_for_testing();
  RunFunction(function, args, browser, flags);
  EXPECT_TRUE(function->GetError().empty()) << "Unexpected error: "
      << function->GetError();
  if (function->GetResultListForTest() &&
      !function->GetResultListForTest()->empty()) {
    return base::Value::ToUniquePtrValue(
        (*function->GetResultListForTest())[0].Clone());
  }
  return nullptr;
}

bool RunFunction(ExtensionFunction* function,
                 const std::string& args,
                 Browser* browser,
                 extensions::api_test_utils::RunFunctionFlags flags) {
  absl::optional<base::Value::List> maybe_parsed_args(ParseList(args));
  EXPECT_TRUE(maybe_parsed_args)
      << "Could not parse extension function arguments: " << args;
  return RunFunction(function, std::move(*maybe_parsed_args), browser, flags);
}

bool RunFunction(ExtensionFunction* function,
                 base::Value::List args,
                 Browser* browser,
                 extensions::api_test_utils::RunFunctionFlags flags) {
  TestFunctionDispatcherDelegate dispatcher_delegate(browser);
  auto dispatcher = std::make_unique<extensions::ExtensionFunctionDispatcher>(
      browser->profile());
  dispatcher->set_delegate(&dispatcher_delegate);
  return extensions::api_test_utils::RunFunction(function, std::move(args),
                                                 std::move(dispatcher), flags);
}

} // namespace extension_function_test_utils
