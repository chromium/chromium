// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/enterprise_hardware_platform/enterprise_hardware_platform_api.h"

#include <utility>

#include "base/functional/bind.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/enterprise_hardware_platform.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"

namespace extensions {

EnterpriseHardwarePlatformGetHardwarePlatformInfoFunction::
    EnterpriseHardwarePlatformGetHardwarePlatformInfoFunction() = default;

EnterpriseHardwarePlatformGetHardwarePlatformInfoFunction::
    ~EnterpriseHardwarePlatformGetHardwarePlatformInfoFunction() = default;

ExtensionFunction::ResponseAction
EnterpriseHardwarePlatformGetHardwarePlatformInfoFunction::Run() {
  Profile* profile = Profile::FromBrowserContext(browser_context());
  if (!profile->GetPrefs()->IsManagedPreference(
          prefs::kEnterpriseHardwarePlatformAPIEnabled) ||
      !profile->GetPrefs()->GetBoolean(
          prefs::kEnterpriseHardwarePlatformAPIEnabled)) {
    return RespondNow(Error("Not allowed"));
  }

  base::SysInfo::GetHardwareInfo(base::BindOnce(
      &EnterpriseHardwarePlatformGetHardwarePlatformInfoFunction::
          OnHardwarePlatformInfo,
      this));
  return RespondLater();
}

void EnterpriseHardwarePlatformGetHardwarePlatformInfoFunction::
    OnHardwarePlatformInfo(base::SysInfo::HardwareInfo info) {
  api::enterprise_hardware_platform::HardwarePlatformInfo result;
  result.manufacturer = std::move(info.manufacturer);
  result.model = std::move(info.model);
  Respond(ArgumentList(api::enterprise_hardware_platform::
                           GetHardwarePlatformInfo::Results::Create(result)));
}

}  // namespace extensions
