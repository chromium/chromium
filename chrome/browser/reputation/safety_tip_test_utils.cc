// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/reputation/safety_tip_test_utils.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "chrome/browser/reputation/safety_tips_config.h"

namespace {

// Retrieve existing config proto if set, or create a new one otherwise.
std::unique_ptr<chrome_browser_safety_tips::SafetyTipsConfig> GetConfig() {
  auto* old = GetSafetyTipsRemoteConfigProto();
  if (old) {
    return std::make_unique<chrome_browser_safety_tips::SafetyTipsConfig>(*old);
  }

  auto conf = std::make_unique<chrome_browser_safety_tips::SafetyTipsConfig>();
  // Any version ID will do.
  conf->set_version_id(4);
  return conf;
}

}  // namespace

void InitializeSafetyTipConfig() {
  SetSafetyTipsRemoteConfigProto(GetConfig());
}

void SetSafetyTipPatternsWithFlagType(
    std::vector<std::string> patterns,
    chrome_browser_safety_tips::FlaggedPage::FlagType type) {
  auto config_proto = GetConfig();
  std::sort(patterns.begin(), patterns.end());
  for (const auto& pattern : patterns) {
    chrome_browser_safety_tips::FlaggedPage* page =
        config_proto->add_flagged_page();
    page->set_pattern(pattern);
    page->set_type(type);
  }

  SetSafetyTipsRemoteConfigProto(std::move(config_proto));
}

void SetSafetyTipBadRepPatterns(std::vector<std::string> patterns) {
  SetSafetyTipPatternsWithFlagType(
      patterns, chrome_browser_safety_tips::FlaggedPage::BAD_REP);
}

void SetSafetyTipAllowlistPatterns(std::vector<std::string> patterns) {
  auto config_proto = GetConfig();
  std::sort(patterns.begin(), patterns.end());
  for (const auto& pattern : patterns) {
    chrome_browser_safety_tips::UrlPattern* page =
        config_proto->add_allowed_pattern();
    page->set_pattern(pattern);
  }
  SetSafetyTipsRemoteConfigProto(std::move(config_proto));
}
