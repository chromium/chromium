// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_API_UNITTEST_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_API_UNITTEST_H_

#include <memory>
#include <string>
#include <utility>

#include "base/memory/ref_counted.h"
#include "base/values.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class ExtensionFunction;

namespace extensions {

// Use this class to enable calling API functions in a unittest.
// By default, this class will create and load an empty unpacked |extension_|,
// which will be used in all API function calls. This extension can be
// overridden using set_extension().
// By default, this class does not create a WebContents for the API functions.
// If a WebContents is needed, calling CreateBackgroundPage() will create a
// background page for the extension and use it in API function calls. (If
// needed, this could be expanded to allow for alternate WebContents).
// When calling RunFunction[AndReturn*], |args| should be in JSON format,
// wrapped in a list. See also RunFunction* in extension_function_test_utils.h.
// TODO(yoz): Move users of this base class to use the equivalent base class
// in extensions/browser/api_unittest.h.
class ExtensionApiUnittest : public BrowserWithTestWindowTest {
 public:
  template <typename... TaskEnvironmentTraits>
  explicit ExtensionApiUnittest(TaskEnvironmentTraits&&... traits)
      : BrowserWithTestWindowTest(
            std::forward<TaskEnvironmentTraits>(traits)...) {}
  ~ExtensionApiUnittest() override;

  const Extension* extension() const { return extension_.get(); }
  scoped_refptr<const Extension> extension_ref() { return extension_; }
  void set_extension(scoped_refptr<const Extension> extension) {
    extension_ = extension;
  }

 protected:
  // SetUp creates and loads an empty, unpacked Extension.
  void SetUp() override;

  // Various ways of running an API function. These methods take ownership of
  // |function|. |args| should be in JSON format, wrapped in a list.
  // See also the RunFunction* methods in extension_function_test_utils.h.

  // Return the function result as a base::Value.
  std::unique_ptr<base::Value> RunFunctionAndReturnValue(
      ExtensionFunction* function,
      const std::string& args);

  // Return the function result as a base::Value::Dict, or absl::nullopt.
  // This will EXPECT-fail if the result is not a base::Value::Dict.
  absl::optional<base::Value::Dict> RunFunctionAndReturnDictionary(
      ExtensionFunction* function,
      const std::string& args);

  // Return the function result as a base::Value, or NULL.
  // This will EXPECT-fail if the result is not a list.
  std::unique_ptr<base::Value> RunFunctionAndReturnList(
      ExtensionFunction* function,
      const std::string& args);

  // Return an error thrown from the function, if one exists.
  // This will EXPECT-fail if any result is returned from the function.
  std::string RunFunctionAndReturnError(ExtensionFunction* function,
                                        const std::string& args);

  // Run the function and ignore any result.
  void RunFunction(ExtensionFunction* function, const std::string& args);

 private:
  // The Extension used when running API function calls.
  scoped_refptr<const Extension> extension_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_API_UNITTEST_H_
