// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/search/search_api.h"

#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/navigator/browser_navigator.h"
#include "chrome/common/extensions/api/search.h"
#include "components/search_engines/util.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"
#include "extensions/buildflags/buildflags.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#else
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "chrome/browser/ui/browser_window/public/profile_browser_collection.h"
#include "chrome/browser/ui/tabs/tab_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#endif

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

#if !BUILDFLAG(IS_ANDROID)
using tabs::TabModel;
#endif

namespace extensions {

namespace {

#if BUILDFLAG(IS_ANDROID)
// Returns the TabModel for the last active window owned by `profile` (and
// optionally its incognito profile). Returns null on failure.
TabModel* GetLastActiveTabModel(Profile* profile, bool include_incognito) {
  // Find the last active browser for the current profile.
  BrowserWindowInterface* browser = nullptr;
  ForEachCurrentBrowserWindowInterfaceOrderedByActivation(
      [&](BrowserWindowInterface* bwi) {
        if (bwi->GetProfile() == profile ||
            (include_incognito && bwi->GetProfile()->GetOriginalProfile() ==
                                      profile->GetOriginalProfile())) {
          browser = bwi;
          return false;
        }
        return true;  // Keep iterating.
      });
  if (browser) {
    return static_cast<TabModel*>(TabListInterface::From(browser));
  }
  return nullptr;
}
#endif  // BUILDFLAG(IS_ANDROID)

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
  // Get web_contents if tab_id is valid, or disposition.
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
  if (!web_contents) {
    // If the extension called the API from a tab, we can use that tab -
    // find the associated browser or tab model.
    web_contents = GetSenderWebContents();
#if !BUILDFLAG(IS_ANDROID)
    BrowserWindowInterface* browser = nullptr;
    if (web_contents) {
      browser = GlobalBrowserCollection::GetInstance()->FindBrowserWithTab(
          web_contents);
    }
    // Otherwise (e.g. when the extension calls the API from the background
    // page or service worker), fall back to the last active browser.
    if (!browser) {
      if (!profile) {
        return RespondNow(Error("No active browser."));
      }
      browser =
          ProfileBrowserCollection::GetForProfile(profile)->FindTabbedBrowser(
              /*match_original_profiles=*/
              include_incognito_information());
      if (!browser) {
        return RespondNow(Error("No active browser."));
      }
      web_contents = browser->GetTabStripModel()->GetActiveWebContents();
    }
#else
    TabModel* tab_model = nullptr;
    // If the extension called the API from a tab, use that tab model.
    if (web_contents) {
      tab_model = TabModelList::GetTabModelForWebContents(web_contents);
    }
    // If the extension called the API from a service worker, fall back to the
    // last active browser's tab model.
    if (!tab_model) {
      tab_model =
          GetLastActiveTabModel(profile, include_incognito_information());
      if (!tab_model) {
        return RespondNow(Error("No active browser."));
      }
      web_contents = tab_model->GetActiveWebContents();
    }
#endif  // !BUILDFLAG(IS_ANDROID)
  }

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
      ExtensionTabUtil::NavigateToURL(
          WindowOpenDisposition::NEW_FOREGROUND_TAB,
          /*source_contents=*/web_contents, url,
          // Binding `this` is safe because it is ref-counted.
          base::BindOnce(&SearchQueryFunction::OnNavigate, this));
      return RespondLater();  // Responds in OnNavigate().

    case Disposition::kNewWindow:
      ExtensionTabUtil::NavigateToURL(
          WindowOpenDisposition::NEW_WINDOW,
          /*source_contents=*/web_contents, url,
          // Binding `this` is safe because it is ref-counted.
          base::BindOnce(&SearchQueryFunction::OnNavigate, this));
      return RespondLater();  // Responds in OnNavigate().
  }

  return RespondNow(NoArguments());
}

void SearchQueryFunction::OnNavigate() {
  Respond(NoArguments());
}

}  // namespace extensions
