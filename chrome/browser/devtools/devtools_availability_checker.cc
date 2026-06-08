// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/devtools_availability_checker.h"

#include <string>

#include "base/check.h"
#include "base/notreached.h"
#include "base/values.h"
#include "chrome/browser/policy/developer_tools_policy_checker.h"
#include "chrome/browser/policy/developer_tools_policy_checker_factory.h"
#include "chrome/browser/policy/developer_tools_policy_handler.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/process_manager.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest.h"
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
#endif

namespace {

policy::DeveloperToolsAvailability GetDevToolsAvailability(
    Profile* profile) {
  using Availability = policy::DeveloperToolsAvailability;
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

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
bool IsRestrictedExtension(const extensions::Extension* extension,
                           Profile* profile) {
  if (!extension) {
    return false;
  }
  if (extensions::Manifest::IsPolicyLocation(extension->location())) {
    return true;
  }
  // We also disallow inspecting component extensions, but only for managed
  // profiles.
  if (extensions::Manifest::IsComponentLocation(extension->location()) &&
      profile->GetProfilePolicyConnector()->IsManaged()) {
    return true;
  }
  return false;
}
#endif

}  // namespace

bool IsInspectionAllowed(Profile* profile,
                         content::DevToolsAgentHost* agent_host) {
  if (content::WebContents* web_contents = agent_host->GetWebContents()) {
    return IsInspectionAllowed(profile, web_contents);
  }
  return IsInspectionAllowed(profile, agent_host->GetURL());
}

bool IsInspectionAllowed(Profile* profile, content::WebContents* web_contents) {
  if (!web_contents) {
    // For contexts without web_contents, we can only check the general policy.
    return IsInspectionAllowed(
        profile, static_cast<const extensions::Extension*>(nullptr));
  }

  policy::DeveloperToolsPolicyChecker* checker =
      policy::DeveloperToolsPolicyCheckerFactory::GetForBrowserContext(profile);
  if (checker) {
    bool is_blocked = false;
    web_contents->ForEachRenderFrameHost([&](content::RenderFrameHost* frame) {
      auto frame_availability =
          checker->GetDevToolsAvailabilityForUrl(frame->GetLastCommittedURL());
      if (frame_availability == policy::DeveloperToolsPolicyChecker::
                                    DevToolsAvailability::kDisallowed) {
        is_blocked = true;
      }
    });
    if (is_blocked) {
      return false;
    }
  }

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  if (auto* process_manager =
          extensions::ProcessManager::Get(web_contents->GetBrowserContext())) {
    if (const extensions::Extension* extension =
            process_manager->GetExtensionForWebContents(web_contents)) {
      return IsInspectionAllowed(profile, extension);
    }
  }
#endif

#if !BUILDFLAG(IS_ANDROID)
  if (web_app::AreWebAppsEnabled(profile)) {
    if (const webapps::AppId* app_id =
            web_app::WebAppTabHelper::GetAppId(web_contents)) {
      if (auto* web_app_provider =
              web_app::WebAppProvider::GetForWebContents(web_contents)) {
        if (const web_app::WebApp* web_app =
                web_app_provider->registrar_unsafe().GetAppById(*app_id)) {
          return IsInspectionAllowed(profile, web_app);
        }
      }
    }
  }
#endif

  if (checker) {
    auto url_availability =
        checker->GetDevToolsAvailabilityForUrl(web_contents->GetURL());
    switch (url_availability) {
      case policy::DeveloperToolsPolicyChecker::DevToolsAvailability::kAllowed:
        return true;
      case policy::DeveloperToolsPolicyChecker::DevToolsAvailability::
          kDisallowed:
        return false;
      case policy::DeveloperToolsPolicyChecker::DevToolsAvailability::kNotSet:
        break;
    }
  }

  // Exhaustively check every frame to prevent subframe bypasses
  // and identify restricted extensions even on error pages.
  bool is_blocked = false;
  web_contents->ForEachRenderFrameHostWithAction(
      [&](content::RenderFrameHost* frame) {
        if (!IsInspectionAllowed(profile, frame->GetLastCommittedURL())) {
          is_blocked = true;
          return content::RenderFrameHost::FrameIterationAction::kStop;
        }
        return content::RenderFrameHost::FrameIterationAction::kContinue;
      });

  if (is_blocked) {
    return false;
  }

  // Fall back to the general enum policy for the tab context.
  using Availability = policy::DeveloperToolsAvailability;
  Availability availability = GetDevToolsAvailability(profile);
  switch (availability) {
    case Availability::kDisallowed:
      return false;
    case Availability::kAllowed:
      return true;
    case Availability::kDisallowedForForceInstalledExtensions:
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
      return true;
    default:
      NOTREACHED() << "Unknown developer tools policy";
  }
}

bool IsInspectionAllowed(Profile* profile,
                         const extensions::Extension* extension) {
#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  policy::DeveloperToolsPolicyChecker* checker =
      policy::DeveloperToolsPolicyCheckerFactory::GetForBrowserContext(profile);
  if (checker && extension) {
    auto url_availability =
        checker->GetDevToolsAvailabilityForUrl(extension->url());
    switch (url_availability) {
      case policy::DeveloperToolsPolicyChecker::DevToolsAvailability::kAllowed:
        return true;
      case policy::DeveloperToolsPolicyChecker::DevToolsAvailability::
          kDisallowed:
        return false;
      case policy::DeveloperToolsPolicyChecker::DevToolsAvailability::kNotSet:
        // The URL is not covered by the URL-based policies, so we fall back to
        // the general enum-based policy.
        break;
    }
  }
#endif

  using Availability = policy::DeveloperToolsAvailability;
  Availability availability =
      extension ? policy::DeveloperToolsPolicyHandler::GetEffectiveAvailability(
                      profile)
                : GetDevToolsAvailability(profile);

  switch (availability) {
    case Availability::kDisallowed:
      if (!extension) {
        // When no specific context is given, we can't check for exceptions
        // like the allowlist. But if the allowlist is not empty, we should
        // allow the DevTools UI to load its resources, so it can be used for
        // allowlisted contexts.
        const base::ListValue& allowlist = profile->GetPrefs()->GetList(
            prefs::kDeveloperToolsAvailabilityAllowlist);
        if (!allowlist.empty()) {
          return true;
        }
      }
      return false;
    case Availability::kAllowed:
      return true;
    case Availability::kDisallowedForForceInstalledExtensions:
// This policy only restricts extensions and web apps. Regular pages are
// allowed.
#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
      if (IsRestrictedExtension(extension, profile)) {
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
    auto url_availability =
        checker->GetDevToolsAvailabilityForUrl(web_app->start_url());
    switch (url_availability) {
      case policy::DeveloperToolsPolicyChecker::DevToolsAvailability::kAllowed:
        return true;
      case policy::DeveloperToolsPolicyChecker::DevToolsAvailability::
          kDisallowed:
        return false;
      case policy::DeveloperToolsPolicyChecker::DevToolsAvailability::kNotSet:
        // The URL is not covered by the URL-based policies, so we fall back to
        // the general enum-based policy.
        break;
    }
  }
  using Availability = policy::DeveloperToolsAvailability;
  Availability availability =
      policy::DeveloperToolsPolicyHandler::GetEffectiveAvailability(profile);
  switch (availability) {
    case Availability::kDisallowed:
      return false;
    case Availability::kAllowed:
      return true;
    case Availability::kDisallowedForForceInstalledExtensions:
      // DevTools should be blocked for Kiosk apps and policy-installed IWAs.
      if (web_app && (web_app->IsKioskInstalledApp() ||
                      web_app->IsIwaPolicyInstalledApp())) {
        return false;
      }
      return true;
    default:
      NOTREACHED() << "Unknown developer tools policy";
  }
}
#endif

bool IsInspectionAllowed(Profile* profile, const GURL& url) {
#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  if (url.SchemeIs(extensions::kExtensionScheme)) {
    if (const extensions::Extension* extension =
            extensions::ExtensionRegistry::Get(profile)->GetExtensionById(
                std::string(url.host()),
                extensions::ExtensionRegistry::EVERYTHING)) {
      return IsInspectionAllowed(profile, extension);
    }
  }
#endif

  policy::DeveloperToolsPolicyChecker* checker =
      policy::DeveloperToolsPolicyCheckerFactory::GetForBrowserContext(profile);
  if (checker) {
    auto url_availability = checker->GetDevToolsAvailabilityForUrl(url);
    switch (url_availability) {
      case policy::DeveloperToolsPolicyChecker::DevToolsAvailability::kAllowed:
        return true;
      case policy::DeveloperToolsPolicyChecker::DevToolsAvailability::
          kDisallowed:
        return false;
      case policy::DeveloperToolsPolicyChecker::DevToolsAvailability::kNotSet:
        break;
    }
  }

  using Availability = policy::DeveloperToolsAvailability;
  Availability availability = GetDevToolsAvailability(profile);
  switch (availability) {
    case Availability::kDisallowed:
      return false;
    case Availability::kAllowed:
    case Availability::kDisallowedForForceInstalledExtensions:
      return true;
    default:
      NOTREACHED() << "Unknown developer tools policy";
  }
}
