// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/external_component_loader.h"

#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/component_extensions_whitelist/whitelist.h"
#include "chrome/browser/media/router/media_router_feature.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/extensions/extension_constants.h"
#include "components/signin/core/browser/signin_manager.h"
#include "extensions/common/extension_urls.h"
#include "extensions/common/feature_switch.h"
#include "extensions/common/manifest.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/policy/profile_policy_connector_factory.h"
#include "chromeos/chromeos_switches.h"
#endif

namespace extensions {

ExternalComponentLoader::ExternalComponentLoader(Profile* profile)
    : profile_(profile) {
}

ExternalComponentLoader::~ExternalComponentLoader() {}

void ExternalComponentLoader::StartLoading() {
  auto prefs = std::make_unique<base::DictionaryValue>();
#if defined(GOOGLE_CHROME_BUILD)
  AddExternalExtension(extension_misc::kInAppPaymentsSupportAppId, prefs.get());
#endif  // defined(GOOGLE_CHROME_BUILD)

#if defined(OS_CHROMEOS)
  {
    // Only load the Assessment Assistant if the current session is managed.
    if (policy::ProfilePolicyConnectorFactory::IsProfileManaged(profile_))
      AddExternalExtension(extension_misc::kAssessmentAssistantExtensionId,
                           prefs.get());
  }
#endif

  if (media_router::MediaRouterEnabled(profile_) &&
      FeatureSwitch::load_media_router_component_extension()->IsEnabled()) {
    AddExternalExtension(extension_misc::kMediaRouterStableExtensionId,
                         prefs.get());
  }

  LoadFinished(std::move(prefs));
}

void ExternalComponentLoader::AddExternalExtension(
    const std::string& extension_id,
    base::DictionaryValue* prefs) {
  if (!IsComponentExtensionWhitelisted(extension_id))
    return;

  prefs->SetString(extension_id + ".external_update_url",
                   extension_urls::GetWebstoreUpdateUrl().spec());
}

}  // namespace extensions
