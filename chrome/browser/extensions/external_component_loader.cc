// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/external_component_loader.h"

#include "base/values.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/component_extensions_allowlist/allowlist.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/extensions/extension_constants.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_urls.h"
#include "extensions/common/feature_switch.h"
#include "extensions/common/manifest.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/chromeos/upload_office_to_cloud/upload_office_to_cloud.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chromeos/constants/chromeos_features.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/crosapi/browser_util.h"
#endif

namespace extensions {

ExternalComponentLoader::ExternalComponentLoader(Profile* profile)
    : profile_(profile) {}

ExternalComponentLoader::~ExternalComponentLoader() {}

void ExternalComponentLoader::StartLoading() {
  auto prefs = base::Value::Dict();
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  AddExternalExtension(extension_misc::kInAppPaymentsSupportAppId, prefs);
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

#if BUILDFLAG(IS_CHROMEOS)
  {
    // Only load the Assessment Assistant if the current session is managed.
    if (profile_->GetProfilePolicyConnector()->IsManaged()) {
      AddExternalExtension(extension_misc::kAssessmentAssistantExtensionId,
                           prefs);
    }

    if (chromeos::cloud_upload::IsMicrosoftOfficeOneDriveIntegrationAllowed(
            profile_)) {
      // Do not load in Ash if Lacros is enabled, otherwise all messages will be
      // routed to the extension in Ash.
      bool should_load = false;
#if BUILDFLAG(IS_CHROMEOS_ASH)
      // In Ash, suppress loading if Lacros is enabled, as the extension is
      // expected to be loaded in Lacros.
      should_load = !crosapi::browser_util::IsLacrosEnabled();
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
      // In Lacros, only load in the primary profile (fileSystemProvider
      // extensions in other profiles won't work).
      should_load = profile_ == ProfileManager::GetPrimaryUserProfile();
#endif
      if (should_load) {
        AddExternalExtension(extension_misc::kODFSExtensionId, prefs);
      }
    }
  }
#endif

  LoadFinished(std::move(prefs));
}

void ExternalComponentLoader::AddExternalExtension(
    const std::string& extension_id,
    base::Value::Dict& prefs) {
  if (!IsComponentExtensionAllowlisted(extension_id))
    return;

  prefs.SetByDottedPath(extension_id + ".external_update_url",
                        extension_urls::GetWebstoreUpdateUrl().spec());
}

}  // namespace extensions
