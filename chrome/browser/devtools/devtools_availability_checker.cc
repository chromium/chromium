// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/devtools_availability_checker.h"

#include "chrome/browser/policy/developer_tools_policy_checker.h"
#include "chrome/browser/policy/developer_tools_policy_checker_factory.h"
#include "chrome/browser/policy/developer_tools_policy_handler.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
#include "extensions/browser/extension_host.h"
#include "extensions/browser/process_manager.h"
#endif

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
  if (!web_contents) {
    // For contexts without web_contents, we can only check the general policy.
    return IsInspectionAllowed(
        profile, static_cast<const extensions::Extension*>(nullptr));
  }
  policy::DeveloperToolsPolicyChecker* checker =
      policy::DeveloperToolsPolicyCheckerFactory::GetForBrowserContext(profile);
  if (checker) {
    if (auto url_check =
            checker->CheckDevToolsAvailabilityForUrl(web_contents->GetURL())) {
      return *url_check;
    }
  }
  // Now check the enum policy
  using Availability = policy::DeveloperToolsPolicyHandler::Availability;
  Availability availability = GetDevToolsAvailability(profile);
  switch (availability) {
    case Availability::kDisallowed:
      return false;
    case Availability::kAllowed:
      return true;
    case Availability::kDisallowedForForceInstalledExtensions: {
// This policy only restricts extensions and web apps. Regular pages are
// allowed.
#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
      const extensions::Extension* extension = nullptr;
      if (auto* process_manager = extensions::ProcessManager::Get(
              web_contents->GetBrowserContext())) {
        extension = process_manager->GetExtensionForWebContents(web_contents);
      }
      if (extension) {
        if (extensions::Manifest::IsPolicyLocation(extension->location()) ||
            (extensions::Manifest::IsComponentLocation(extension->location()) &&
             profile->GetProfilePolicyConnector()->IsManaged())) {
          return false;
        }
      }
#endif
#if !BUILDFLAG(IS_ANDROID)
      if (web_app::AreWebAppsEnabled(profile)) {
        const webapps::AppId* app_id =
            web_app::WebAppTabHelper::GetAppId(web_contents);
        auto* web_app_provider =
            web_app::WebAppProvider::GetForWebContents(web_contents);
        if (app_id && web_app_provider) {
          const web_app::WebApp* web_app =
              web_app_provider->registrar_unsafe().GetAppById(*app_id);
          if (web_app && (web_app->IsKioskInstalledApp() ||
                          web_app->IsIwaPolicyInstalledApp())) {
            return false;
          }
        }
      }
#endif
      // If it's not a restricted extension or web app, it's allowed.
      return true;
    }
    default:
      NOTREACHED();
  }
}

bool IsInspectionAllowed(Profile* profile,
                         const extensions::Extension* extension) {
#if !BUILDFLAG(IS_ANDROID)
  if (extension) {
    policy::DeveloperToolsPolicyChecker* checker =
        policy::DeveloperToolsPolicyCheckerFactory::GetForBrowserContext(
            profile);
    if (auto url_check =
            checker->CheckDevToolsAvailabilityForUrl(extension->url())) {
      return *url_check;
    }
  }
#endif
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
      if (!extension) {
        // When no specific context is given, we can't check for exceptions
        // like the allowlist. But if the allowlist is not empty, we should
        // allow the DevTools UI to load its resources, so it can be used for
        // allowlisted contexts.
        const base::Value::List& allowlist = profile->GetPrefs()->GetList(
            prefs::kDeveloperToolsAvailabilityAllowlist);
        if (!allowlist.empty()) {
          return true;
        }
      }
      return false;
    case Availability::kAllowed:
      return true;
    case Availability::kDisallowedForForceInstalledExtensions:
#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
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
#endif
      return true;
    default:
      NOTREACHED() << "Unknown developer tools policy";
  }
}

#if !BUILDFLAG(IS_ANDROID)
bool IsInspectionAllowed(Profile* profile, const web_app::WebApp* web_app) {
  if (web_app) {
    policy::DeveloperToolsPolicyChecker* checker =
        policy::DeveloperToolsPolicyCheckerFactory::GetForBrowserContext(
            profile);
    if (auto url_check =
            checker->CheckDevToolsAvailabilityForUrl(web_app->start_url())) {
      return *url_check;
    }
  }
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
