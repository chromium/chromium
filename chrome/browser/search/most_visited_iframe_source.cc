// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/most_visited_iframe_source.h"

#include "base/memory/ref_counted_memory.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "chrome/browser/search/instant_io_context.h"
#include "chrome/browser/search/ntp_features.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/local_ntp_resources.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/resources/grit/webui_resources.h"
#include "url/gurl.h"

namespace {

// Single-iframe version, used by the local NTP and the Google remote NTP.
const char kSingleHTMLPath[] = "/single.html";
const char kSingleCSSPath[] = "/single.css";
const char kSingleJSPath[] = "/single.js";

// Multi-iframe version, used by third party remote NTPs.
const char kAssertJsPath[] = "/assert.js";
const char kCommonCSSPath[] = "/common.css";
const char kDontShowPngPath[] = "/dont_show.png";
const char kDontShow2XPngPath[] = "/dont_show_2x.png";
const char kTitleHTMLPath[] = "/title.html";
const char kTitleCSSPath[] = "/title.css";
const char kTitleJSPath[] = "/title.js";
const char kUtilJSPath[] = "/util.js";

// Edit custom links dialog iframe and resources, used by the local NTP and the
// Google remote NTP.
const char kEditHTMLPath[] = "/edit.html";
const char kEditCSSPath[] = "/edit.css";
const char kEditJSPath[] = "/edit.js";
const char kAddSvgPath[] = "/add_link.svg";
const char kAddWhiteSvgPath[] = "/add_link_white.svg";
const char kEditMenuSvgPath[] = "/edit_menu.svg";

// Used in the single-iframe version and the edit custom links dialog iframe.
const char kAnimationsCSSPath[] = "/animations.css";
const char kAnimationsJSPath[] = "/animations.js";
const char kLocalNTPCommonCSSPath[] = "/local-ntp-common.css";
const char kLocalNTPUtilsJSPath[] = "/utils.js";

}  // namespace

MostVisitedIframeSource::MostVisitedIframeSource() = default;

MostVisitedIframeSource::~MostVisitedIframeSource() = default;

std::string MostVisitedIframeSource::GetSource() {
  return chrome::kChromeSearchMostVisitedHost;
}

void MostVisitedIframeSource::StartDataRequest(
    const GURL& url,
    const content::WebContents::Getter& wc_getter,
    const content::URLDataSource::GotDataCallback& callback) {
  // TODO(crbug/1009127): Simplify usages of |path| since |url| is available.
  const std::string path(url.path());

  if (path == kSingleHTMLPath) {
    SendResource(IDR_MOST_VISITED_SINGLE_HTML, callback);
  } else if (path == kSingleCSSPath) {
    SendResource(IDR_MOST_VISITED_SINGLE_CSS, callback);
  } else if (path == kSingleJSPath) {
    SendJSWithOrigin(IDR_MOST_VISITED_SINGLE_JS, wc_getter, callback);
  } else if (path == kTitleHTMLPath) {
    SendResource(IDR_MOST_VISITED_TITLE_HTML, callback);
  } else if (path == kTitleCSSPath) {
    SendResource(IDR_MOST_VISITED_TITLE_CSS, callback);
  } else if (path == kTitleJSPath) {
    SendResource(IDR_MOST_VISITED_TITLE_JS, callback);
  } else if (path == kUtilJSPath) {
    SendJSWithOrigin(IDR_MOST_VISITED_UTIL_JS, wc_getter, callback);
  } else if (path == kCommonCSSPath) {
    SendResource(IDR_MOST_VISITED_IFRAME_CSS, callback);
  } else if (path == kDontShowPngPath) {
    SendResource(IDR_MOST_VISITED_DONT_SHOW_PNG, callback);
  } else if (path == kDontShow2XPngPath) {
    SendResource(IDR_MOST_VISITED_DONT_SHOW_2X_PNG, callback);
  } else if (path == kEditHTMLPath) {
    SendResource(IDR_CUSTOM_LINKS_EDIT_HTML, callback);
  } else if (path == kEditCSSPath) {
    SendResource(IDR_CUSTOM_LINKS_EDIT_CSS, callback);
  } else if (path == kEditJSPath) {
    SendJSWithOrigin(IDR_CUSTOM_LINKS_EDIT_JS, wc_getter, callback);
  } else if (path == kAddSvgPath) {
    SendResource(IDR_CUSTOM_LINKS_ADD_SVG, callback);
  } else if (path == kAddWhiteSvgPath) {
    SendResource(IDR_CUSTOM_LINKS_ADD_WHITE_SVG, callback);
  } else if (path == kEditMenuSvgPath) {
    SendResource(IDR_CUSTOM_LINKS_EDIT_MENU_SVG, callback);
  } else if (path == kLocalNTPCommonCSSPath) {
    SendResource(IDR_LOCAL_NTP_COMMON_CSS, callback);
  } else if (path == kAnimationsCSSPath) {
    SendResource(IDR_LOCAL_NTP_ANIMATIONS_CSS, callback);
  } else if (path == kAnimationsJSPath) {
    SendResource(IDR_LOCAL_NTP_ANIMATIONS_JS, callback);
  } else if (path == kLocalNTPUtilsJSPath) {
    SendResource(IDR_LOCAL_NTP_UTILS_JS, callback);
  } else if (path == kAssertJsPath) {
    SendResource(IDR_WEBUI_JS_ASSERT, callback);
  } else {
    callback.Run(nullptr);
  }
}

std::string MostVisitedIframeSource::GetMimeType(
    const std::string& path_and_query) {
  std::string path(GURL("chrome-search://host/" + path_and_query).path());
  if (base::EndsWith(path, ".js", base::CompareCase::INSENSITIVE_ASCII))
    return "application/javascript";
  if (base::EndsWith(path, ".png", base::CompareCase::INSENSITIVE_ASCII))
    return "image/png";
  if (base::EndsWith(path, ".css", base::CompareCase::INSENSITIVE_ASCII))
    return "text/css";
  if (base::EndsWith(path, ".html", base::CompareCase::INSENSITIVE_ASCII))
    return "text/html";
  if (base::EndsWith(path, ".svg", base::CompareCase::INSENSITIVE_ASCII))
    return "image/svg+xml";
  return std::string();
}

bool MostVisitedIframeSource::AllowCaching() {
  return false;
}

bool MostVisitedIframeSource::ShouldServiceRequest(
    const GURL& url,
    content::ResourceContext* resource_context,
    int render_process_id) {
  return InstantIOContext::ShouldServiceRequest(url, resource_context,
                                                render_process_id) &&
         url.SchemeIs(chrome::kChromeSearchScheme) &&
         url.host_piece() == GetSource() && ServesPath(url.path());
}

bool MostVisitedIframeSource::ShouldDenyXFrameOptions() {
  return false;
}

bool MostVisitedIframeSource::ServesPath(const std::string& path) const {
  return path == kSingleHTMLPath || path == kSingleCSSPath ||
         path == kSingleJSPath || path == kTitleHTMLPath ||
         path == kTitleCSSPath || path == kTitleJSPath || path == kUtilJSPath ||
         path == kCommonCSSPath || path == kEditHTMLPath ||
         path == kEditCSSPath || path == kEditJSPath || path == kAddSvgPath ||
         path == kAddWhiteSvgPath || path == kEditMenuSvgPath ||
         path == kLocalNTPCommonCSSPath || path == kAnimationsCSSPath ||
         path == kAnimationsJSPath || path == kLocalNTPUtilsJSPath ||
         path == kAssertJsPath || path == kDontShowPngPath ||
         path == kDontShow2XPngPath;
}

void MostVisitedIframeSource::SendResource(
    int resource_id,
    const content::URLDataSource::GotDataCallback& callback) {
  callback.Run(ui::ResourceBundle::GetSharedInstance().LoadDataResourceBytes(
      resource_id));
}

void MostVisitedIframeSource::SendJSWithOrigin(
    int resource_id,
    const content::WebContents::Getter& wc_getter,
    const content::URLDataSource::GotDataCallback& callback) {
  std::string origin;
  if (!GetOrigin(wc_getter, &origin)) {
    callback.Run(nullptr);
    return;
  }

  base::StringPiece template_js =
      ui::ResourceBundle::GetSharedInstance().GetRawDataResource(resource_id);
  std::string response(template_js.as_string());
  base::ReplaceFirstSubstringAfterOffset(&response, 0, "{{ORIGIN}}", origin);
  callback.Run(base::RefCountedString::TakeString(&response));
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

  *origin = entry->GetURL().GetOrigin().spec();
  // Origin should not include a trailing slash. That is part of the path.
  base::TrimString(*origin, "/", origin);
  return true;
}
