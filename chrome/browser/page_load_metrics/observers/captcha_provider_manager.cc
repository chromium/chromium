// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/captcha_provider_manager.h"

#include <string_view>

#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "net/base/url_util.h"
#include "url/gurl.h"

namespace page_load_metrics {

namespace {

constexpr char kReCaptchaUrlSubstring[] = "google.com/recaptcha/";
constexpr char kReCaptchaNetUrlSubstring[] = "recaptcha.net/recaptcha/";
constexpr char kHCaptchaUrlSubstring[] = "hcaptcha.com/";
constexpr char kCloudflareTurnstileUrlSubstring[] =
    "challenges.cloudflare.com/";

CaptchaProvider GetCaptchaProviderForUrlPattern(std::string_view url_pattern) {
  if (url_pattern.find(kReCaptchaUrlSubstring) != std::string::npos) {
    return CaptchaProvider::kReCaptcha;
  }
  if (url_pattern.find(kReCaptchaNetUrlSubstring) != std::string::npos) {
    return CaptchaProvider::kReCaptcha;
  }
  if (url_pattern.find(kHCaptchaUrlSubstring) != std::string::npos) {
    return CaptchaProvider::kHCaptcha;
  }
  if (url_pattern.find(kCloudflareTurnstileUrlSubstring) != std::string::npos) {
    return CaptchaProvider::kCloudflareTurnstile;
  }
  return CaptchaProvider::kUnknown;
}

}  // namespace

// static
CaptchaProviderManager* CaptchaProviderManager::GetInstance() {
  static base::NoDestructor<CaptchaProviderManager> instance;
  return instance.get();
}

// static
CaptchaProviderManager CaptchaProviderManager::CreateForTesting() {  // IN-TEST
  return CaptchaProviderManager();
}

CaptchaProviderManager::CaptchaProviderManager() {
  // This class is allowed to be instantiated on any thread.
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

CaptchaProviderManager::~CaptchaProviderManager() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

bool CaptchaProviderManager::loaded() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return is_loaded_;
}

bool CaptchaProviderManager::empty() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return captcha_provider_patterns_.empty();
}

void CaptchaProviderManager::SetCaptchaProviders(
    const std::vector<std::string>& captcha_providers) {
  DETACH_FROM_SEQUENCE(sequence_checker_);

  for (std::string_view captcha_provider : captcha_providers) {
    UrlPattern pattern;
    pattern.provider = GetCaptchaProviderForUrlPattern(captcha_provider);

    // Split the captcha provider url pattern into host and path.
    auto slash_pos = captcha_provider.find_first_of('/');
    if (slash_pos != std::string::npos) {
      pattern.host = captcha_provider.substr(0, slash_pos);
      pattern.path = captcha_provider.substr(slash_pos);
    } else {
      pattern.host = captcha_provider;
    }

    // Patterns without a host are invalid.
    if (pattern.host.empty()) {
      continue;
    }

    // Remove the wildcards from the host and path if present and set the
    // corresponding flags.
    if (pattern.host.front() == '*') {
      pattern.has_subdomain_wildcard = true;
      pattern.host = std::move(pattern.host).substr(1);
    }
    if (!pattern.path.empty() && pattern.path.back() == '*') {
      pattern.has_path_wildcard = true;
      pattern.path = std::move(pattern.path).substr(0, pattern.path.size() - 1);
    }

    captcha_provider_patterns_.push_back(std::move(pattern));
  }

  is_loaded_ = true;
}

bool CaptchaProviderManager::IsCaptchaUrl(const GURL& url) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return GetCaptchaProviderForUrl(url).has_value();
}

std::optional<CaptchaProvider> CaptchaProviderManager::GetCaptchaProviderForUrl(
    const GURL& url) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  for (const UrlPattern& pattern : captcha_provider_patterns_) {
    // First, check if the host matches the pattern.
    std::string host(url.host());
    bool host_match = false;
    if (pattern.has_subdomain_wildcard) {
      while (!host.empty()) {
        if (host == pattern.host) {
          host_match = true;
          break;
        }
        host = net::GetSuperdomain(host);
      }
    } else {
      host_match = host == pattern.host;
    }

    if (!host_match) {
      continue;
    }

    // Second, check if the path matches the pattern.
    const bool path_exact_match = pattern.path == url.path();
    const bool path_wildcard_match =
        pattern.has_path_wildcard && url.path().starts_with(pattern.path);
    if (pattern.path.empty() || path_exact_match || path_wildcard_match) {
      return pattern.provider;
    }
  }

  return std::nullopt;
}

}  // namespace page_load_metrics
