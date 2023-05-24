// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/data_controls/attributes_condition.h"

#include "base/containers/contains.h"
#include "components/url_matcher/url_util.h"

namespace data_controls {

namespace {

// Constants used to parse sub-dictionaries of DLP policies that should map to
// an AttributesCondition.
constexpr char kKeyUrls[] = "urls";

#if BUILDFLAG(IS_CHROMEOS)
constexpr char kKeyComponents[] = "components";
#endif  // BUILDFLAG(IS_CHROMEOS)

}  // namespace

AttributesCondition::AttributesCondition() = default;
AttributesCondition::~AttributesCondition() = default;

// static
std::unique_ptr<AttributesCondition> AttributesCondition::Create(
    const base::Value& value) {
  if (!value.is_dict()) {
    return nullptr;
  }

  return AttributesCondition::Create(value.GetDict());
}

// static
std::unique_ptr<AttributesCondition> AttributesCondition::Create(
    const base::Value::Dict& value) {
  auto attributes_condition = std::make_unique<AttributesCondition>();

  const base::Value::List* urls_value = value.FindList(kKeyUrls);
  if (urls_value) {
    for (const base::Value& url_pattern : *urls_value) {
      if (!url_pattern.is_string()) {
        return nullptr;
      }
    }

    auto url_matcher = std::make_unique<url_matcher::URLMatcher>();
    base::MatcherStringPattern::ID id(0);
    url_matcher::util::AddFilters(url_matcher.get(), true, &id, *urls_value);

    if (!url_matcher->IsEmpty()) {
      attributes_condition->url_matcher_ = std::move(url_matcher);
    }
  }

#if BUILDFLAG(IS_CHROMEOS)
  const base::Value::List* components_value = value.FindList(kKeyComponents);
  if (components_value) {
    std::set<Component> components;
    for (const auto& component_string : *components_value) {
      if (!component_string.is_string()) {
        continue;
      }

      Component component = GetComponentMapping(component_string.GetString());
      if (component != Component::kUnknownComponent) {
        components.insert(component);
      }
    }
    attributes_condition->components_ = std::move(components);
  }
#endif  // BUILDFLAG(IS_CHROMEOS)

  if (attributes_condition->IsValid()) {
    return attributes_condition;
  }

  return nullptr;
}

bool AttributesCondition::IsTriggered(
    const ActionContext& action_context) const {
#if BUILDFLAG(IS_CHROMEOS)
  if (!components_.empty() &&
      !base::Contains(components_, action_context.component)) {
    return false;
  }
#endif
  if (url_matcher_ && action_context.url.is_valid() &&
      url_matcher_->MatchURL(action_context.url).empty()) {
    return false;
  }
  return true;
}

bool AttributesCondition::IsValid() const {
#if BUILDFLAG(IS_CHROMEOS)
  return (url_matcher_ && !url_matcher_->IsEmpty()) || !components_.empty();
#else
  return url_matcher_ && !url_matcher_->IsEmpty();
#endif  // BUILDFLAG(IS_CHROMEOS)
}

}  // namespace data_controls
