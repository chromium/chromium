// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/privacy_sandbox/notice/desktop_entrypoint_handlers_helper.h"

#include "chrome/browser/search/search.h"
#include "chrome/common/webui_url_constants.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/manifest_handlers/chrome_url_overrides_handler.h"

namespace {

#if BUILDFLAG(IS_CHROMEOS)
bool HasExtensionNtpOverride(
    extensions::ExtensionRegistry* extension_registry) {
  for (const auto& extension : extension_registry->enabled_extensions()) {
    const auto& overrides =
        extensions::URLOverrides::GetChromeURLOverrides(extension.get());
    if (overrides.find(chrome::kChromeUINewTabHost) != overrides.end()) {
      return true;
    }
  }
  return false;
}

bool IsChromeControlledNtpUrl(const GURL& url) {
  // Convert to origins for comparison, as any appended paths are irrelevant.
  const auto ntp_origin = url::Origin::Create(url);

  return ntp_origin ==
             url::Origin::Create(GURL(chrome::kChromeUINewTabPageURL)) ||
         ntp_origin == url::Origin::Create(
                           GURL(chrome::kChromeUINewTabPageThirdPartyURL));
}
#endif  // BUILDFLAG(IS_CHROMEOS)

}  // namespace

namespace privacy_sandbox {
// static
bool IsUrlSuitableForPrompt(const GURL& url) {
  // The prompt should be shown on a limited list of pages:
  // about:blank is valid.
  if (url.IsAboutBlank()) {
    return true;
  }
  // Chrome settings page is valid. The subpages aren't as most of them are not
  // related to the prompt.
  if (url == GURL(chrome::kChromeUISettingsURL)) {
    return true;
  }
  // Chrome history is valid as the prompt mentions history.
  if (url == GURL(chrome::kChromeUIHistoryURL)) {
    return true;
  }
  // Only a Chrome controlled New Tab Page is valid. Third party NTP is still
  // Chrome controlled, but is without Google branding.
  if (url == GURL(chrome::kChromeUINewTabPageURL) ||
      url == GURL(chrome::kChromeUINewTabPageThirdPartyURL)) {
    return true;
  }
  return false;
}

#if BUILDFLAG(IS_CHROMEOS)
// static
void MaybeOpenAboutBlankOnChrome(content::NavigationHandle* navigation_handle,
                                 Profile* profile,
                                 content::WebContents* web_contents) {
  if (web_contents->GetLastCommittedURL() == GURL(chrome::kChromeUINewTabURL)) {
    const bool has_extention_override =
        HasExtensionNtpOverride(extensions::ExtensionRegistry::Get(profile));

    const GURL new_tab_page = search::GetNewTabPageURL(profile);
    const bool is_non_chrome_controlled_ntp =
        navigation_handle->GetURL() == new_tab_page &&
        !IsChromeControlledNtpUrl(new_tab_page);

    if (has_extention_override || is_non_chrome_controlled_ntp) {
      web_contents->OpenURL(
          content::OpenURLParams(GURL(url::kAboutBlankURL), content::Referrer(),
                                 WindowOpenDisposition::NEW_FOREGROUND_TAB,
                                 ui::PAGE_TRANSITION_AUTO_TOPLEVEL,
                                 /*is_renderer_initiated=*/false),
          /*navigation_handle_callback=*/{});
      return;
    }
  }
}
#endif  // BUILDFLAG(IS_CHROMEOS)

}  // namespace privacy_sandbox
