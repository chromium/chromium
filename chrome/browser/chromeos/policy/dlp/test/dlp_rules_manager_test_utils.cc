// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/dlp/test/dlp_rules_manager_test_utils.h"

#include <utility>

#include "base/check.h"

namespace policy::dlp_test_util {

namespace {

const char kName[] = "name";
const char kDescription[] = "description";
const char kRuleId[] = "rule_id";
const char kSources[] = "sources";
const char kUrls[] = "urls";
const char kDestinations[] = "destinations";
const char kComponents[] = "components";
const char kRestrictions[] = "restrictions";
const char kClass[] = "class";
const char kLevel[] = "level";

}  // namespace

base::Value::Dict CreateSources(base::Value::List urls) {
  return base::Value::Dict().Set(kUrls, std::move(urls));
}

base::Value::Dict CreateDestinations(
    std::optional<base::Value::List> urls,
    std::optional<base::Value::List> components) {
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
  return base::Value::Dict().Set(kClass, restriction).Set(kLevel, level);
}

base::Value::Dict CreateRule(const std::string& name,
                             const std::string& desc,
                             const std::string& rule_id,
                             base::Value::List src_urls,
                             std::optional<base::Value::List> dst_urls,
                             std::optional<base::Value::List> dst_components,
                             base::Value::List restrictions) {
  auto rule =
      base::Value::Dict()
          .Set(kName, name)
          .Set(kDescription, desc)
          .Set(kSources, CreateSources(std::move(src_urls)))
          .Set(kDestinations, CreateDestinations(std::move(dst_urls),
                                                 std::move(dst_components)))
          .Set(kRestrictions, std::move(restrictions));
  if (!rule_id.empty()) {
    rule.Set(kRuleId, rule_id);
  }
  return rule;
}

DlpRule::DlpRule(const std::string& name,
                 const std::string& description,
                 const std::string& id)
    : name(name), description(description), id(id) {}
DlpRule::DlpRule() = default;
DlpRule::~DlpRule() = default;
DlpRule::DlpRule(const DlpRule& other) = default;
DlpRule& DlpRule::AddSrcUrl(const std::string& url) {
  src_urls.emplace_back(url);
  return *this;
}
DlpRule& DlpRule::AddDstUrl(const std::string& url) {
  dst_urls.emplace_back(url);
  return *this;
}
DlpRule& DlpRule::AddDstComponent(const std::string& component) {
  dst_components.emplace_back(component);
  return *this;
}
DlpRule& DlpRule::AddRestriction(const std::string& type,
                                 const std::string& level) {
  restrictions.emplace_back(type, level);
  return *this;
}

base::Value::Dict DlpRule::Create() const {
  base::Value::List src_urls_list;
  for (const std::string& src : src_urls) {
    src_urls_list.Append(src);
  }

  base::Value::List dst_urls_list;
  for (const std::string& dst : dst_urls) {
    dst_urls_list.Append(dst);
  }

  base::Value::List dst_components_list;
  for (const std::string& component : dst_components) {
    dst_components_list.Append(component);
  }

  base::Value::List restrictions_list;
  for (const auto& [type, level] : restrictions) {
    restrictions_list.Append(
        base::Value::Dict().Set("class", type).Set("level", level));
  }

  return CreateRule(name, description, id, std::move(src_urls_list),
                    std::move(dst_urls_list), std::move(dst_components_list),
                    std::move(restrictions_list));
}

}  // namespace policy::dlp_test_util
