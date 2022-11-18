// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_api_unittest.h"

#include "chrome/browser/extensions/extension_function_test_utils.h"
#include "chrome/browser/ui/browser.h"
#include "extensions/browser/extension_function.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_handlers/background_info.h"

namespace utils = extension_function_test_utils;

namespace extensions {

ExtensionApiUnittest::~ExtensionApiUnittest() {
}

void ExtensionApiUnittest::SetUp() {
  BrowserWithTestWindowTest::SetUp();
  extension_ = ExtensionBuilder("Test").Build();
}

std::unique_ptr<base::Value> ExtensionApiUnittest::RunFunctionAndReturnValue(
    ExtensionFunction* function,
    const std::string& args) {
  function->set_extension(extension());
  return utils::RunFunctionAndReturnSingleResult(function, args, browser());
}

absl::optional<base::Value::Dict>
ExtensionApiUnittest::RunFunctionAndReturnDictionary(
    ExtensionFunction* function,
    const std::string& args) {
  std::unique_ptr<base::Value> value =
      RunFunctionAndReturnValue(function, args);
  // We expect to either have successfully retrieved a dictionary from the
  // value or the value to have been nullptr.
  EXPECT_TRUE(!value || value->is_dict());

  if (!value || !value->is_dict())
    return absl::nullopt;

  return std::move(*value).TakeDict();
}

std::unique_ptr<base::Value> ExtensionApiUnittest::RunFunctionAndReturnList(
    ExtensionFunction* function,
    const std::string& args) {
  base::Value* value = RunFunctionAndReturnValue(function, args).release();

  if (value && !value->is_list())
    delete value;

  // We expect to have successfully retrieved a list from the value.
  EXPECT_TRUE(value && value->is_list());
  return std::unique_ptr<base::Value>(value);
}

std::string ExtensionApiUnittest::RunFunctionAndReturnError(
    ExtensionFunction* function,
    const std::string& args) {
  function->set_extension(extension());
  return utils::RunFunctionAndReturnError(function, args, browser());
}

void ExtensionApiUnittest::RunFunction(ExtensionFunction* function,
                                       const std::string& args) {
  RunFunctionAndReturnValue(function, args);
}

}  // namespace extensions
