// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/external_component_loader.h"

#include "base/memory/scoped_refptr.h"
#include "base/values.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/component_extensions_allowlist/allowlist.h"
#include "chrome/browser/extensions/forced_extensions/install_stage_tracker.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/extensions/extension_constants.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_urls.h"
#include "extensions/common/feature_switch.h"
#include "extensions/common/manifest.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/chromeos/upload_office_to_cloud/upload_office_to_cloud.h"
#include "chrome/browser/extensions/forced_extensions/assessment_assistant_tracker.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chromeos/constants/chromeos_features.h"
#endif

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace extensions {

ExternalComponentLoader::ExternalComponentLoader(Profile* profile)
    : profile_(profile) {}

ExternalComponentLoader::~ExternalComponentLoader() = default;

void ExternalComponentLoader::StartLoading() {
  auto prefs = base::Value::Dict();
  // Skip in-app payments app on Android. crbug.com/409396604
#if BUILDFLAG(GOOGLE_CHROME_BRANDING) && !BUILDFLAG(IS_ANDROID)
  AddExternalExtension(extension_misc::kInAppPaymentsSupportAppId, prefs);
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING) && !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS)
  {
    // Only load the Assessment Assistant if the current session is managed.
    if (profile_->GetProfilePolicyConnector()->IsManaged()) {
      // TODO(http://crbug.com/297415232): Remove the following observer
      // registrations once the bug is fixed.
      AssessmentAssistantTrackerFactory::GetInstance()->GetForBrowserContext(
          profile_);

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
