// Copyright 2014 The Chromium Authors. All rights reserved.
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

ExtensionApiUnittest::ExtensionApiUnittest() {
}

ExtensionApiUnittest::~ExtensionApiUnittest() {
}

void ExtensionApiUnittest::SetUp() {
  BrowserWithTestWindowTest::SetUp();
  extension_ = ExtensionBuilder("Test").Build();
}

std::unique_ptr<base::Value> ExtensionApiUnittest::RunFunctionAndReturnValue(
    UIThreadExtensionFunction* function,
    const std::string& args) {
  function->set_extension(extension());
  return std::unique_ptr<base::Value>(
      utils::RunFunctionAndReturnSingleResult(function, args, browser()));
}

std::unique_ptr<base::DictionaryValue>
ExtensionApiUnittest::RunFunctionAndReturnDictionary(
    UIThreadExtensionFunction* function,
    const std::string& args) {
  base::Value* value = RunFunctionAndReturnValue(function, args).release();
  base::DictionaryValue* dict = NULL;

  if (value && !value->GetAsDictionary(&dict))
    delete value;

  // We expect to either have successfuly retrieved a dictionary from the value,
  // or the value to have been NULL.
  EXPECT_TRUE(dict || !value);
  return std::unique_ptr<base::DictionaryValue>(dict);
}

std::unique_ptr<base::ListValue> ExtensionApiUnittest::RunFunctionAndReturnList(
    UIThreadExtensionFunction* function,
    const std::string& args) {
  base::Value* value = RunFunctionAndReturnValue(function, args).release();
  base::ListValue* list = NULL;

  if (value && !value->GetAsList(&list))
    delete value;

  // We expect to either have successfuly retrieved a list from the value,
  // or the value to have been NULL.
  EXPECT_TRUE(list);
  return std::unique_ptr<base::ListValue>(list);
}

std::string ExtensionApiUnittest::RunFunctionAndReturnError(
    UIThreadExtensionFunction* function, const std::string& args) {
  function->set_extension(extension());
  return utils::RunFunctionAndReturnError(function, args, browser());
}

void ExtensionApiUnittest::RunFunction(
    UIThreadExtensionFunction* function, const std::string& args) {
  RunFunctionAndReturnValue(function, args);
}

}  // namespace extensions
