// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/search.h"

#include <stddef.h>

#include <string>

#include "base/check_deref.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/search_engines/ui_thread_search_terms_data.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "components/google/core/common/google_util.h"
#include "components/search/ntp_features.h"
#include "components/search/search.h"
#include "components/search_engines/search_engine_type.h"
#include "components/search_engines/template_url_service.h"
#include "components/supervised_user/core/browser/supervised_user_preferences.h"
#include "components/supervised_user/core/browser/supervised_user_service.h"
#include "components/supervised_user/core/browser/supervised_user_url_filter.h"  // nogncheck
#include "components/supervised_user/core/browser/supervised_user_utils.h"
#include "components/supervised_user/core/common/buildflags.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/search/instant_service.h"
#include "chrome/browser/search/instant_service_factory.h"
#include "chrome/browser/ui/webui/new_tab_page/new_tab_page_ui.h"
#include "chrome/browser/ui/webui/new_tab_page_third_party/new_tab_page_third_party_ui.h"
#endif

namespace search {

namespace {

const char kServiceWorkerFileName[] = "newtab-serviceworker.js";

bool MatchesOrigin(const GURL& my_url, const GURL& other_url) {
  return my_url.scheme_piece() == other_url.scheme_piece() &&
         my_url.host_piece() == other_url.host_piece() &&
         my_url.port() == other_url.port();
}

}  // namespace

// Returns true if |my_url| matches |other_url| in terms of origin (i.e. host,
// port, and scheme) and path.
// Defined outside of the anonymous namespace so that it's accessible to unit
// tests.
bool MatchesOriginAndPath(const GURL& my_url, const GURL& other_url) {
  return MatchesOrigin(my_url, other_url) &&
         my_url.path_piece() == other_url.path_piece();
}

namespace {

// Status of the New Tab URL for the default Search provider. NOTE: Used in a
// UMA histogram so values should only be added at the end and not reordered.
enum NewTabURLState {
  // Valid URL that should be used.
  NEW_TAB_URL_VALID = 0,

  // Corrupt state (e.g. no profile or template url).
  NEW_TAB_URL_BAD = 1,

  // URL should not be used because in incognito window.
  NEW_TAB_URL_INCOGNITO = 2,

  // No New Tab URL set for provider.
  NEW_TAB_URL_NOT_SET = 3,

  // URL is not secure.
  NEW_TAB_URL_INSECURE = 4,

  // URL should not be used because Suggest is disabled.
  // Not used anymore, see crbug.com/340424.
  // NEW_TAB_URL_SUGGEST_OFF = 5,

  // URL should not be used because it is blocked for a supervised user.
  NEW_TAB_URL_BLOCKED = 6,

  NEW_TAB_URL_MAX
};

const TemplateURL* GetDefaultSearchProviderTemplateURL(Profile* profile) {
  if (profile) {
    TemplateURLService* template_url_service =
        TemplateURLServiceFactory::GetForProfile(profile);
    if (template_url_service) {
      return template_url_service->GetDefaultSearchProvider();
    }
  }
  return nullptr;
}

bool IsMatchingServiceWorker(const GURL& my_url, const GURL& document_url) {
  // The origin should match.
  if (!MatchesOrigin(my_url, document_url)) {
    return false;
  }

  // The url filename should be the new tab page ServiceWorker.
  std::string my_filename = my_url.ExtractFileName();
  if (my_filename != kServiceWorkerFileName) {
    return false;
  }

  // The paths up to the filenames should be the same.
  std::string my_path_without_filename = my_url.path();
  my_path_without_filename = my_path_without_filename.substr(
      0, my_path_without_filename.length() - my_filename.length());
  std::string document_filename = document_url.ExtractFileName();
  std::string document_path_without_filename = document_url.path();
  document_path_without_filename = document_path_without_filename.substr(
      0, document_path_without_filename.length() - document_filename.length());

  return my_path_without_filename == document_path_without_filename;
}

// Returns true if |url| matches the NTP URL or the URL of the NTP's associated
// service worker.
bool IsNTPOrRelatedURLHelper(const GURL& url, Profile* profile) {
  if (!url.is_valid()) {
    return false;
  }

  const GURL new_tab_url(GetNewTabPageURL(profile));
  return new_tab_url.is_valid() && (MatchesOriginAndPath(url, new_tab_url) ||
                                    IsMatchingServiceWorker(url, new_tab_url));
}

bool IsURLAllowedForSupervisedUser(const GURL& url, Profile& profile) {
  if (!profile.IsChild()) {
    return true;
  }
  supervised_user::SupervisedUserService* supervised_user_service =
      SupervisedUserServiceFactory::GetForProfile(&profile);
  supervised_user::SupervisedUserURLFilter* url_filter =
      supervised_user_service->GetURLFilter();
  if (url_filter->GetFilteringBehaviorForURL(url) ==
      supervised_user::FilteringBehavior::kBlock) {
    return false;
  }
  return true;
}

// Used to look up the URL to use for the New Tab page. Also tracks how we
// arrived at that URL so it can be logged with UMA.
struct NewTabURLDetails {
  NewTabURLDetails(const GURL& url, NewTabURLState state)
      : url(url), state(state) {}

  static NewTabURLDetails ForProfile(Profile* profile) {
    // Incognito and Guest profiles have their own New Tab.
    // This function may also be called by other off-the-record profiles that
    // can exceptionally open a browser window.
    // See OTRProfileID::AllowsBrowserWindows() for more context.
    if (profile->IsOffTheRecord()) {
      return NewTabURLDetails(GURL(), NEW_TAB_URL_INCOGNITO);
    }

#if BUILDFLAG(IS_ANDROID)
    const GURL local_url;
#else
    const bool default_is_google = DefaultSearchProviderIsGoogle(profile);
    const GURL local_url(default_is_google
                             ? chrome::kChromeUINewTabPageURL
                             : chrome::kChromeUINewTabPageThirdPartyURL);
    if (default_is_google) {
      return NewTabURLDetails(local_url, NEW_TAB_URL_VALID);
    }
#endif

    const TemplateURL* template_url =
        GetDefaultSearchProviderTemplateURL(profile);
    if (!profile || !template_url) {
      return NewTabURLDetails(local_url, NEW_TAB_URL_BAD);
    }

    GURL search_provider_url(template_url->new_tab_url_ref().ReplaceSearchTerms(
        TemplateURLRef::SearchTermsArgs(std::u16string()),
        UIThreadSearchTermsData()));

    if (!search_provider_url.is_valid()) {
      return NewTabURLDetails(local_url, NEW_TAB_URL_NOT_SET);
    }
    if (!search_provider_url.SchemeIsCryptographic()) {
      return NewTabURLDetails(local_url, NEW_TAB_URL_INSECURE);
    }
    if (!IsURLAllowedForSupervisedUser(search_provider_url,
                                       CHECK_DEREF(profile))) {
      return NewTabURLDetails(local_url, NEW_TAB_URL_BLOCKED);
    }

    return NewTabURLDetails(search_provider_url, NEW_TAB_URL_VALID);
  }

  const GURL url;
  const NewTabURLState state;
};

bool IsRenderedInInstantProcess(content::WebContents* contents,
                                Profile* profile) {
#if BUILDFLAG(IS_ANDROID)
  return false;
#else
  content::RenderProcessHost* process_host =
      contents->GetPrimaryMainFrame()->GetProcess();
  if (!process_host) {
    return false;
  }

  const InstantService* instant_service =
      InstantServiceFactory::GetForProfile(profile);
  if (!instant_service) {
    return false;
  }

  return instant_service->IsInstantProcess(process_host->GetID());
#endif
}

}  // namespace

bool DefaultSearchProviderIsGoogle(Profile* profile) {
  return DefaultSearchProviderIsGoogle(
      TemplateURLServiceFactory::GetForProfile(profile));
}

bool IsNTPOrRelatedURL(const GURL& url, Profile* profile) {
  if (!url.is_valid()) {
    return false;
  }

  if (!IsInstantExtendedAPIEnabled()) {
    return url == chrome::kChromeUINewTabURL;
  }

  return profile && IsNTPOrRelatedURLHelper(url, profile);
}

bool IsNTPURL(const GURL& url) {
  if (url.SchemeIs(chrome::kChromeSearchScheme) &&
      url.host_piece() == chrome::kChromeSearchRemoteNtpHost) {
    return true;
  }
#if BUILDFLAG(IS_ANDROID)
  return false;
#else
  return NewTabPageUI::IsNewTabPageOrigin(url) ||
         NewTabPageThirdPartyUI::IsNewTabPageOrigin(url);
#endif
}

bool IsInstantNTP(content::WebContents* contents) {
  if (!contents) {
    return false;
  }

  content::NavigationEntry* entry =
      contents->GetController().GetLastCommittedEntry();
  if (!entry) {
    entry = contents->GetController().GetVisibleEntry();
  }
  return NavEntryIsInstantNTP(contents, entry);
}

bool NavEntryIsInstantNTP(content::WebContents* contents,
                          content::NavigationEntry* entry) {
  if (!contents || !entry || !IsInstantExtendedAPIEnabled()) {
    return false;
  }

  Profile* profile = Profile::FromBrowserContext(contents->GetBrowserContext());
  if (!IsRenderedInInstantProcess(contents, profile)) {
    return false;
  }

  return IsInstantNTPURL(entry->GetURL(), profile);
}

bool IsInstantNTPURL(const GURL& url, Profile* profile) {
  if (MatchesOrigin(url, GURL(chrome::kChromeUINewTabPageURL))) {
    return true;
  }

  if (!IsInstantExtendedAPIEnabled()) {
    return false;
  }

  GURL new_tab_url(GetNewTabPageURL(profile));
  return new_tab_url.is_valid() && MatchesOriginAndPath(url, new_tab_url);
}

GURL GetNewTabPageURL(Profile* profile) {
  return NewTabURLDetails::ForProfile(profile).url;
}

#if !BUILDFLAG(IS_ANDROID)

bool ShouldAssignURLToInstantRenderer(const GURL& url, Profile* profile) {
  if (!url.is_valid() || !profile || !IsInstantExtendedAPIEnabled() ||
      url.SchemeIs(content::kChromeUIScheme)) {
    return false;
  }

  return IsNTPOrRelatedURLHelper(url, profile) ||
         url.SchemeIs(chrome::kChromeSearchScheme);
}

bool ShouldUseProcessPerSiteForInstantSiteURL(const GURL& site_url,
                                              Profile* profile) {
  return ShouldAssignURLToInstantRenderer(site_url, profile) &&
         site_url.host_piece() == chrome::kChromeSearchRemoteNtpHost;
}

GURL GetEffectiveURLForInstant(const GURL& url, Profile* profile) {
  CHECK(ShouldAssignURLToInstantRenderer(url, profile))
      << "Error granting Instant access.";

  if (url.SchemeIs(chrome::kChromeSearchScheme)) {
    return url;
  }

  // Replace the scheme with "chrome-search:", and clear the port, since
  // chrome-search is a scheme without port.
  GURL::Replacements replacements;
  replacements.SetSchemeStr(chrome::kChromeSearchScheme);
  replacements.ClearPort();

  // If this is the URL for a server-provided NTP, replace the host with
  // "remote-ntp".
  std::string remote_ntp_host(chrome::kChromeSearchRemoteNtpHost);
  NewTabURLDetails details = NewTabURLDetails::ForProfile(profile);
  if (details.state == NEW_TAB_URL_VALID &&
      (MatchesOriginAndPath(url, details.url) ||
       IsMatchingServiceWorker(url, details.url))) {
    replacements.SetHostStr(remote_ntp_host);
  }

  return url.ReplaceComponents(replacements);
}

bool HandleNewTabURLRewrite(GURL* url,
                            content::BrowserContext* browser_context) {
  if (!IsInstantExtendedAPIEnabled()) {
    return false;
  }

  if (!(url->SchemeIs(content::kChromeUIScheme) &&
        url->host() == chrome::kChromeUINewTabHost)) {
    return false;
  }

  Profile* profile = Profile::FromBrowserContext(browser_context);
  NewTabURLDetails details(NewTabURLDetails::ForProfile(profile));
  UMA_HISTOGRAM_ENUMERATION("NewTabPage.URLState", details.state,
                            NEW_TAB_URL_MAX);
  if (details.url.is_valid()) {
    *url = details.url;
    return true;
  }
  return false;
}

bool HandleNewTabURLReverseRewrite(GURL* url,
                                   content::BrowserContext* browser_context) {
  if (!IsInstantExtendedAPIEnabled()) {
    return false;
  }

  // Do nothing in incognito.
  Profile* profile = Profile::FromBrowserContext(browser_context);
  DCHECK(profile);
  if (profile->IsOffTheRecord()) {
    return false;
  }

  if (IsInstantNTPURL(*url, profile)) {
    *url = GURL(chrome::kChromeUINewTabURL);
    return true;
  }

  return false;
}

#endif  // !BUILDFLAG(IS_ANDROID)

}  // namespace search
