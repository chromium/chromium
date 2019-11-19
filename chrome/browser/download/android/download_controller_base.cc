// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/android/download_controller_base.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "net/url_request/url_request.h"

const char kOMADrmMessageMimeType[] = "application/vnd.oma.drm.message";
const char kOMADrmContentMimeType[] = "application/vnd.oma.drm.content";
const char kOMADrmRightsMimeType1[] = "application/vnd.oma.drm.rights+xml";
const char kOMADrmRightsMimeType2[] = "application/vnd.oma.drm.rights+wbxml";

content::WebContents* GetWebContents(int render_process_id,
                                     int render_view_id) {
  content::RenderViewHost* render_view_host =
      content::RenderViewHost::FromID(render_process_id, render_view_id);

  if (!render_view_host)
    return nullptr;

  return content::WebContents::FromRenderViewHost(render_view_host);
}

// static
DownloadControllerBase* DownloadControllerBase::download_controller_ = nullptr;

DownloadInfo::DownloadInfo(const GURL& url,
                           const GURL& original_url,
                           const std::string& content_disposition,
                           const std::string& original_mime_type,
                           const std::string& user_agent,
                           const std::string& cookie,
                           const std::string& referer)
    : url(url),
      original_url(original_url),
      content_disposition(content_disposition),
      original_mime_type(original_mime_type),
      user_agent(user_agent),
      cookie(cookie),
      referer(referer) {}

DownloadInfo::DownloadInfo(const DownloadInfo& other) = default;

DownloadInfo::~DownloadInfo() {}
