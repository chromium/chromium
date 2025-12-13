// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_ui_util.h"

#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/extension_constants.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/url_formatter/elide_url.h"
#include "components/url_formatter/url_formatter.h"
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

std::u16string GetEnabledExtensionNameForUrl(const GURL& url,
                                             content::BrowserContext* context) {
  if (!url.SchemeIs(extensions::kExtensionScheme))
    return std::u16string();

  extensions::ExtensionRegistry* extension_registry =
      extensions::ExtensionRegistry::Get(context);
  const extensions::Extension* extension =
      extension_registry->enabled_extensions().GetByID(url.GetHost());
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
