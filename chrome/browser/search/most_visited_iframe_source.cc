// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/most_visited_iframe_source.h"

#include <string_view>

#include "base/memory/ref_counted_memory.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "chrome/browser/search/instant_service.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/new_tab_page_instant_resources.h"
#include "components/search/ntp_features.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/resource/resource_bundle.h"
#include "url/gurl.h"

namespace {

// Multi-iframe version, used by third party remote NTPs.
const char kTitleHTMLPath[] = "/title.html";
const char kTitleCSSPath[] = "/title.css";
const char kTitleJSPath[] = "/title.js";

}  // namespace

MostVisitedIframeSource::MostVisitedIframeSource() = default;

MostVisitedIframeSource::~MostVisitedIframeSource() = default;

std::string MostVisitedIframeSource::GetSource() {
  return chrome::kChromeSearchMostVisitedHost;
}

void MostVisitedIframeSource::StartDataRequest(
    const GURL& url,
    const content::WebContents::Getter& wc_getter,
    content::URLDataSource::GotDataCallback callback) {
  // TODO(crbug.com/40050262): Simplify usages of |path| since |url| is
  // available.
  const std::string path(url.path());

  if (path == kTitleHTMLPath) {
    SendResource(IDR_NEW_TAB_PAGE_INSTANT_MOST_VISITED_TITLE_HTML,
                 std::move(callback));
  } else if (path == kTitleCSSPath) {
    SendResource(IDR_NEW_TAB_PAGE_INSTANT_MOST_VISITED_TITLE_CSS,
                 std::move(callback));
  } else if (path == kTitleJSPath) {
    SendJSWithOrigin(IDR_NEW_TAB_PAGE_INSTANT_MOST_VISITED_TITLE_JS, wc_getter,
                     std::move(callback));
  } else {
    std::move(callback).Run(nullptr);
  }
}

std::string MostVisitedIframeSource::GetMimeType(const GURL& url) {
  std::string_view path = url.path_piece();
  if (base::EndsWith(path, ".js", base::CompareCase::INSENSITIVE_ASCII))
    return "application/javascript";
  if (base::EndsWith(path, ".css", base::CompareCase::INSENSITIVE_ASCII))
    return "text/css";
  if (base::EndsWith(path, ".html", base::CompareCase::INSENSITIVE_ASCII))
    return "text/html";
  return std::string();
}

bool MostVisitedIframeSource::ShouldServeMimeTypeAsContentTypeHeader() {
  return true;
}

bool MostVisitedIframeSource::AllowCaching() {
  return false;
}

bool MostVisitedIframeSource::ShouldServiceRequest(
    const GURL& url,
    content::BrowserContext* browser_context,
    int render_process_id) {
  return InstantService::ShouldServiceRequest(url, browser_context,
                                              render_process_id) &&
         url.SchemeIs(chrome::kChromeSearchScheme) &&
         url.host_piece() == GetSource() && ServesPath(url.path());
}

bool MostVisitedIframeSource::ShouldDenyXFrameOptions() {
  return false;
}

bool MostVisitedIframeSource::ServesPath(const std::string& path) const {
  return path == kTitleHTMLPath || path == kTitleCSSPath ||
         path == kTitleJSPath;
}

void MostVisitedIframeSource::SendResource(
    int resource_id,
    content::URLDataSource::GotDataCallback callback) {
  std::move(callback).Run(
      ui::ResourceBundle::GetSharedInstance().LoadDataResourceBytes(
          resource_id));
}

void MostVisitedIframeSource::SendJSWithOrigin(
    int resource_id,
    const content::WebContents::Getter& wc_getter,
    content::URLDataSource::GotDataCallback callback) {
  std::string origin;
  if (!GetOrigin(wc_getter, &origin)) {
    std::move(callback).Run(nullptr);
    return;
  }

  std::string response =
      ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
          resource_id);
  base::ReplaceFirstSubstringAfterOffset(&response, 0, "{{ORIGIN}}", origin);
  std::move(callback).Run(
      base::MakeRefCounted<base::RefCountedString>(std::move(response)));
}

bool MostVisitedIframeSource::GetOrigin(
    const content::WebContents::Getter& wc_getter,
    std::string* origin) const {
  if (wc_getter.is_null())
    return false;
  content::WebContents* contents = wc_getter.Run();
  if (!contents)
    return false;
  content::NavigationEntry* entry = contents->GetController().GetVisibleEntry();
  if (!entry)
    return false;

  *origin = entry->GetURL().DeprecatedGetOriginAsURL().spec();
  // Origin should not include a trailing slash. That is part of the path.
  base::TrimString(*origin, "/", origin);
  return true;
}
