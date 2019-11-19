// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_CHROME_EXTENSION_FUNCTION_H_
#define CHROME_BROWSER_EXTENSIONS_CHROME_EXTENSION_FUNCTION_H_

#include "base/macros.h"
#include "chrome/browser/extensions/chrome_extension_function_details.h"
#include "extensions/browser/extension_function.h"

class Profile;

// A chrome specific analog to AsyncExtensionFunction. This has access to a
// chrome Profile.
//
// DEPRECATED: Inherit directly from ExtensionFunction.
// Then if you need access to Chrome details, you can construct a
// ChromeExtensionFunctionDetails object within your function implementation.
class ChromeAsyncExtensionFunction : public ExtensionFunction {
 public:
  ChromeAsyncExtensionFunction();

  Profile* GetProfile() const;

  void SetError(const std::string& error);

  const std::string& GetError() const override;

 protected:
  ~ChromeAsyncExtensionFunction() override;

  // Deprecated. See class comments.
  virtual bool RunAsync() = 0;

  // ValidationFailure override to match RunAsync().
  static bool ValidationFailure(ChromeAsyncExtensionFunction* function);

  // Responds with success/failure. |results_| or |error_| should be set
  // accordingly.
  void SendResponse(bool success);

  // Sets a single Value as the results of the function.
  void SetResult(std::unique_ptr<base::Value> result);

  // Sets multiple Values as the results of the function.
  void SetResultList(std::unique_ptr<base::ListValue> results);

  // Exposed versions of ExtensionFunction::results_ and
  // ExtensionFunction::error_ that are curried into the response.
  // These need to keep the same name to avoid breaking existing
  // implementations, but this should be temporary with crbug.com/648275
  // and crbug.com/634140.
  std::unique_ptr<base::ListValue> results_;
  std::string error_;

 private:
  // If you're hitting a compile error here due to "final" - great! You're doing
  // the right thing, you just need to extend ExtensionFunction instead
  // of ChromeAsyncExtensionFunction.
  ResponseAction Run() final;

  ChromeExtensionFunctionDetails chrome_details_;

  DISALLOW_COPY_AND_ASSIGN(ChromeAsyncExtensionFunction);
};

#endif  // CHROME_BROWSER_EXTENSIONS_CHROME_EXTENSION_FUNCTION_H_
