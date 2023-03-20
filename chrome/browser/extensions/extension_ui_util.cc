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
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_util.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/image_util.h"
#include "extensions/common/manifest_handlers/app_display_info.h"

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
      extension_registry->enabled_extensions().GetByID(url.host());
  return extension ? base::CollapseWhitespace(
                         base::UTF8ToUTF16(extension->name()), false)
                   : std::u16string();
}

}  // namespace ui_util
}  // namespace extensions
