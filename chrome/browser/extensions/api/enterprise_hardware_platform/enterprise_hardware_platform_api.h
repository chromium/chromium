// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_ENTERPRISE_HARDWARE_PLATFORM_ENTERPRISE_HARDWARE_PLATFORM_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_ENTERPRISE_HARDWARE_PLATFORM_ENTERPRISE_HARDWARE_PLATFORM_API_H_

#include "base/system/sys_info.h"
#include "extensions/browser/extension_function.h"

namespace extensions {

class EnterpriseHardwarePlatformGetHardwarePlatformInfoFunction
    : public ExtensionFunction {
 public:
  EnterpriseHardwarePlatformGetHardwarePlatformInfoFunction();

  EnterpriseHardwarePlatformGetHardwarePlatformInfoFunction(
      const EnterpriseHardwarePlatformGetHardwarePlatformInfoFunction&) =
      delete;
  EnterpriseHardwarePlatformGetHardwarePlatformInfoFunction& operator=(
      const EnterpriseHardwarePlatformGetHardwarePlatformInfoFunction&) =
      delete;

 protected:
  ~EnterpriseHardwarePlatformGetHardwarePlatformInfoFunction() override;

  ResponseAction Run() override;

 private:
  DECLARE_EXTENSION_FUNCTION(
      "enterprise.hardwarePlatform.getHardwarePlatformInfo",
      ENTERPRISE_HARDWAREPLATFORM_GETHARDWAREPLATFORMINFO)

  void OnHardwarePlatformInfo(base::SysInfo::HardwareInfo info);
};

}  //  namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_ENTERPRISE_HARDWARE_PLATFORM_ENTERPRISE_HARDWARE_PLATFORM_API_H_
