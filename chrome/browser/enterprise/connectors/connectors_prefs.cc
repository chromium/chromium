// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/connectors_prefs.h"

#include "components/prefs/pref_registry_simple.h"

namespace enterprise_connectors {

const char kOnFileAttachedPref[] = "enterprise_connectors.on_file_attached";

const char kOnFileDownloadedPref[] = "enterprise_connectors.on_file_downloaded";

const char kOnBulkDataEntryPref[] = "enterprise_connectors.on_bulk_data_entry";

const char kOnSecurityEventPref[] = "enterprise_connectors.on_security_event";

const char kOnFileAttachedScopePref[] =
    "enterprise_connectors.scope.on_file_attached";
const char kOnFileDownloadedScopePref[] =
    "enterprise_connectors.scope.on_file_downloaded";
const char kOnBulkDataEntryScopePref[] =
    "enterprise_connectors.scope.on_bulk_data_entry";
const char kOnSecurityEventScopePref[] =
    "enterprise_connectors.scope.on_security_event";

void RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterListPref(kOnFileAttachedPref);
  registry->RegisterListPref(kOnFileDownloadedPref);
  registry->RegisterListPref(kOnBulkDataEntryPref);
  registry->RegisterListPref(kOnSecurityEventPref);
  registry->RegisterIntegerPref(kOnFileAttachedScopePref, 0);
  registry->RegisterIntegerPref(kOnFileDownloadedScopePref, 0);
  registry->RegisterIntegerPref(kOnBulkDataEntryScopePref, 0);
  registry->RegisterIntegerPref(kOnSecurityEventScopePref, 0);
}

}  // namespace enterprise_connectors
