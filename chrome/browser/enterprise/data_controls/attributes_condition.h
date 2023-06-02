// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_DATA_CONTROLS_ATTRIBUTES_CONDITION_H_
#define CHROME_BROWSER_ENTERPRISE_DATA_CONTROLS_ATTRIBUTES_CONDITION_H_

#include <memory>

#include "base/values.h"
#include "chrome/browser/enterprise/data_controls/condition.h"
#include "components/url_matcher/url_matcher.h"

#if BUILDFLAG(IS_CHROMEOS)
#include <set>

#include "chrome/browser/enterprise/data_controls/component.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace data_controls {

// Implementation of the "root" level condition of a Data Control policy, which
// evaluates all the attributes in an `ActionContext`. This class is a
// representation of the following JSON:
// {
//   urls: [string],
//   components: [ARC|CROSTINI|PLUGIN_VM|DRIVE|USB], <= CrOS only
// }
// This can represent either the `sources` or `destinations` fields of the
// DataLeakPreventionRulesList policy.
class AttributesCondition : public Condition {
 public:
  AttributesCondition();
  virtual ~AttributesCondition();

  // Returns nullptr if the passed JSON doesn't match the expected schema.
  static std::unique_ptr<AttributesCondition> Create(const base::Value& value);
  static std::unique_ptr<AttributesCondition> Create(
      const base::Value::Dict& value);

  // Condition:
  bool IsTriggered(const ActionContext& action_context) const override;

 private:
  // Returns true if at least one of the internal values is non-null/empty.
  bool IsValid() const;

  std::unique_ptr<url_matcher::URLMatcher> url_matcher_;
#if BUILDFLAG(IS_CHROMEOS)
  std::set<Component> components_;
#endif  // BUILDFLAG(IS_CHROMEOS)
};

}  // namespace data_controls

#endif  // CHROME_BROWSER_ENTERPRISE_DATA_CONTROLS_ATTRIBUTES_CONDITION_H_
