// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_ENTERPRISE_HARDWARE_PLATFORM_ENTERPRISE_HARDWARE_PLATFORM_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_ENTERPRISE_HARDWARE_PLATFORM_ENTERPRISE_HARDWARE_PLATFORM_API_H_

#include "base/macros.h"
#include "base/system/sys_info.h"
#include "extensions/browser/extension_function.h"

namespace extensions {

class EnterpriseHardwarePlatformGetHardwarePlatformInfoFunction
    : public ExtensionFunction {
 public:
  EnterpriseHardwarePlatformGetHardwarePlatformInfoFunction();

 protected:
  ~EnterpriseHardwarePlatformGetHardwarePlatformInfoFunction() override;

  ResponseAction Run() override;

 private:
  DECLARE_EXTENSION_FUNCTION(
      "enterprise.hardwarePlatform.getHardwarePlatformInfo",
      ENTERPRISE_HARDWAREPLATFORM_GETHARDWAREPLATFORMINFO)

  void OnHardwarePlatformInfo(base::SysInfo::HardwareInfo info);

  DISALLOW_COPY_AND_ASSIGN(
      EnterpriseHardwarePlatformGetHardwarePlatformInfoFunction);
};

}  //  namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_ENTERPRISE_HARDWARE_PLATFORM_ENTERPRISE_HARDWARE_PLATFORM_API_H_
