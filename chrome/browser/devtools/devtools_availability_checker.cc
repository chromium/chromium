// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/devtools_availability_checker.h"

#include "chrome/browser/policy/developer_tools_policy_handler.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/process_manager.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_tab_helper.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#endif

#if BUILDFLAG(IS_CHROMEOS)
#include "chromeos/constants/pref_names.h"
#include "components/prefs/pref_service.h"
#endif

namespace {

policy::DeveloperToolsPolicyHandler::Availability GetDevToolsAvailability(
    Profile* profile) {
  using Availability = policy::DeveloperToolsPolicyHandler::Availability;
  Availability availability =
      policy::DeveloperToolsPolicyHandler::GetEffectiveAvailability(profile);
#if BUILDFLAG(IS_CHROMEOS)
  // On ChromeOS disable dev tools for captive portal signin windows to prevent
  // them from being used for general navigation.
  if (availability != Availability::kDisallowed) {
    const PrefService::Preference* const captive_portal_pref =
        profile->GetPrefs()->FindPreference(
            chromeos::prefs::kCaptivePortalSignin);
    if (captive_portal_pref && captive_portal_pref->GetValue()->GetBool()) {
      availability = Availability::kDisallowed;
    }
  }
#endif
  return availability;
}

}  // namespace

bool IsInspectionAllowed(Profile* profile, content::WebContents* web_contents) {
  const extensions::Extension* extension = nullptr;
  if (web_contents) {
    if (auto* process_manager = extensions::ProcessManager::Get(
            web_contents->GetBrowserContext())) {
      extension = process_manager->GetExtensionForWebContents(web_contents);
    }
    if (extension) {
      return IsInspectionAllowed(profile, extension);
    }
#if !BUILDFLAG(IS_ANDROID)
    if (!web_app::AreWebAppsEnabled(profile)) {
      return IsInspectionAllowed(profile, extension);
    }
    const webapps::AppId* app_id =
        web_app::WebAppTabHelper::GetAppId(web_contents);
    auto* web_app_provider =
        web_app::WebAppProvider::GetForWebContents(web_contents);
    if (app_id && web_app_provider) {
      const web_app::WebApp* web_app =
          web_app_provider->registrar_unsafe().GetAppById(*app_id);
      return IsInspectionAllowed(profile, web_app);
    }
#endif
  }
  // |extension| is always nullptr here.
  return IsInspectionAllowed(profile, extension);
}

bool IsInspectionAllowed(Profile* profile,
                         const extensions::Extension* extension) {
  using Availability = policy::DeveloperToolsPolicyHandler::Availability;
  Availability availability;
  if (extension) {
    availability =
        policy::DeveloperToolsPolicyHandler::GetEffectiveAvailability(profile);
  } else {
    // Perform additional checks for browser windows (extension == null).
    availability = GetDevToolsAvailability(profile);
  }
  switch (availability) {
    case Availability::kDisallowed:
      return false;
    case Availability::kAllowed:
      return true;
    case Availability::kDisallowedForForceInstalledExtensions:
      if (!extension) {
        return true;
      }
      if (extensions::Manifest::IsPolicyLocation(extension->location())) {
        return false;
      }
      // We also disallow inspecting component extensions, but only for managed
      // profiles.
      if (extensions::Manifest::IsComponentLocation(extension->location()) &&
          profile->GetProfilePolicyConnector()->IsManaged()) {
        return false;
      }
      return true;
    default:
      NOTREACHED() << "Unknown developer tools policy";
  }
}

#if !BUILDFLAG(IS_ANDROID)
bool IsInspectionAllowed(Profile* profile, const web_app::WebApp* web_app) {
  using Availability = policy::DeveloperToolsPolicyHandler::Availability;
  Availability availability =
      policy::DeveloperToolsPolicyHandler::GetEffectiveAvailability(profile);
  switch (availability) {
    case Availability::kDisallowed:
      return false;
    case Availability::kAllowed:
      return true;
    case Availability::kDisallowedForForceInstalledExtensions: {
      if (!web_app) {
        return true;
      }
      // DevTools should be blocked for Kiosk apps and policy-installed IWAs.
      if (web_app->IsKioskInstalledApp() ||
          web_app->IsIwaPolicyInstalledApp()) {
        return false;
      }
      return true;
    }
    default:
      NOTREACHED() << "Unknown developer tools policy";
  }
}
#endif
