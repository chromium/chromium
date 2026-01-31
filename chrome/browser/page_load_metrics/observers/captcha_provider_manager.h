// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_CAPTCHA_PROVIDER_MANAGER_H_
#define CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_CAPTCHA_PROVIDER_MANAGER_H_

#include <string>
#include <vector>

#include "base/sequence_checker.h"
#include "url/gurl.h"

namespace page_load_metrics {

class CaptchaProviderManager {
 public:
  static CaptchaProviderManager* GetInstance();
  static CaptchaProviderManager CreateForTesting();

  CaptchaProviderManager();

  CaptchaProviderManager(const CaptchaProviderManager&) = delete;
  CaptchaProviderManager& operator=(const CaptchaProviderManager&) = delete;

  ~CaptchaProviderManager();

  void SetCaptchaProviders(const std::vector<std::string>& captcha_providers);
  bool IsCaptchaUrl(const GURL& url) const;
  bool loaded() const;
  bool empty() const;

 private:
  // The Captcha URL patterns will be set by the component updater. They will
  // include a host and a path with optional domain and path wildcards.
  //
  // Examples:
  //   domain.com
  //   domain.com/path/to/resource
  //   sub.domain.com/path/to/resource
  //   *domain.com/path/to/resource
  //   domain.com/path/*
  //   *domain.com/path/*
  //
  // The domain wildcard will match any subdomain of the specified domain (as
  // well as the specified domain itself), and the path wildcard will match any
  // URL path that starts with the specified path prefix.
  struct UrlPattern {
    std::string host = "";
    std::string path = "";
    bool has_subdomain_wildcard = false;
    bool has_path_wildcard = false;
  };

  std::vector<UrlPattern> captcha_provider_patterns_;
  bool is_loaded_ = false;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace page_load_metrics

#endif  // CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_CAPTCHA_PROVIDER_MANAGER_H_
