// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <string>
#include <vector>

#include "chrome/browser/enterprise/connectors/connectors_prefs.h"
#include "chrome/browser/enterprise/connectors/connectors_service.h"
#include "chrome/browser/enterprise/connectors/file_system/access_token_fetcher.h"
#include "chrome/browser/enterprise/connectors/service_provider_config.h"

#include "components/prefs/pref_registry_simple.h"

namespace enterprise_connectors {

const char kSendDownloadToCloudPref[] =
    "enterprise_connectors.send_download_to_cloud";

const char kOnFileAttachedPref[] = "enterprise_connectors.on_file_attached";

const char kOnFileDownloadedPref[] = "enterprise_connectors.on_file_downloaded";

const char kOnBulkDataEntryPref[] = "enterprise_connectors.on_bulk_data_entry";

const char kOnSecurityEventPref[] = "enterprise_connectors.on_security_event";

const char kContextAwareAccessSignalsAllowlistPref[] =
    "enterprise_connectors.device_trust.origins";
const char kDeviceTrustPrivateKeyPref[] =
    "enterprise_connectors.device_trust.private_key";
const char kDeviceTrustPublicKeyPref[] =
    "enterprise_connectors.device_trust.public_key";

const char kOnFileAttachedScopePref[] =
    "enterprise_connectors.scope.on_file_attached";
const char kOnFileDownloadedScopePref[] =
    "enterprise_connectors.scope.on_file_downloaded";
const char kOnBulkDataEntryScopePref[] =
    "enterprise_connectors.scope.on_bulk_data_entry";
const char kOnSecurityEventScopePref[] =
    "enterprise_connectors.scope.on_security_event";

// Template to store the Box folder_id for caching purposes
constexpr char kFileSystemUploadFolderIdPref[] =
    "enterprise_connectors.file_system.box.folder_id";

namespace {

void RegisterFileSystemPrefs(PrefRegistrySimple* registry) {
  std::vector<std::string> all_service_providers =
      GetServiceProviderConfig()->GetServiceProviderNames();
  std::vector<std::string> fs_service_providers;
  std::copy_if(all_service_providers.begin(), all_service_providers.end(),
               std::back_inserter(fs_service_providers), [](const auto& name) {
                 const ServiceProviderConfig::ServiceProvider* provider =
                     GetServiceProviderConfig()->GetServiceProvider(name);
                 return !provider->fs_home_url().empty();
               });

  for (const auto& name : fs_service_providers) {
    RegisterFileSystemPrefsForServiceProvider(registry, name);
  }
}

}  // namespace

void RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterListPref(kSendDownloadToCloudPref);
  registry->RegisterListPref(kOnFileAttachedPref);
  registry->RegisterListPref(kOnFileDownloadedPref);
  registry->RegisterListPref(kOnBulkDataEntryPref);
  registry->RegisterListPref(kOnSecurityEventPref);
  registry->RegisterIntegerPref(kOnFileAttachedScopePref, 0);
  registry->RegisterIntegerPref(kOnFileDownloadedScopePref, 0);
  registry->RegisterIntegerPref(kOnBulkDataEntryScopePref, 0);
  registry->RegisterIntegerPref(kOnSecurityEventScopePref, 0);
  registry->RegisterListPref(kContextAwareAccessSignalsAllowlistPref);

  RegisterFileSystemPrefs(registry);
}

void RegisterLocalPrefs(PrefRegistrySimple* registry) {
  registry->RegisterStringPref(kDeviceTrustPrivateKeyPref, std::string());
  registry->RegisterStringPref(kDeviceTrustPublicKeyPref, std::string());
}

}  // namespace enterprise_connectors
