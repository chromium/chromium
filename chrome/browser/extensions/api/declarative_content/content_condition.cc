// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/declarative_content/content_condition.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "extensions/common/api/declarative/declarative_constants.h"

namespace extensions {

namespace {

// TODO(jyasskin): improve error messaging to give more meaningful messages
// to the extension developer.
// Error messages:
const char kExpectedDictionary[] = "A condition has to be a dictionary.";
const char kConditionWithoutInstanceType[] = "A condition had no instanceType";
const char kExpectedOtherConditionType[] = "Expected a condition of type "
    "declarativeContent.PageStateMatcher";
const char kUnknownConditionAttribute[] = "Unknown condition attribute '%s'";

}  // namespace

ContentCondition::ContentCondition(
    std::vector<std::unique_ptr<const ContentPredicate>> predicates)
    : predicates(std::move(predicates)) {}

ContentCondition::~ContentCondition() {}

std::unique_ptr<ContentCondition> CreateContentCondition(
    const Extension* extension,
    const std::map<std::string, ContentPredicateFactory*>& predicate_factories,
    const base::Value& api_condition,
    std::string* error) {
  const base::DictionaryValue* api_condition_dict = nullptr;
  if (!api_condition.GetAsDictionary(&api_condition_dict)) {
    *error = kExpectedDictionary;
    return nullptr;
  }

  // Verify that we are dealing with a Condition whose type we understand.
  std::string instance_type;
  if (!api_condition_dict->GetString(
          declarative_content_constants::kInstanceType, &instance_type)) {
    *error = kConditionWithoutInstanceType;
    return nullptr;
  }
  if (instance_type != declarative_content_constants::kPageStateMatcherType) {
    *error = kExpectedOtherConditionType;
    return nullptr;
  }

  std::vector<std::unique_ptr<const ContentPredicate>> predicates;
  for (base::DictionaryValue::Iterator iter(*api_condition_dict);
       !iter.IsAtEnd(); iter.Advance()) {
    const std::string& predicate_name = iter.key();
    const base::Value& predicate_value = iter.value();
    if (predicate_name == declarative_content_constants::kInstanceType)
      continue;

    const auto loc = predicate_factories.find(predicate_name);
    if (loc != predicate_factories.end())
      predicates.push_back(loc->second->CreatePredicate(extension,
                                                        predicate_value,
                                                        error));
    else
      *error = base::StringPrintf(kUnknownConditionAttribute,
                                  predicate_name.c_str());

    if (!error->empty())
      return nullptr;
  }

  return std::make_unique<ContentCondition>(std::move(predicates));
}

}  // namespace extensions
