// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_INFO_PRIVATE_INFO_PRIVATE_API_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_INFO_PRIVATE_INFO_PRIVATE_API_H_

#include <string>

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
  void RespondWithResult(base::Value result);

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
  void RespondWithResult(bool found);
  std::string param_name_;

  DECLARE_EXTENSION_FUNCTION("chromeosInfoPrivate.set", CHROMEOSINFOPRIVATE_SET)
};

// API function that is called to get the tablet mode enabled status as a
// boolean.
class ChromeosInfoPrivateIsTabletModeEnabledFunction
    : public ExtensionFunction {
 public:
  ChromeosInfoPrivateIsTabletModeEnabledFunction();

 protected:
  ~ChromeosInfoPrivateIsTabletModeEnabledFunction() override;
  ResponseAction Run() override;

 private:
  void RespondWithResult(bool enabled);

  DECLARE_EXTENSION_FUNCTION("chromeosInfoPrivate.isTabletModeEnabled",
                             CHROMEOSINFOPRIVATE_ISTABLETMODEENABLED)
};

// API function that is called to return the lacros enabled status as a
// boolean.
// TODO(337089191): Deprecate this function after Lacros migration is completed.
class ChromeosInfoPrivateIsRunningOnLacrosFunction : public ExtensionFunction {
 public:
  ChromeosInfoPrivateIsRunningOnLacrosFunction();

 protected:
  ~ChromeosInfoPrivateIsRunningOnLacrosFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  DECLARE_EXTENSION_FUNCTION("chromeosInfoPrivate.isRunningOnLacros",
                             CHROMEOSINFOPRIVATE_ISRUNNINGONLACROS)
};

}  // namespace extensions

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_INFO_PRIVATE_INFO_PRIVATE_API_H_
