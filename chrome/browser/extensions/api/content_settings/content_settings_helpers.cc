// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/content_settings/content_settings_helpers.h"

#include <memory>

#include "base/notreached.h"
#include "components/content_settings/core/browser/website_settings_info.h"
#include "components/content_settings/core/browser/website_settings_registry.h"
#include "content/public/common/url_constants.h"
#include "extensions/common/url_pattern.h"

namespace {

const char kNoPathWildcardsError[] =
    "Path wildcards in file URL patterns are not allowed.";
const char kNoPathsError[] = "Specific paths are not allowed.";
const char kInvalidPatternError[] = "The pattern \"*\" is invalid.";

// TODO(bauerb): Move this someplace where it can be reused.
std::string GetDefaultPort(const std::string& scheme) {
  if (scheme == url::kHttpScheme)
    return "80";
  if (scheme == url::kHttpsScheme)
    return "443";
  NOTREACHED();
  return std::string();
}

}  // namespace

namespace extensions {
namespace content_settings_helpers {

ContentSettingsPattern ParseExtensionPattern(const std::string& pattern_str,
                                             std::string* error) {
  const int kAllowedSchemes =
      URLPattern::SCHEME_HTTP | URLPattern::SCHEME_HTTPS |
      URLPattern::SCHEME_FILE;
  URLPattern url_pattern(kAllowedSchemes);
  URLPattern::ParseResult result = url_pattern.Parse(pattern_str);
  if (result != URLPattern::ParseResult::kSuccess) {
    *error = URLPattern::GetParseResultString(result);
    return ContentSettingsPattern();
  } else {
    std::unique_ptr<ContentSettingsPattern::BuilderInterface> builder =
        ContentSettingsPattern::CreateBuilder();
    builder->WithHost(url_pattern.host());
    if (url_pattern.match_subdomains())
      builder->WithDomainWildcard();

    std::string scheme = url_pattern.scheme();
    if (scheme == "*")
      builder->WithSchemeWildcard();
    else
      builder->WithScheme(scheme);

    std::string port = url_pattern.port();
    if (port.empty() && scheme != "file") {
      if (scheme == "*")
        port = "*";
      else
        port = GetDefaultPort(scheme);
    }
    if (port == "*")
      builder->WithPortWildcard();
    else
      builder->WithPort(port);

    std::string path = url_pattern.path();
    if (scheme == "file") {
      // For file URLs we allow only exact path matches.
      if (path.find_first_of("*?") != std::string::npos) {
        *error = kNoPathWildcardsError;
        return ContentSettingsPattern();
      } else {
        builder->WithPath(path);
      }
    } else if (path != "/*") {
      // For other URLs we allow only paths which match everything.
      *error = kNoPathsError;
      return ContentSettingsPattern();
    }

    ContentSettingsPattern pattern = builder->Build();
    if (!pattern.IsValid())
      *error = kInvalidPatternError;
    return pattern;
  }
}

ContentSettingsType StringToContentSettingsType(
    const std::string& content_type) {
  const content_settings::WebsiteSettingsInfo* info =
      content_settings::WebsiteSettingsRegistry::GetInstance()->GetByName(
          content_type);
  if (info)
    return info->type();

  return ContentSettingsType::DEFAULT;
}

std::string ContentSettingsTypeToString(ContentSettingsType type) {
  return content_settings::WebsiteSettingsRegistry::GetInstance()
      ->Get(type)
      ->name();
}

}  // namespace content_settings_helpers
}  // namespace extensions
