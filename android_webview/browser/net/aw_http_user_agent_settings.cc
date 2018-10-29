// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/net/aw_http_user_agent_settings.h"

#include "android_webview/browser/aw_content_browser_client.h"
#include "android_webview/common/aw_content_client.h"
#include "content/public/browser/browser_thread.h"
#include "net/http/http_util.h"

namespace android_webview {

AwHttpUserAgentSettings::AwHttpUserAgentSettings() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

AwHttpUserAgentSettings::~AwHttpUserAgentSettings() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
}

std::string AwHttpUserAgentSettings::GetAcceptLanguage() const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  std::string new_aw_accept_language =
      AwContentBrowserClient::GetAcceptLangsImpl();
  if (new_aw_accept_language != last_aw_accept_language_) {
    new_aw_accept_language =
        net::HttpUtil::ExpandLanguageList(new_aw_accept_language);
    last_http_accept_language_ =
        net::HttpUtil::GenerateAcceptLanguageHeader(new_aw_accept_language);
    last_aw_accept_language_ = new_aw_accept_language;
  }
  return last_http_accept_language_;
}

std::string AwHttpUserAgentSettings::GetUserAgent() const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  return android_webview::GetUserAgent();
}

}  // namespace android_webview
