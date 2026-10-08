// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/devtools_availability_checker.h"

#include <optional>
#include <string>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/notreached.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "chrome/browser/devtools/features.h"
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
#include "extensions/buildflags/buildflags.h"
#include "url/gurl.h"
#include "url/url_constants.h"

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/process_manager.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest.h"
#endif

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_tab_helper.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "components/webapps/isolated_web_apps/scheme.h"
#endif

#if BUILDFLAG(IS_CHROMEOS)
#include "chromeos/constants/pref_names.h"
#endif

namespace {

using Availability = policy::DeveloperToolsAvailability;
using UrlAvailability =
    policy::DeveloperToolsPolicyChecker::DevToolsAvailability;

// Returns why the general DeveloperToolsAvailability policy blocks DevTools
// for a context that is neither an extension nor a web app, or kNotBlocked.
DevToolsBlockReason GetGeneralPolicyBlockReason(Profile* profile) {
  switch (
      policy::DeveloperToolsPolicyHandler::GetEffectiveAvailability(profile)) {
    case Availability::kDisallowed:
      return DevToolsBlockReason::kPolicyDisallowed;
    case Availability::kAllowed:
    case Availability::kDisallowedForForceInstalledExtensions:
      break;
  }
#if BUILDFLAG(IS_CHROMEOS)
  // On ChromeOS disable dev tools for captive portal signin windows to prevent
  // them from being used for general navigation.
  const PrefService::Preference* const captive_portal_pref =
      profile->GetPrefs()->FindPreference(
          chromeos::prefs::kCaptivePortalSignin);
  if (captive_portal_pref && captive_portal_pref->GetValue()->GetBool()) {
    return DevToolsBlockReason::kCaptivePortalSignin;
  }
#endif
  return DevToolsBlockReason::kNotBlocked;
}

// Evaluates the URL-based DeveloperToolsAvailabilityAllowlist and
// DeveloperToolsAvailabilityBlocklist policies for |url|. Returns nullopt if
// they have no rule for |url|, in which case callers fall back to the general
// enum-based policy.
std::optional<DevToolsBlockReason> GetUrlPoliciesBlockReason(Profile* profile,
                                                             const GURL& url) {
  policy::DeveloperToolsPolicyChecker* checker =
      policy::DeveloperToolsPolicyCheckerFactory::GetForBrowserContext(profile);
  if (!checker) {
    return std::nullopt;
  }
  switch (checker->GetDevToolsAvailabilityForUrl(url)) {
    case UrlAvailability::kAllowed:
      return DevToolsBlockReason::kNotBlocked;
    case UrlAvailability::kDisallowed:
      return DevToolsBlockReason::kUrlAllowlistOrBlocklist;
    case UrlAvailability::kNotSet:
      return std::nullopt;
  }
  NOTREACHED();
}

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
// Returns why |extension| is restricted under the default
// DeveloperToolsAvailability value (DisallowedForForceInstalledExtensions),
// or kNotBlocked.
DevToolsBlockReason GetRestrictedExtensionBlockReason(
    const extensions::Extension* extension,
    Profile* profile) {
  if (!extension) {
    return DevToolsBlockReason::kNotBlocked;
  }
  if (extensions::Manifest::IsPolicyLocation(extension->location())) {
    return DevToolsBlockReason::kForceInstalledExtension;
  }
  if (extensions::Manifest::IsComponentLocation(extension->location()) &&
      profile->GetProfilePolicyConnector()->IsManaged()) {
    return DevToolsBlockReason::kComponentExtensionInManagedProfile;
  }
  return DevToolsBlockReason::kNotBlocked;
}
#endif

}  // namespace

DevToolsBlockReason GetDevToolsBlockReason(
    Profile* profile,
    content::DevToolsAgentHost* agent_host) {
  if (base::FeatureList::IsEnabled(features::kDevToolsTargetLevelEvaluation)) {
    GURL target_url = agent_host->GetURL();
    DevToolsBlockReason reason = GetDevToolsBlockReason(profile, target_url);
    if (reason != DevToolsBlockReason::kNotBlocked) {
      return reason;
    }

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
    // Enforce extension policy at the target level before falling back to the
    // parent WebContents.
    if (target_url.SchemeIs(extensions::kExtensionScheme)) {
      if (auto* registry = extensions::ExtensionRegistry::Get(profile)) {
        if (const extensions::Extension* extension =
                registry->GetInstalledExtension(
                    std::string(target_url.host()))) {
          reason = GetDevToolsBlockReason(profile, extension);
          if (reason != DevToolsBlockReason::kNotBlocked) {
            return reason;
          }
        }
      }
    }
#endif

    if (content::WebContents* web_contents = agent_host->GetWebContents()) {
      return GetDevToolsBlockReason(profile, web_contents);
    }
    return DevToolsBlockReason::kNotBlocked;
  }

  if (content::WebContents* web_contents = agent_host->GetWebContents()) {
    return GetDevToolsBlockReason(profile, web_contents);
  }
  return GetDevToolsBlockReason(profile, agent_host->GetURL());
}

DevToolsBlockReason GetDevToolsBlockReason(Profile* profile,
                                           content::WebContents* web_contents) {
  if (!web_contents) {
    // For contexts without web_contents, we can only check the general policy.
    return GetDevToolsBlockReason(
        profile, static_cast<const extensions::Extension*>(nullptr));
  }

  if (base::FeatureList::IsEnabled(features::kDevToolsTargetLevelEvaluation)) {
    if (content::RenderFrameHost* main_frame =
            web_contents->GetPrimaryMainFrame()) {
      DevToolsBlockReason reason =
          GetDevToolsBlockReason(profile, main_frame->GetLastCommittedURL());
      if (reason != DevToolsBlockReason::kNotBlocked) {
        return reason;
      }
    }
  } else {
    policy::DeveloperToolsPolicyChecker* checker =
        policy::DeveloperToolsPolicyCheckerFactory::GetForBrowserContext(
            profile);
    if (checker) {
      if (content::RenderFrameHost* main_frame =
              web_contents->GetPrimaryMainFrame()) {
        using FrameIterationAction =
            content::RenderFrameHost::FrameIterationAction;
        bool is_blocked = false;
        main_frame->ForEachRenderFrameHostWithAction(
            [&](content::RenderFrameHost* frame) {
              if (frame->GetLastCommittedURL().is_empty() ||
                  frame->GetLastCommittedURL().SchemeIs(url::kAboutScheme)) {
                return FrameIterationAction::kContinue;
              }
              auto frame_availability = checker->GetDevToolsAvailabilityForUrl(
                  frame->GetLastCommittedURL());
              if (frame_availability == UrlAvailability::kDisallowed) {
                is_blocked = true;
                return FrameIterationAction::kStop;
              }
              return FrameIterationAction::kContinue;
            });
        if (is_blocked) {
          return DevToolsBlockReason::kUrlAllowlistOrBlocklist;
        }
      }
    }
  }

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  if (auto* process_manager =
          extensions::ProcessManager::Get(web_contents->GetBrowserContext())) {
    if (const extensions::Extension* extension =
            process_manager->GetExtensionForWebContents(web_contents)) {
      return GetDevToolsBlockReason(profile, extension);
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
          return GetDevToolsBlockReason(profile, web_app);
        }
      }
    }
  }
#endif

  // Evaluate both the visible URL and the last committed URL and enforce
  // whichever is more restrictive. Checking only the visible URL would allow
  // a page to spoof its URL via a pending navigation, while checking only the
  // committed URL lags behind the visible URL during BFCache transitions.
  const std::optional<DevToolsBlockReason> visible_reason =
      GetUrlPoliciesBlockReason(profile, web_contents->GetURL());
  const std::optional<DevToolsBlockReason> committed_reason =
      GetUrlPoliciesBlockReason(profile, web_contents->GetLastCommittedURL());
  // Restrictiveness order: blocked > no rule (nullopt) > allowed. Having no
  // rule falls back to the general policy, which may still disallow DevTools,
  // so it is more restrictive than an explicit kNotBlocked.
  if (visible_reason == DevToolsBlockReason::kUrlAllowlistOrBlocklist ||
      committed_reason == DevToolsBlockReason::kUrlAllowlistOrBlocklist) {
    return DevToolsBlockReason::kUrlAllowlistOrBlocklist;
  }
  if (visible_reason == DevToolsBlockReason::kNotBlocked &&
      committed_reason == DevToolsBlockReason::kNotBlocked) {
    return DevToolsBlockReason::kNotBlocked;
  }

  // Fall back to the general enum policy for the tab context.
  return GetGeneralPolicyBlockReason(profile);
}

DevToolsBlockReason GetDevToolsBlockReason(
    Profile* profile,
    const extensions::Extension* extension) {
#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  if (extension) {
    if (std::optional<DevToolsBlockReason> reason =
            GetUrlPoliciesBlockReason(profile, extension->url())) {
      return *reason;
    }
  }
#endif

  if (!extension) {
    DevToolsBlockReason reason = GetGeneralPolicyBlockReason(profile);
    // When no specific context is given, we can't check for exceptions
    // like the allowlist. But if the allowlist is not empty, we should
    // allow the DevTools UI to load its resources, so it can be used for
    // allowlisted contexts.
    if (reason != DevToolsBlockReason::kNotBlocked &&
        !profile->GetPrefs()
             ->GetList(prefs::kDeveloperToolsAvailabilityAllowlist)
             .empty()) {
      return DevToolsBlockReason::kNotBlocked;
    }
    return reason;
  }

  switch (
      policy::DeveloperToolsPolicyHandler::GetEffectiveAvailability(profile)) {
    case Availability::kDisallowed:
      return DevToolsBlockReason::kPolicyDisallowed;
    case Availability::kAllowed:
      return DevToolsBlockReason::kNotBlocked;
    case Availability::kDisallowedForForceInstalledExtensions:
      // This policy only restricts extensions and web apps. Regular pages are
      // allowed.
#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
      return GetRestrictedExtensionBlockReason(extension, profile);
#else
      return DevToolsBlockReason::kNotBlocked;
#endif
  }
  NOTREACHED();
}

#if !BUILDFLAG(IS_ANDROID)
DevToolsBlockReason GetDevToolsBlockReason(Profile* profile,
                                           const web_app::WebApp* web_app) {
  if (web_app) {
    if (std::optional<DevToolsBlockReason> reason =
            GetUrlPoliciesBlockReason(profile, web_app->start_url())) {
      return *reason;
    }
  }
  switch (
      policy::DeveloperToolsPolicyHandler::GetEffectiveAvailability(profile)) {
    case Availability::kDisallowed:
      return DevToolsBlockReason::kPolicyDisallowed;
    case Availability::kAllowed:
      return DevToolsBlockReason::kNotBlocked;
    case Availability::kDisallowedForForceInstalledExtensions:
      // DevTools should be blocked for Kiosk apps and policy-installed IWAs.
      if (web_app) {
        if (web_app->IsKioskInstalledApp()) {
          return DevToolsBlockReason::kKioskWebApp;
        }
        if (web_app->IsIwaPolicyInstalledApp()) {
          return DevToolsBlockReason::kPolicyInstalledIsolatedWebApp;
        }
      }
      return DevToolsBlockReason::kNotBlocked;
  }
  NOTREACHED();
}
#endif

DevToolsBlockReason GetDevToolsBlockReason(Profile* profile, const GURL& url) {
  if (url.is_empty() || url.SchemeIs(url::kAboutScheme)) {
    return DevToolsBlockReason::kNotBlocked;
  }
#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  if (url.SchemeIs(extensions::kExtensionScheme)) {
    if (const extensions::Extension* extension =
            extensions::ExtensionRegistry::Get(profile)->GetExtensionById(
                std::string(url.host()),
                extensions::ExtensionRegistry::EVERYTHING)) {
      return GetDevToolsBlockReason(profile, extension);
    }
  }
#endif

#if !BUILDFLAG(IS_ANDROID)
  if (url.SchemeIs(webapps::kIsolatedAppScheme) &&
      web_app::AreWebAppsEnabled(profile)) {
    base::expected<web_app::IsolatedWebAppUrlInfo, std::string> url_info =
        web_app::IsolatedWebAppUrlInfo::Create(url);
    if (url_info.has_value()) {
      if (auto* web_app_provider =
              web_app::WebAppProvider::GetForWebApps(profile)) {
        if (const web_app::WebApp* web_app =
                web_app_provider->registrar_unsafe().GetAppById(
                    url_info->app_id())) {
          return GetDevToolsBlockReason(profile, web_app);
        }
      }
    }
  }
#endif

  if (std::optional<DevToolsBlockReason> reason =
          GetUrlPoliciesBlockReason(profile, url)) {
    return *reason;
  }
  // If the URL-based policy doesn't have a rule for this URL, we fall back to
  // the general enum-based policy.
  return GetGeneralPolicyBlockReason(profile);
}

bool IsInspectionAllowed(Profile* profile,
                         content::DevToolsAgentHost* agent_host) {
  return GetDevToolsBlockReason(profile, agent_host) ==
         DevToolsBlockReason::kNotBlocked;
}

bool IsInspectionAllowed(Profile* profile, content::WebContents* web_contents) {
  return GetDevToolsBlockReason(profile, web_contents) ==
         DevToolsBlockReason::kNotBlocked;
}

bool IsInspectionAllowed(Profile* profile,
                         const extensions::Extension* extension) {
  return GetDevToolsBlockReason(profile, extension) ==
         DevToolsBlockReason::kNotBlocked;
}

#if !BUILDFLAG(IS_ANDROID)
bool IsInspectionAllowed(Profile* profile, const web_app::WebApp* web_app) {
  return GetDevToolsBlockReason(profile, web_app) ==
         DevToolsBlockReason::kNotBlocked;
}
#endif

bool IsInspectionAllowed(Profile* profile, const GURL& url) {
  return GetDevToolsBlockReason(profile, url) ==
         DevToolsBlockReason::kNotBlocked;
}
