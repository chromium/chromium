// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_INFO_PRIVATE_API_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_INFO_PRIVATE_API_H_

#include <string>

#include "base/compiler_specific.h"
#include "extensions/browser/extension_function.h"

namespace base {
class Value;
}

namespace extensions {

class ChromeosInfoPrivateGetFunction : public ExtensionFunction {
 public:
  ChromeosInfoPrivateGetFunction();

 protected:
  ~ChromeosInfoPrivateGetFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  // Returns a newly allocate value, or null.
  std::unique_ptr<base::Value> GetValue(const std::string& property_name);

  DECLARE_EXTENSION_FUNCTION("chromeosInfoPrivate.get", CHROMEOSINFOPRIVATE_GET)
};

class ChromeosInfoPrivateSetFunction : public ExtensionFunction {
 public:
  ChromeosInfoPrivateSetFunction();

 protected:
  ~ChromeosInfoPrivateSetFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  DECLARE_EXTENSION_FUNCTION("chromeosInfoPrivate.set", CHROMEOSINFOPRIVATE_SET)
};

}  // namespace extensions

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_INFO_PRIVATE_API_H_
