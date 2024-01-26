// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/search/search_api.h"

#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/common/extensions/api/search.h"
#include "components/search_engines/util.h"

namespace extensions {

namespace {

void NavigateToURL(WindowOpenDisposition disposition,
                   Browser* browser,
                   const GURL& url) {
  NavigateParams navigate_params(browser, url, ui::PAGE_TRANSITION_FROM_API);
  navigate_params.window_action = NavigateParams::SHOW_WINDOW;
  navigate_params.disposition = disposition;
  Navigate(&navigate_params);
}

}  // namespace

using extensions::api::search::Disposition;

ExtensionFunction::ResponseAction SearchQueryFunction::Run() {
  std::optional<api::search::Query::Params> params =
      api::search::Query::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  // Convenience for input params.
  const std::string& text = params->query_info.text;
  const std::optional<int>& tab_id = params->query_info.tab_id;
  Disposition disposition = params->query_info.disposition;

  // Simple validation of input params.
  if (text.empty()) {
    return RespondNow(Error("Empty text parameter."));
  }
  if (tab_id && disposition != Disposition::kNone) {
    return RespondNow(Error("Cannot set both 'disposition' and 'tabId'."));
  }

  Profile* profile = Profile::FromBrowserContext(browser_context());

  // We need to find which tab to navigate, which may be a specific tab or a
  // new tab.
  content::WebContents* web_contents = nullptr;

  // If the extension specified a tab, that takes priority.
  // Get web_contents if tab_id is valid, or dispoosition.
  if (tab_id) {
    if (!ExtensionTabUtil::GetTabById(
            *tab_id, profile, include_incognito_information(), &web_contents)) {
      return RespondNow(
          Error(base::StringPrintf("No tab with id: %d.", *tab_id)));
    }
    // If tab_id was specified, disposition couldn't have been (checked above).
    DCHECK_EQ(Disposition::kNone, disposition);
  }

  // If the extension didn't specify a tab, we need to find a browser to use.
  Browser* browser = nullptr;
  if (!web_contents) {
    // If the extension called the API from a tab, we can use that tab -
    // find the associated browser.
    web_contents = GetSenderWebContents();
    if (web_contents) {
      browser = chrome::FindBrowserWithTab(web_contents);
    }
    // Otherwise (e.g. when the extension calls the API from the background
    // page or service worker), fall back to the last active browser.
    if (!browser) {
      browser = chrome::FindTabbedBrowser(
          profile,
          /*match_original_profiles=*/include_incognito_information());
      if (!browser) {
        return RespondNow(Error("No active browser."));
      }
      web_contents = browser->tab_strip_model()->GetActiveWebContents();
    }
  }

  DCHECK(browser || (web_contents && disposition == Disposition::kNone));

  // GURL for default search provider.
  TemplateURLService* url_service =
      TemplateURLServiceFactory::GetForProfile(profile);
  DCHECK(url_service);
  GURL url =
      GetDefaultSearchURLForSearchTerms(url_service, base::UTF8ToUTF16(text));
  if (!url.is_valid()) {
    return RespondNow(Error("Missing default search provider."));
  }

  switch (disposition) {
    case Disposition::kCurrentTab:
    case Disposition::kNone:
      DCHECK(url.is_valid());
      web_contents->GetController().LoadURL(
          url, content::Referrer(),
          ui::PageTransition::PAGE_TRANSITION_FROM_API,
          /*extra_headers=*/std::string());
      break;
    case Disposition::kNewTab:
      NavigateToURL(WindowOpenDisposition::NEW_FOREGROUND_TAB, browser, url);
      break;
    case Disposition::kNewWindow:
      NavigateToURL(WindowOpenDisposition::NEW_WINDOW, browser, url);
      break;
  }

  return RespondNow(NoArguments());
}

}  // namespace extensions
