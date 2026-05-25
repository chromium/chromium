// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_ui_util.h"

#include "base/check.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/extension_constants.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/url_formatter/elide_url.h"
#include "components/url_formatter/url_formatter.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/ui_util.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/image_util.h"
#include "extensions/common/manifest_handlers/app_display_info.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace extensions {

namespace {

bool IsBlockedByPolicy(const Extension* app, content::BrowserContext* context) {
  Profile* profile = Profile::FromBrowserContext(context);
  DCHECK(profile);

  return app->id() == extensions::kWebStoreAppId &&
         profile->GetPrefs()->GetBoolean(
             policy::policy_prefs::kHideWebStoreIcon);
}

}  // namespace

namespace ui_util {

bool ShouldDisplayInAppLauncher(const Extension* extension,
                                content::BrowserContext* context) {
  return CanDisplayInAppLauncher(extension, context);
}

bool CanDisplayInAppLauncher(const Extension* extension,
                             content::BrowserContext* context) {
  return AppDisplayInfo::ShouldDisplayInAppLauncher(*extension) &&
         !IsBlockedByPolicy(extension, context);
}

bool ShouldDisplayInNewTabPage(const Extension* extension,
                               content::BrowserContext* context) {
  return AppDisplayInfo::ShouldDisplayInNewTabPage(*extension) &&
         !IsBlockedByPolicy(extension, context);
}

// Two paths are checked, in order:
//  1. If `url` uses the chrome-extension:// scheme, returns the name of
//     the extension identified by the URL's host.
//  2. Otherwise, consults
//     `extensions::ui_util::GetTopLevelMimeHandlerExtension(web_contents)`
//     to identify a generic MIME handler extension rendering the primary
//     main frame. The MIME-handler branch only runs on platforms where
//     the extensions/browser/mime_handler target is built (non-Android).
std::u16string GetEnabledExtensionNameForUrl(
    const GURL& url,
    content::WebContents& web_contents) {
  const Extension* extension = nullptr;
  // This branch also covers the case where an extension serves a resource
  // (e.g., a bundled PDF) from its own chrome-extension:// URL: the URL host
  // is the extension ID, so the extension's own name is shown and the
  // MIME-handler branch below does not apply.
  if (url.SchemeIs(kExtensionScheme)) {
    auto* registry = ExtensionRegistry::Get(web_contents.GetBrowserContext());
    extension = registry ? registry->enabled_extensions().GetByID(url.GetHost())
                         : nullptr;
#if !BUILDFLAG(IS_ANDROID)
  } else if (web_contents.GetLastCommittedURL() == url) {
    // Only match when the location-bar URL equals the frame's last committed
    // URL: during navigation the location bar can show a pending URL that
    // doesn't yet reflect the committed content, so we skip the MIME-handler
    // check to avoid misidentifying the extension in that transient state.
    extension = GetTopLevelMimeHandlerExtension(web_contents);
#endif
  }
  return extension ? base::CollapseWhitespace(
                         base::UTF8ToUTF16(extension->name()), false)
                   : std::u16string();
}

bool HasManageableExtensions(content::BrowserContext* browser_context) {
  auto* const registry = extensions::ExtensionRegistry::Get(browser_context);
  if (!registry) {
    return false;
  }

  // This logic mirrors the logic used to determine which extensions are
  // displayed on the "manage extensions" page (chrome://extensions) - see
  // `DeveloperPrivateGetExtensionsInfoFunction`.
  const auto has_manageable_extension =
      [](const extensions::ExtensionSet& extensions) {
        for (const auto& extension : extensions) {
          if (ShouldDisplayInExtensionSettings(*extension)) {
            return true;
          }
        }
        return false;
      };

  return has_manageable_extension(registry->enabled_extensions()) ||
         has_manageable_extension(registry->disabled_extensions()) ||
         has_manageable_extension(registry->terminated_extensions()) ||
         has_manageable_extension(registry->blocklisted_extensions());
}

std::u16string GetFormattedHostForDisplay(content::WebContents& web_contents) {
  auto url = web_contents.GetLastCommittedURL();
  // Hide the scheme when necessary (e.g hide "https://" but don't
  // "chrome://").
  return url_formatter::FormatUrlForDisplayOmitSchemePathAndTrivialSubdomains(
      url);
}

}  // namespace ui_util
}  // namespace extensions
