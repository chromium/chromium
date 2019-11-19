// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_FUNCTION_TEST_UTILS_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_FUNCTION_TEST_UTILS_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "extensions/browser/api_test_utils.h"
#include "extensions/common/manifest.h"

class Browser;
class ExtensionFunction;

namespace base {
class Value;
class DictionaryValue;
class ListValue;
}

// TODO(ckehoe): Accept args as std::unique_ptr<base::Value>,
// and migrate existing users to the new API.
// This file is DEPRECATED. New tests should use the versions in
// extensions/browser/api_test_utils.h.
namespace extension_function_test_utils {

// Parse JSON and return as a ListValue, or null if invalid.
base::ListValue* ParseList(const std::string& data);

// If |val| is a dictionary, return it as one, otherwise NULL.
base::DictionaryValue* ToDictionary(base::Value* val);

// If |val| is a list, return it as one, otherwise NULL.
base::ListValue* ToList(base::Value* val);

// Returns true if |val| contains any privacy information, e.g. url,
// pendingUrl, title or faviconUrl.
bool HasAnyPrivacySensitiveFields(base::DictionaryValue* val);

// Run |function| with |args| and return the resulting error. Adds an error to
// the current test if |function| returns a result. Takes ownership of
// |function|.
std::string RunFunctionAndReturnError(
    ExtensionFunction* function,
    const std::string& args,
    Browser* browser,
    extensions::api_test_utils::RunFunctionFlags flags);
std::string RunFunctionAndReturnError(ExtensionFunction* function,
                                      const std::string& args,
                                      Browser* browser);

// Run |function| with |args| and return the result. Adds an error to the
// current test if |function| returns an error. Takes ownership of
// |function|. The caller takes ownership of the result.
base::Value* RunFunctionAndReturnSingleResult(
    ExtensionFunction* function,
    const std::string& args,
    Browser* browser,
    extensions::api_test_utils::RunFunctionFlags flags);
base::Value* RunFunctionAndReturnSingleResult(ExtensionFunction* function,
                                              const std::string& args,
                                              Browser* browser);

// Create and run |function| with |args|. Works with both synchronous and async
// functions. Ownership of |function| remains with the caller.
//
// TODO(aa): It would be nice if |args| could be validated against the schema
// that |function| expects. That way, we know that we are testing something
// close to what the bindings would actually send.
//
// TODO(aa): I'm concerned that this style won't scale to all the bits and bobs
// we're going to need to frob for all the different extension functions. But
// we can refactor when we see what is needed.
bool RunFunction(ExtensionFunction* function,
                 const std::string& args,
                 Browser* browser,
                 extensions::api_test_utils::RunFunctionFlags flags);
bool RunFunction(ExtensionFunction* function,
                 std::unique_ptr<base::ListValue> args,
                 Browser* browser,
                 extensions::api_test_utils::RunFunctionFlags flags);

} // namespace extension_function_test_utils

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_FUNCTION_TEST_UTILS_H_
