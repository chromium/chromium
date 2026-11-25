// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/ntp/new_tab_page_url_handler.h"

#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "base/android/device_info.h"
#include "base/command_line.h"
#include "base/strings/escape.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/search.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/common/content_features.h"
#include "content/public/common/url_constants.h"
#include "extensions/buildflags/buildflags.h"
#include "net/base/url_util.h"
#include "ui/webui/buildflags.h"
#include "url/gurl.h"

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
#include "chrome/browser/extensions/extension_url_overrides.h"
#endif

namespace {

constexpr std::string_view kBookmarkFolderPath = "folder/";
constexpr std::string_view kBookmarkFolderPrefix = "/folder/";
constexpr std::string_view kBookmarkIdQueryKey = "id";
constexpr std::string_view kBookmarkSearchQueryKey = "q";

bool IsChromeOrNativeScheme(const GURL& url) {
  return url.SchemeIs(content::kChromeUIScheme) ||
         url.SchemeIs(chrome::kChromeNativeScheme);
}

// Unescapes a bookmark folder identifier taken from a percent-encoded URL path
// segment or fragment.
std::string UnescapeBookmarkFolderId(std::string_view encoded_id) {
  return base::UnescapeURLComponent(
      encoded_id,
      base::UnescapeRule::SPACES | base::UnescapeRule::PATH_SEPARATORS |
          base::UnescapeRule::URL_SPECIAL_CHARS_EXCEPT_PATH_SEPARATORS);
}

// Extracts an unescaped bookmark folder identifier from any of the Android
// native or WebUI bookmark URL formats:
// * `chrome-native://bookmarks/folder/<id>` (path)
// * `chrome://bookmarks/?id=<id>` (query parameter)
// * `chrome-native://bookmarks/#<id>` (ref)
// Returns an empty string if no valid folder ID is present (including `.` or
// `..` dot-segments).
std::string ExtractBookmarkFolderId(const GURL& url) {
  std::string folder_id;
  if (std::optional<std::string_view> suffix =
          base::RemovePrefix(url.path(), kBookmarkFolderPrefix)) {
    std::string_view trimmed =
        base::TrimString(*suffix, "/", base::TRIM_TRAILING);
    if (!trimmed.empty()) {
      folder_id = UnescapeBookmarkFolderId(trimmed);
    }
  }
  if (folder_id.empty()) {
    net::GetValueForKeyInQuery(url, kBookmarkIdQueryKey, &folder_id);
  }
  if (folder_id.empty() && url.has_ref()) {
    folder_id = UnescapeBookmarkFolderId(url.ref());
  }
  return (folder_id == "." || folder_id == "..") ? std::string() : folder_id;
}

// Extracts the unescaped `q` bookmark search query parameter if present, or
// returns an empty string otherwise.
std::string ExtractBookmarkSearchQuery(const GURL& url) {
  std::string search_query;
  net::GetValueForKeyInQuery(url, kBookmarkSearchQueryKey, &search_query);
  return search_query;
}

// Returns true if `url` (a `chrome://` or `chrome-native://` URL whose host is
// `kChromeUIBookmarksHost`) should be rewritten to the canonical WebUI format
// (`chrome://bookmarks/`, `chrome://bookmarks/?id=<id>`, or
// `chrome://bookmarks/?q=<query>`). Specifically captures:
// * Any `chrome-native://bookmarks/...` URL (root, `/folder/<id>`, `#<id>`, or
//   `?q=<query>`).
// * Native-formatted `chrome://bookmarks/folder/<id>` or
//   `chrome://bookmarks/#<id>` URLs, while leaving already-canonical
//   `chrome://bookmarks/`, `chrome://bookmarks/?id=<id>`, and
//   `chrome://bookmarks/?q=<query>` unchanged.
bool NeedsRewriteToWebUiBookmarksUrl(const GURL& url) {
  return url.SchemeIs(chrome::kChromeNativeScheme) ||
         base::StartsWith(url.path(), kBookmarkFolderPrefix) || url.has_ref();
}

// Returns true if `url` (a `chrome://` or `chrome-native://` URL whose host is
// `kChromeUIBookmarksHost`) should be rewritten to the Android native format
// (`chrome-native://bookmarks/`, `chrome-native://bookmarks/folder/<id>`, or
// `chrome-native://bookmarks/?q=<query>`). Specifically captures:
// * Any `chrome://bookmarks/...` URL (e.g., restored WebUI tabs or `@bookmarks`
//   site search queries).
// * `chrome-native://bookmarks/...` URLs using legacy `#<id>` fragments or
//   WebUI `?id=<id>` query parameters, while leaving other native URLs (such as
//   `/`, `/folder/<id>`, `/uncategorized/`, or `/?q=<query>`) untouched.
bool NeedsRewriteToNativeBookmarksUrl(const GURL& url) {
  return url.SchemeIs(content::kChromeUIScheme) || url.has_ref() ||
         net::GetValueForKeyInQuery(url, kBookmarkIdQueryKey, nullptr);
}

// Translates bookmark URLs between Android native
// (`chrome-native://bookmarks/`) and WebUI (`chrome://bookmarks/`) formats
// depending on whether WebUI Bookmarks is enabled for `browser_context`,
// preserving folder IDs and `?q=` search queries.
// TODO(crbug.com/587862341): Unify Android native and WebUI bookmark folder
// URL routing onto a single schema.
bool HandleBookmarksURL(GURL* url, content::BrowserContext* browser_context) {
  if (!IsChromeOrNativeScheme(*url) ||
      url->host() != chrome::kChromeUIBookmarksHost) {
    return false;
  }

  bool use_webui = false;
#if BUILDFLAG(ENABLE_WEBUI_BOOKMARKS)
  // Guest profiles are excluded because
  // `BookmarksMessageHandler::OnJavascriptAllowed()` disallows Guest sessions.
  if (base::FeatureList::IsEnabled(
          chrome::android::kUseWebUiBookmarksAndroid)) {
    Profile* profile = Profile::FromBrowserContext(browser_context);
    use_webui = profile && !profile->IsGuestSession();
  }
#endif

  const bool needs_rewrite = use_webui ? NeedsRewriteToWebUiBookmarksUrl(*url)
                                       : NeedsRewriteToNativeBookmarksUrl(*url);
  if (!needs_rewrite) {
    return false;
  }

  std::string folder_id = ExtractBookmarkFolderId(*url);
  std::string search_query = ExtractBookmarkSearchQuery(*url);

  GURL target_url;
  if (use_webui) {
    target_url = GURL(chrome::kChromeUIBookmarksURL);
    if (!folder_id.empty()) {
      target_url =
          net::AppendQueryParameter(target_url, kBookmarkIdQueryKey, folder_id);
    }
  } else {
    target_url =
        folder_id.empty()
            ? GURL(chrome::kChromeUINativeBookmarksURL)
            : GURL(base::StrCat({chrome::kChromeUINativeBookmarksURL,
                                 kBookmarkFolderPath,
                                 base::EscapeAllExceptUnreserved(folder_id)}));
  }

  if (!search_query.empty()) {
    target_url = net::AppendQueryParameter(target_url, kBookmarkSearchQueryKey,
                                           search_query);
  }

  if (*url == target_url) {
    return false;
  }
  *url = std::move(target_url);
  return true;
}

}  // namespace

namespace chrome {
namespace android {

bool HandleAndroidNativePageURL(GURL* url,
                                content::BrowserContext* browser_context) {
#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  if (base::FeatureList::IsEnabled(
          chrome::android::kChromeNativeUrlOverriding)) {
    // If an extension is overriding this URL, do not redirect it.
    if (ExtensionUrlOverrides::GetNumberOfExtensionsOverridingURL(
            *url, browser_context) > 0) {
      return false;
    }
  }
#endif

  if (search::IsNewTabOrigin(*url)) {
    // The url handler rewrites the url from chrome://newtab to the WebUI
    // NTP if the WebUI NTP is enabled. Depending on the default search
    // engine, it either returns the 1P WebUI NTP, the DSE's remote 3P NTP,
    // or the Chrome-owned fallback 3P WebUI NTP.
    if (search::IsWebUiNtpEnabledForDesktopAndroid()) {
      GURL new_tab_url = search::GetNewTabPageURL(
          Profile::FromBrowserContext(browser_context));
      if (new_tab_url.is_valid()) {
        *url = new_tab_url;
      } else {
        // Off-the-record profiles, such as Incognito or Guest profiles,
        // return an invalid URL, so default to the Java Native NTP instead.
        *url = GURL(chrome::kChromeUINativeNewTabURL);
      }
    } else {
      *url = GURL(chrome::kChromeUINativeNewTabURL);
    }
    return true;
  }

  // LINT.IfChange(AndroidDesktopWebUiHistory)
  if (base::android::device_info::is_desktop() &&
      base::FeatureList::IsEnabled(
          chrome::android::kAndroidDesktopWebUiHistory)) {
    if (url->SchemeIs(chrome::kChromeNativeScheme) &&
        url->GetHost() == kChromeUIHistoryHost) {
      GURL::Replacements replacements;
      replacements.SetSchemeStr(content::kChromeUIScheme);
      *url = url->ReplaceComponents(replacements);
      return true;
    }
  }
  // LINT.ThenChange(//chrome/browser/history/java/src/org/chromium/chrome/browser/history/HistoryFeatures.java:AndroidDesktopWebUiHistory,
  // //chrome/browser/ui/webui/history/history_ui.cc:AndroidDesktopWebUiHistory)

  return HandleBookmarksURL(url, browser_context);
}

bool HandleAndroidNativePageURLReverse(
    GURL* url,
    content::BrowserContext* browser_context) {
  // No need to modify `url`, but return true so
  // `NavigationControllerImpl::UpdateVirtualURLToURL()` updates the displayed
  // virtual URL when rewriting between `chrome-native://` and `chrome://` for
  // Bookmarks or History (whereas `chrome://newtab` keeps its virtual URL).
  return IsChromeOrNativeScheme(*url) &&
         (url->host() == kChromeUIBookmarksHost ||
          url->host() == kChromeUIHistoryHost);
}

}  // namespace android
}  // namespace chrome
