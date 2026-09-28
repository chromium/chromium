// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/gemini_enterprise/geic_url_allowlist.h"

#include <string_view>

#include "url/gurl.h"

namespace geic {

bool IsAllowedGeminiEnterpriseHost(const GURL& url) {
  const std::string_view host = url.host();
  return host == "business.gemini.google" || host == "gemini.google.com" ||
         url.DomainIs("cloud.google.com") || url.DomainIs("cloud.google") ||
         url.DomainIs("corp.google.com");
}

}  // namespace geic
