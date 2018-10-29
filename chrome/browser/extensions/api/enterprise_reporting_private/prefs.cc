// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/enterprise_reporting_private/prefs.h"

#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"

namespace extensions {
namespace enterprise_reporting {

const char kReportVersionData[] = "enterprise_reporting.report_version_data";

const char kReportPolicyData[] = "enterprise_reporting.report_policy_data";

const char kReportMachineIDData[] =
    "enterprise_reporting.report_machine_id_data";

const char kReportUserIDData[] = "enterprise_reporting.report_user_id_data";

void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(kReportVersionData, true);
  registry->RegisterBooleanPref(kReportPolicyData, true);
  registry->RegisterBooleanPref(kReportMachineIDData, true);
  registry->RegisterBooleanPref(kReportUserIDData, true);
  registry->RegisterBooleanPref(prefs::kCloudReportingEnabled, false);
}

}  // namespace enterprise_reporting
}  // namespace extensions
