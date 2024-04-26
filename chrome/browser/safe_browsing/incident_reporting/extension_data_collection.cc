// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/incident_reporting/extension_data_collection.h"

#include "base/containers/contains.h"
#include "base/json/json_string_value_serializer.h"
#include "base/time/time.h"
#include "base/version.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/chrome_content_browser_client_extensions_part.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/extensions/install_signer.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/safe_browsing/incident_reporting/incident_reporting_service.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_prefs_factory.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_factory.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/manifest_constants.h"

namespace safe_browsing {

namespace {

// Helper function to extract information from extension and extension_prefs
// into extension_info.
void PopulateExtensionInfo(
    const extensions::Extension& extension,
    const extensions::ExtensionPrefs& extension_prefs,
    const extensions::ExtensionRegistry& extension_registry,
    ClientIncidentReport_ExtensionData_ExtensionInfo* extension_info) {
  std::string extension_id = extension.id();
  extension_info->set_id(extension_id);
  extension_info->set_version(extension.version().GetString());
  extension_info->set_name(extension.name());
  extension_info->set_description(extension.description());

  typedef ClientIncidentReport_ExtensionData_ExtensionInfo Info;
  if (extension_registry.enabled_extensions().Contains(extension_id))
    extension_info->set_state(Info::STATE_ENABLED);
  else if (extension_registry.disabled_extensions().Contains(extension_id))
    extension_info->set_state(Info::STATE_DISABLED);
  else if (extension_registry.blocklisted_extensions().Contains(extension_id))
    extension_info->set_state(Info::STATE_BLOCKLISTED);
  else if (extension_registry.blocked_extensions().Contains(extension_id))
    extension_info->set_state(Info::STATE_BLOCKED);
  else if (extension_registry.terminated_extensions().Contains(extension_id))
    extension_info->set_state(Info::STATE_TERMINATED);

  extension_info->set_type(extension.GetType());
  if (const std::string* update_url = extension.manifest()->FindStringPath(
          extensions::manifest_keys::kUpdateURL)) {
    extension_info->set_update_url(*update_url);
  }

  extension_info->set_installed_by_default(
      extension.was_installed_by_default());
  extension_info->set_installed_by_oem(extension.was_installed_by_oem());
  // TODO(crbug.com/40124309): Remove this setter.
  extension_info->set_from_bookmark(false);
  extension_info->set_from_webstore(extension.from_webstore());
  extension_info->set_converted_from_user_script(
      extension.converted_from_user_script());
  extension_info->set_may_be_untrusted(extension.may_be_untrusted());
  extension_info->set_install_time_msec(
      extension_prefs.GetLastUpdateTime(extension.id())
          .InMillisecondsSinceUnixEpoch());

  std::unique_ptr<extensions::InstallSignature> signature_from_prefs =
      extensions::InstallSignature::FromDict(
          extension_prefs.GetInstallSignature());
  if (signature_from_prefs) {
    if (base::Contains(signature_from_prefs->ids, extension_id)) {
      extension_info->set_has_signature_validation(true);
      extension_info->set_signature_is_valid(true);
    } else if (base::Contains(signature_from_prefs->invalid_ids,
                              extension_id)) {
      extension_info->set_has_signature_validation(true);
      extension_info->set_signature_is_valid(false);
    }
  }

  std::string manifest_json;
  JSONStringValueSerializer serializer(&manifest_json);
  if (serializer.Serialize(*extension.manifest()->value()))
    extension_info->mutable_manifest()->swap(manifest_json);

  extension_info->set_manifest_location_type(
      static_cast<int>(extension.manifest()->location()));
}

}  // namespace

// Finds the last installed extension and adds relevant information to data's
// last_installed_extension field.
void CollectExtensionData(ClientIncidentReport_ExtensionData* data) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  scoped_refptr<const extensions::Extension> last_installed_extension;
  Profile* profile_for_last_installed_extension = nullptr;
  base::Time last_install_time;

  for (Profile* profile :
       g_browser_process->profile_manager()->GetLoadedProfiles()) {
    // Skip profiles for which the incident reporting service is not enabled.
    //
    // Some profiles cannot have extensions, such as the System Profile.
    if (!IncidentReportingService::IsEnabledForProfile(profile) ||
        extensions::ChromeContentBrowserClientExtensionsPart::
            AreExtensionsDisabledForProfile(profile)) {
      continue;
    }

    const extensions::ExtensionSet extensions =
        extensions::ExtensionRegistryFactory::GetForBrowserContext(profile)
            ->GenerateInstalledExtensionsSet();
    extensions::ExtensionPrefs* extension_prefs =
        extensions::ExtensionPrefsFactory::GetForBrowserContext(profile);
    for (const auto& extension : extensions) {
      base::Time install_time =
          extension_prefs->GetLastUpdateTime(extension->id());
      if (install_time > last_install_time) {
        last_install_time = install_time;
        last_installed_extension = extension;
        profile_for_last_installed_extension = profile;
      }
    }
  }

  if (last_installed_extension && profile_for_last_installed_extension) {
    PopulateExtensionInfo(
        *last_installed_extension,
        *extensions::ExtensionPrefs::Get(profile_for_last_installed_extension),
        *extensions::ExtensionRegistryFactory::GetForBrowserContext(
            profile_for_last_installed_extension),
        data->mutable_last_installed_extension());
  }
#endif
}

}  // namespace safe_browsing
