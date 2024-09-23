// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/web_navigation/frame_navigation_state.h"

#include "base/check.h"
#include "base/containers/fixed_flat_set.h"
#include "base/notreached.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/render_frame_host.h"
#include "extensions/common/constants.h"

namespace extensions {


// static
bool FrameNavigationState::allow_extension_scheme_ = false;

DOCUMENT_USER_DATA_KEY_IMPL(FrameNavigationState);

FrameNavigationState::FrameNavigationState(
    content::RenderFrameHost* render_frame_host)
    : content::DocumentUserData<FrameNavigationState>(render_frame_host) {}
FrameNavigationState::~FrameNavigationState() = default;

// static
bool FrameNavigationState::IsValidUrl(const GURL& url) {
  constexpr auto kValidSchemes = base::MakeFixedFlatSet<std::string_view>({
      content::kChromeUIScheme,
      url::kHttpScheme,
      url::kHttpsScheme,
      url::kFileScheme,
      url::kFtpScheme,
      url::kJavaScriptScheme,
      url::kDataScheme,
      url::kFileSystemScheme,
  });

  if (kValidSchemes.contains(url.scheme_piece())) {
    return true;
  }

  // Allow about:blank and about:srcdoc.
  if (url.IsAboutBlank() || url.IsAboutSrcdoc()) {
    return true;
  }

  return allow_extension_scheme_ && url.scheme() == kExtensionScheme;
}

bool FrameNavigationState::CanSendEvents() const {
  return !error_occurred_ && IsValidUrl(url_);
}

void FrameNavigationState::StartTrackingDocumentLoad(
    const GURL& url,
    bool is_same_document,
    bool is_from_back_forward_cache,
    bool is_error_page) {
  error_occurred_ = is_error_page;
  url_ = url;
  if (!is_same_document && !is_from_back_forward_cache) {
    is_loading_ = true;
    is_parsing_ = true;
  }
}

GURL FrameNavigationState::GetUrl() const {
  return url_;
}

void FrameNavigationState::SetErrorOccurredInFrame() {
  error_occurred_ = true;
}

bool FrameNavigationState::GetErrorOccurredInFrame() const {
  return error_occurred_;
}

void FrameNavigationState::SetDocumentLoadCompleted() {
  is_loading_ = false;
}

bool FrameNavigationState::GetDocumentLoadCompleted() const {
  return !is_loading_;
}

void FrameNavigationState::SetParsingFinished() {
  is_parsing_ = false;
}

bool FrameNavigationState::GetParsingFinished() const {
  return !is_parsing_;
}

}  // namespace extensions
