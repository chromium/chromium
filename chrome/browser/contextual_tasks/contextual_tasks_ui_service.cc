// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service.h"

#include "base/logging.h"
#include "base/strings/string_util.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui.h"
#include "content/public/browser/web_contents.h"
#include "net/base/url_util.h"

namespace contextual_tasks {

namespace {
constexpr char kAiPageHost[] = "https://google.com";
constexpr char kAiDefaultPageUrl[] =
    "https://www.google.com/search?udm=50&aep=11&igu=1";

bool IsContextualTasksHost(const GURL& url) {
  return url.scheme() == content::kChromeUIScheme &&
         url.host() == kContextualTasksUiHost;
}

}  // namespace

ContextualTasksUiService::ContextualTasksUiService() {
  ai_page_host_ = GURL(kAiPageHost);
}

ContextualTasksUiService::~ContextualTasksUiService() = default;

void ContextualTasksUiService::OnNavigationToAiPageIntercepted(
    const GURL& url,
    content::WebContents* source_contents,
    bool is_to_new_tab) {}

void ContextualTasksUiService::OnThreadLinkClicked(
    const GURL& url,
    content::WebContents* source_contents) {}

bool ContextualTasksUiService::HandleNavigation(
    const GURL& navigation_url,
    const GURL& responsible_web_contents_url,
    content::WebContents* source_contents,
    bool is_to_new_tab) {
  // Allow any navigation to the contextual tasks host.
  if (IsContextualTasksHost(navigation_url)) {
    return false;
  }

  bool is_nav_to_ai = IsAiUrl(navigation_url);

  // Intercept any navigation where the wrapping WebContents is the WebUI host
  // unless it is the AI page.
  if (IsContextualTasksHost(responsible_web_contents_url)) {
    if (is_nav_to_ai) {
      return false;
    }
    OnThreadLinkClicked(navigation_url, source_contents);
    return true;
  }

  // Navigations to the AI URL in the topmost frame should always be
  // intercepted.
  if (is_nav_to_ai) {
    OnNavigationToAiPageIntercepted(navigation_url, source_contents,
                                    is_to_new_tab);
    return true;
  }

  // Allow anything else.
  return false;
}

GURL ContextualTasksUiService::GetDefaultAiPageUrl() {
  return GURL(kAiDefaultPageUrl);
}

bool ContextualTasksUiService::IsAiUrl(const GURL& url) {
  if (!url.SchemeIsHTTPOrHTTPS() ||
      !base::EndsWith(url.host(), ai_page_host_.host())) {
    return false;
  }

  if (!base::StartsWith(url.path(), "/search")) {
    return false;
  }

  // AI pages are identified by the "udm" URL param having a value of 50.
  std::string udm_value;
  if (!net::GetValueForKeyInQuery(url, "udm", &udm_value)) {
    return false;
  }

  return udm_value == "50";
}

}  // namespace contextual_tasks
