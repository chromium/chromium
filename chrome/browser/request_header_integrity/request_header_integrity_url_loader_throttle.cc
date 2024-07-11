// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/request_header_integrity/request_header_integrity_url_loader_throttle.h"

#include <string>

#include "base/strings/string_util.h"
#include "build/branding_buildflags.h"
#include "chrome/common/channel_info.h"
#include "components/google/core/common/google_util.h"
#include "services/network/public/cpp/resource_request.h"

#if !defined(CHANNEL_NAME_HEADER_NAME)
#define CHANNEL_NAME_HEADER_NAME "X-Placeholder-1"
#endif

namespace request_header_integrity {

namespace {

// Returns extended, stable, beta, dev, or canary if a channel is available,
// otherwise the empty string.
std::string GetChannelName() {
  std::string channel_name =
      chrome::GetChannelName(chrome::WithExtendedStable(true));

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  if (channel_name.empty()) {
    // For branded builds, stable is represented as the empty string.
    channel_name = "stable";
  }
#endif

  if (base::ToLowerASCII(channel_name) == "unknown") {
    return "";
  }

  return channel_name;
}

}  // namespace

RequestHeaderIntegrityURLLoaderThrottle::
    RequestHeaderIntegrityURLLoaderThrottle() = default;

RequestHeaderIntegrityURLLoaderThrottle::
    ~RequestHeaderIntegrityURLLoaderThrottle() = default;

void RequestHeaderIntegrityURLLoaderThrottle::WillStartRequest(
    network::ResourceRequest* request,
    bool* defer) {
  if (!google_util::IsGoogleAssociatedDomainUrl(request->url)) {
    return;
  }

  std::string channel_name = GetChannelName();
  if (!channel_name.empty()) {
    request->headers.SetHeader(CHANNEL_NAME_HEADER_NAME, channel_name);
  }
}

}  // namespace request_header_integrity
