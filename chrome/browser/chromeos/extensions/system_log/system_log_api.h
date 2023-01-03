// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_SYSTEM_LOG_SYSTEM_LOG_API_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_SYSTEM_LOG_SYSTEM_LOG_API_H_

#include "extensions/browser/extension_function.h"
#include "extensions/browser/extension_function_histogram_value.h"

namespace extensions {

class SystemLogAddFunction : public ExtensionFunction {
 public:
  SystemLogAddFunction();

  SystemLogAddFunction(const SystemLogAddFunction&) = delete;

  SystemLogAddFunction& operator=(const SystemLogAddFunction&) = delete;

  DECLARE_EXTENSION_FUNCTION("systemLog.add", SYSTEMLOG_ADD)

 protected:
  ~SystemLogAddFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_SYSTEM_LOG_SYSTEM_LOG_API_H_
