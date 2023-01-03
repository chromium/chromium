// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager_test_utils.h"

#include <utility>

#include "base/check.h"

namespace policy {

namespace dlp_test_util {

namespace {

const char kName[] = "name";
const char kDescription[] = "description";
const char kSources[] = "sources";
const char kUrls[] = "urls";
const char kDestinations[] = "destinations";
const char kComponents[] = "components";
const char kRestrictions[] = "restrictions";
const char kClass[] = "class";
const char kLevel[] = "level";

}  // namespace

base::Value::Dict CreateSources(base::Value::List urls) {
  base::Value::Dict srcs;
  srcs.Set(kUrls, std::move(urls));
  return srcs;
}

base::Value::Dict CreateDestinations(
    absl::optional<base::Value::List> urls,
    absl::optional<base::Value::List> components) {
  base::Value::Dict dsts;
  if (urls.has_value()) {
    dsts.Set(kUrls, std::move(urls.value()));
  }
  if (components.has_value()) {
    dsts.Set(kComponents, std::move(components.value()));
  }
  return dsts;
}

base::Value::Dict CreateRestrictionWithLevel(const std::string& restriction,
                                             const std::string& level) {
  base::Value::Dict dict;
  dict.Set(kClass, restriction);
  dict.Set(kLevel, level);
  return dict;
}

base::Value::Dict CreateRule(const std::string& name,
                             const std::string& desc,
                             base::Value::List src_urls,
                             absl::optional<base::Value::List> dst_urls,
                             absl::optional<base::Value::List> dst_components,
                             base::Value::List restrictions) {
  base::Value::Dict rule;
  rule.Set(kName, name);
  rule.Set(kDescription, desc);
  rule.Set(kSources, CreateSources(std::move(src_urls)));
  rule.Set(kDestinations,
           CreateDestinations(std::move(dst_urls), std::move(dst_components)));
  rule.Set(kRestrictions, std::move(restrictions));
  return rule;
}

}  // namespace dlp_test_util

}  // namespace policy
