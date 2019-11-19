// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/web_navigation/frame_navigation_state.h"

#include "base/logging.h"
#include "base/stl_util.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/render_frame_host.h"
#include "extensions/common/constants.h"

namespace extensions {

namespace {

// URL schemes for which we'll send events.
const char* const kValidSchemes[] = {
    content::kChromeUIScheme,
    url::kHttpScheme,
    url::kHttpsScheme,
    url::kFileScheme,
    url::kFtpScheme,
    url::kJavaScriptScheme,
    url::kDataScheme,
    url::kFileSystemScheme,
};

}  // namespace

FrameNavigationState::FrameState::FrameState() {
  error_occurred = false;
  is_loading = false;
  is_parsing = false;
}

// static
bool FrameNavigationState::allow_extension_scheme_ = false;

FrameNavigationState::FrameNavigationState() {
}

FrameNavigationState::~FrameNavigationState() {}

// static
bool FrameNavigationState::IsValidUrl(const GURL& url) {
  for (unsigned i = 0; i < base::size(kValidSchemes); ++i) {
    if (url.scheme() == kValidSchemes[i])
      return true;
  }
  // Allow about:blank and about:srcdoc.
  if (url.IsAboutBlank() || url.IsAboutSrcdoc())
    return true;
  return allow_extension_scheme_ && url.scheme() == kExtensionScheme;
}

bool FrameNavigationState::CanSendEvents(
    content::RenderFrameHost* frame_host) const {
  auto it = frame_host_state_map_.find(frame_host);
  if (it == frame_host_state_map_.end() || it->second.error_occurred) {
    return false;
  }
  return IsValidUrl(it->second.url);
}

void FrameNavigationState::StartTrackingDocumentLoad(
    content::RenderFrameHost* frame_host,
    const GURL& url,
    bool is_same_document,
    bool is_error_page) {
  FrameState& frame_state = frame_host_state_map_[frame_host];
  frame_state.error_occurred = is_error_page;
  frame_state.url = url;
  if (!is_same_document) {
    frame_state.is_loading = true;
    frame_state.is_parsing = true;
  }
}

void FrameNavigationState::FrameHostCreated(
    content::RenderFrameHost* frame_host) {
  frame_hosts_.insert(frame_host);
}

void FrameNavigationState::FrameHostDeleted(
    content::RenderFrameHost* frame_host) {
  frame_host_state_map_.erase(frame_host);
  frame_hosts_.erase(frame_host);
}

bool FrameNavigationState::IsValidFrame(
    content::RenderFrameHost* frame_host) const {
  return frame_host_state_map_.find(frame_host) != frame_host_state_map_.end();
}

GURL FrameNavigationState::GetUrl(content::RenderFrameHost* frame_host) const {
  auto it = frame_host_state_map_.find(frame_host);
  if (it == frame_host_state_map_.end())
    return GURL();

  return it->second.url;
}

void FrameNavigationState::SetErrorOccurredInFrame(
    content::RenderFrameHost* frame_host) {
  auto it = frame_host_state_map_.find(frame_host);
  if (it == frame_host_state_map_.end()) {
    NOTREACHED();
    return;
  }
  it->second.error_occurred = true;
}

bool FrameNavigationState::GetErrorOccurredInFrame(
    content::RenderFrameHost* frame_host) const {
  auto it = frame_host_state_map_.find(frame_host);
  DCHECK(it != frame_host_state_map_.end());
  return it == frame_host_state_map_.end() || it->second.error_occurred;
}

void FrameNavigationState::SetDocumentLoadCompleted(
    content::RenderFrameHost* frame_host) {
  auto it = frame_host_state_map_.find(frame_host);
  if (it == frame_host_state_map_.end()) {
    NOTREACHED();
    return;
  }
  it->second.is_loading = false;
}

bool FrameNavigationState::GetDocumentLoadCompleted(
    content::RenderFrameHost* frame_host) const {
  auto it = frame_host_state_map_.find(frame_host);
  DCHECK(it != frame_host_state_map_.end());
  return it == frame_host_state_map_.end() || !it->second.is_loading;
}

void FrameNavigationState::SetParsingFinished(
    content::RenderFrameHost* frame_host) {
  auto it = frame_host_state_map_.find(frame_host);
  if (it == frame_host_state_map_.end()) {
    NOTREACHED();
    return;
  }
  it->second.is_parsing = false;
}

bool FrameNavigationState::GetParsingFinished(
    content::RenderFrameHost* frame_host) const {
  auto it = frame_host_state_map_.find(frame_host);
  DCHECK(it != frame_host_state_map_.end());
  return it == frame_host_state_map_.end() || !it->second.is_parsing;
}

}  // namespace extensions
