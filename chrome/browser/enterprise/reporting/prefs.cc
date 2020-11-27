// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/reporting/prefs.h"

#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/common/pref_names.h"
#include "components/enterprise/browser/reporting/common_pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_registry_simple.h"

namespace enterprise_reporting {

// The browser version that performed the most recent report upload.
const char kLastUploadVersion[] = "enterprise_reporting.last_upload_version";

void RegisterLocalStatePrefs(PrefRegistrySimple* registry) {
  // This is also registered as a Profile pref which will be removed after
  // the migration.
  registry->RegisterBooleanPref(kCloudReportingEnabled, false);
  registry->RegisterTimePref(kLastUploadTimestamp, base::Time());
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  registry->RegisterStringPref(kLastUploadVersion, std::string());
#endif
}

void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(prefs::kCloudExtensionRequestEnabled, false);
  registry->RegisterDictionaryPref(prefs::kCloudExtensionRequestIds);
}

}  // namespace enterprise_reporting
