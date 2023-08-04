// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/enterprise/incognito_navigation_throttle.h"

#include "base/i18n/message_formatter.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/browser_resources.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/webui/jstemplate_builder.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/chromeos/strings/grit/ui_chromeos_strings.h"

namespace chromeos {
namespace {
std::string GetIncognitoNavigationBlockedErrorPage(
    base::Value::List blocking_extension,
    base::Value::List missing_extension) {
  auto strings =
      base::Value::Dict()
          .Set("incognitoBlockedPageTitle",
               l10n_util::GetPluralStringFUTF16(
                   IDS_INCOGNITO_NAVIGATION_BLOCKED_PAGE_TITLE,
                   blocking_extension.size()))
          .Set("incognitoBlockedMessage",
               l10n_util::GetPluralStringFUTF16(
                   IDS_INCOGNITO_NAVIGATION_MESSAGE, blocking_extension.size()))
          .Set("incognitoBlockedInstructions",
               l10n_util::GetStringUTF16(IDS_INCOGNITO_NAVIGATION_INSTRUCTIONS))
          .Set("incognitoBlockedInstructionsStep1",
               l10n_util::GetStringUTF16(
                   IDS_INCOGNITO_NAVIGATION_INSTRUCTIONS_STEP_1))
          .Set("incognitoBlockedInstructionsStep2",
               l10n_util::GetStringUTF16(
                   IDS_INCOGNITO_NAVIGATION_INSTRUCTIONS_STEP_2))
          .Set("incognitoBlockedInstructionsStep3",
               l10n_util::GetStringUTF16(
                   IDS_INCOGNITO_NAVIGATION_INSTRUCTIONS_STEP_3))
          .Set("incognitoBlockedInstructionsStep4",
               l10n_util::GetStringUTF16(
                   IDS_INCOGNITO_NAVIGATION_INSTRUCTIONS_STEP_4))
          .Set("incognitoBlockedMissingExtensionsTitle",
               l10n_util::GetPluralStringFUTF16(
                   IDS_INCOGNITO_NAVIGATION_MISSING_TITLE,
                   missing_extension.size()))
          .Set("incognitoBlockedMissingExtensionsMessage",
               l10n_util::GetPluralStringFUTF16(
                   IDS_INCOGNITO_NAVIGATION_MISSING_EXTENSIONS,
                   missing_extension.size()))
          .Set("blockingExtensions", std::move(blocking_extension))
          .Set("missingExtensions", std::move(missing_extension));

  std::string html =
      ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
          IDR_INCOGNITO_NAVIGATION_BLOCKED_PAGE_HTML);
  const std::string& app_locale = g_browser_process->GetApplicationLocale();
  webui::SetLoadTimeDataDefaults(app_locale, &strings);

  return webui::GetI18nTemplateHtml(html, strings);
}

}  // namespace

// static
std::unique_ptr<IncognitoNavigationThrottle>
IncognitoNavigationThrottle::MaybeCreateThrottleFor(
    content::NavigationHandle* navigation_handle) {
  content::BrowserContext* browser_context =
      navigation_handle->GetWebContents()->GetBrowserContext();
  Profile* profile = Profile::FromBrowserContext(browser_context);
  if (!profile->IsIncognitoProfile()) {
    return nullptr;
  }
  const base::Value::List& mandatory_extensions = profile->GetPrefs()->GetList(
      prefs::kMandatoryExtensionsForIncognitoNavigation);
  if (mandatory_extensions.empty()) {
    return nullptr;
  }
  return std::make_unique<IncognitoNavigationThrottle>(navigation_handle,
                                                       profile);
}

IncognitoNavigationThrottle::IncognitoNavigationThrottle(
    content::NavigationHandle* navigation_handle,
    Profile* profile)
    : content::NavigationThrottle(navigation_handle), profile_(profile) {}

IncognitoNavigationThrottle::~IncognitoNavigationThrottle() = default;

content::NavigationThrottle::ThrottleCheckResult
IncognitoNavigationThrottle::WillStartRequest() {
  ReadMandatoryExtensionsStatus();
  if (blocking_extensions_.empty() && missing_extensions_.empty()) {
    return content::NavigationThrottle::PROCEED;
  }
  return ThrottleCheckResult(
      content::NavigationThrottle::CANCEL, net::ERR_BLOCKED_BY_ADMINISTRATOR,
      GetIncognitoNavigationBlockedErrorPage(std::move(blocking_extensions_),
                                             std::move(missing_extensions_)));
}

content::NavigationThrottle::ThrottleCheckResult
IncognitoNavigationThrottle::WillRedirectRequest() {
  return WillStartRequest();
}

const char* IncognitoNavigationThrottle::GetNameForLogging() {
  return "IncognitoNavigationThrottle";
}

void IncognitoNavigationThrottle::ReadMandatoryExtensionsStatus() {
  DCHECK(profile_);
  blocking_extensions_.clear();
  missing_extensions_.clear();
  extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(profile_);
  const base::Value::List& mandatory_extensions = profile_->GetPrefs()->GetList(
      prefs::kMandatoryExtensionsForIncognitoNavigation);
  if (mandatory_extensions.empty()) {
    return;
  }
  for (const auto& val : mandatory_extensions) {
    if (!val.is_string()) {
      continue;
    }
    const std::string id = val.GetString();
    auto* ext = registry->GetInstalledExtension(id);
    if (ext) {
      if (!extensions::util::IsIncognitoEnabled(id, profile_)) {
        blocking_extensions_.Append(ext->name());
      }
    } else {
      missing_extensions_.Append(id);
    }
  }
}

}  // namespace chromeos
