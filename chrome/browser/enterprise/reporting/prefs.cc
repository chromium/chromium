// Copyright 2019 The Chromium Authors
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
// The list of requests that have been uploaded to the server.
const char kCloudExtensionRequestUploadedIds[] =
    "enterprise_reporting.extension_request.pending.ids";

const char kCloudLegacyTechReportAllowlist[] =
    "enterprise_reporting.legacy_tech.urls";

const base::TimeDelta kDefaultReportFrequency = base::Hours(24);

void RegisterLocalStatePrefs(PrefRegistrySimple* registry) {
  // This is also registered as a Profile pref which will be removed after
  // the migration.
  registry->RegisterBooleanPref(kCloudReportingEnabled, false);
  registry->RegisterTimePref(kLastUploadTimestamp, base::Time());
  registry->RegisterTimePref(kLastUploadSucceededTimestamp, base::Time());
  registry->RegisterStringPref(kLastUploadVersion, std::string());
  registry->RegisterTimeDeltaPref(kCloudReportingUploadFrequency,
                                  kDefaultReportFrequency);
}

void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(kCloudProfileReportingEnabled, false);
  registry->RegisterTimePref(kLastUploadTimestamp, base::Time());
  registry->RegisterTimePref(kLastUploadSucceededTimestamp, base::Time());
  registry->RegisterStringPref(kLastUploadVersion, std::string());
  // TODO(crbug.com/40215470): We reuse the report frequency pref for profile
  // reporting for now. This might need to be changed in the future.
  registry->RegisterTimeDeltaPref(kCloudReportingUploadFrequency,
                                  kDefaultReportFrequency);
#if BUILDFLAG(ENABLE_EXTENSIONS)
  registry->RegisterBooleanPref(prefs::kCloudExtensionRequestEnabled, false);
  registry->RegisterDictionaryPref(prefs::kCloudExtensionRequestIds);
  registry->RegisterDictionaryPref(kCloudExtensionRequestUploadedIds);
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

  registry->RegisterListPref(kCloudLegacyTechReportAllowlist);
}

}  // namespace enterprise_reporting
