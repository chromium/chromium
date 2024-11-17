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
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/chromeos/upload_office_to_cloud/upload_office_to_cloud.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chromeos/constants/chromeos_features.h"
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
      AddExternalExtension(extension_misc::kODFSExtensionId, prefs);
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
