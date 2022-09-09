// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_api_unittest.h"

#include "base/values.h"
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

std::unique_ptr<base::DictionaryValue>
ExtensionApiUnittest::RunFunctionAndReturnDictionary(
    ExtensionFunction* function,
    const std::string& args) {
  base::Value* value = RunFunctionAndReturnValue(function, args).release();
  base::DictionaryValue* dict = nullptr;

  if (value && !value->GetAsDictionary(&dict))
    delete value;

  // We expect to either have successfuly retrieved a dictionary from the value,
  // or the value to have been NULL.
  EXPECT_TRUE(dict || !value);
  return std::unique_ptr<base::DictionaryValue>(dict);
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
