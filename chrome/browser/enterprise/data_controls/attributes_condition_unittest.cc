// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/data_controls/attributes_condition.h"

#include <vector>

#include "base/json/json_reader.h"
#include "base/strings/stringprintf.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace data_controls {

namespace {

constexpr char kGoogleUrl[] = "https://google.com";
constexpr char kChromiumUrl[] = "https://chromium.org";

base::Value CreateDict(const std::string& value) {
  auto dict = base::JSONReader::Read(value, base::JSON_ALLOW_TRAILING_COMMAS);
  EXPECT_TRUE(dict.has_value());
  return std::move(dict.value());
}

}  // namespace

TEST(AttributesConditionTest, InvalidInputs) {
  // Invalid JSON types are rejected.
  ASSERT_FALSE(AttributesCondition::Create(base::Value("some string")));
  ASSERT_FALSE(AttributesCondition::Create(base::Value(12345)));
  ASSERT_FALSE(AttributesCondition::Create(base::Value(99.999)));
  ASSERT_FALSE(AttributesCondition::Create(
      base::Value(std::vector<char>({1, 2, 3, 4, 5}))));

  // Invalid dictionaries are rejected.
  ASSERT_FALSE(AttributesCondition::Create(base::Value::Dict()));
  ASSERT_FALSE(AttributesCondition::Create(CreateDict(R"({"foo": 1})")));

  // Dictionaries with correct keys but wrong schema for values are rejected
  ASSERT_FALSE(AttributesCondition::Create(
      CreateDict(R"({"urls": "https://foo.com"})")));
  ASSERT_FALSE(AttributesCondition::Create(CreateDict(R"({"urls": 1})")));
  ASSERT_FALSE(AttributesCondition::Create(CreateDict(R"({"urls": 99.999})")));
#if BUILDFLAG(IS_CHROMEOS)
  ASSERT_FALSE(
      AttributesCondition::Create(CreateDict(R"({"urls": "https://foo.com",
                                                    "components": "ARC"})")));
  ASSERT_FALSE(AttributesCondition::Create(CreateDict(R"({"urls": 1,
                                                    "components": "ARC"})")));
  ASSERT_FALSE(AttributesCondition::Create(CreateDict(R"({"urls": 99.999,
                                                    "components": "ARC"})")));
  ASSERT_FALSE(
      AttributesCondition::Create(CreateDict(R"({"components": "ARC"})")));
  ASSERT_FALSE(
      AttributesCondition::Create(CreateDict(R"({"components": 12345})")));
  ASSERT_FALSE(
      AttributesCondition::Create(CreateDict(R"({"components": 99.999})")));
#endif  // BUILDFLAG(IS_CHROMEOS)

  // Dictionaries with valid schemas but invalid URL patterns or components are
  // rejected.
  ASSERT_FALSE(
      AttributesCondition::Create(CreateDict(R"({"urls": ["http://:port"]})")));
  ASSERT_FALSE(AttributesCondition::Create(
      CreateDict(R"({"urls": ["http://?query"]})")));
  ASSERT_FALSE(
      AttributesCondition::Create(CreateDict(R"({"urls": ["https://"]})")));
  ASSERT_FALSE(AttributesCondition::Create(CreateDict(R"({"urls": ["//"]})")));
  ASSERT_FALSE(
      AttributesCondition::Create(CreateDict(R"({"urls": ["a", 1]})")));
#if BUILDFLAG(IS_CHROMEOS)
  ASSERT_FALSE(AttributesCondition::Create(CreateDict(R"({"urls": ["a", 1],
                                                    "components": ["ARC"]})")));
  ASSERT_FALSE(
      AttributesCondition::Create(CreateDict(R"({"components": ["1", "a"]})")));
  ASSERT_FALSE(
      AttributesCondition::Create(CreateDict(R"({"components": ["5.5"]})")));
#endif  // BUILDFLAG(IS_CHROMEOS)
}

TEST(AttributesConditionTest, AnyURL) {
  auto any_url = AttributesCondition::Create(CreateDict(R"({"urls": ["*"]})"));
  ASSERT_TRUE(any_url);
  for (const char* url : {kGoogleUrl, kChromiumUrl}) {
    ActionContext context = {.url = GURL(url)};
    ASSERT_TRUE(any_url->IsTriggered(context));
  }
}

TEST(AttributesConditionTest, SpecificURL) {
  auto google_url =
      AttributesCondition::Create(CreateDict(R"({"urls": ["google.com"]})"));
  auto chromium_url =
      AttributesCondition::Create(CreateDict(R"({"urls": ["chromium.org"]})"));

  ASSERT_TRUE(google_url);
  ASSERT_TRUE(chromium_url);

  ASSERT_TRUE(google_url->IsTriggered({.url = GURL(kGoogleUrl)}));
  ASSERT_TRUE(chromium_url->IsTriggered({.url = GURL(kChromiumUrl)}));

  ASSERT_FALSE(google_url->IsTriggered({.url = GURL(kChromiumUrl)}));
  ASSERT_FALSE(chromium_url->IsTriggered({.url = GURL(kGoogleUrl)}));
}

#if BUILDFLAG(IS_CHROMEOS)
TEST(AttributesConditionTest, AllComponents) {
  auto any_component = AttributesCondition::Create(CreateDict(R"(
      {
        "components": ["ARC", "CROSTINI", "PLUGIN_VM", "USB", "DRIVE",
                       "ONEDRIVE"]
      })"));
  ASSERT_TRUE(any_component);
  for (Component component : kAllComponents) {
    ActionContext context = {.component = component};
    ASSERT_TRUE(any_component->IsTriggered(context));
  }
}

TEST(AttributesConditionTest, OneComponent) {
  for (Component condition_component : kAllComponents) {
    constexpr char kTemplate[] = R"({"components": ["%s"]})";
    auto one_component =
        AttributesCondition::Create(CreateDict(base::StringPrintf(
            kTemplate, GetComponentMapping(condition_component).c_str())));

    for (Component context_component : kAllComponents) {
      ActionContext context = {.component = context_component};
      if (context_component == condition_component) {
        ASSERT_TRUE(one_component->IsTriggered(context));
      } else {
        ASSERT_FALSE(one_component->IsTriggered(context));
      }
    }
  }
}

TEST(AttributesConditionTest, URLAndAllComponents) {
  auto any_component_or_url = AttributesCondition::Create(CreateDict(R"(
      {
        "urls": ["*"],
        "components": ["ARC", "CROSTINI", "PLUGIN_VM", "USB", "DRIVE",
                       "ONEDRIVE"]
      })"));
  ASSERT_TRUE(any_component_or_url);
  for (Component component : kAllComponents) {
    for (const char* url : {kGoogleUrl, kChromiumUrl}) {
      ActionContext context = {.url = GURL(url), .component = component};
      ASSERT_TRUE(any_component_or_url->IsTriggered(context));
    }
  }
}

TEST(AttributesConditionTest, URLAndOneComponent) {
  for (Component condition_component : kAllComponents) {
    constexpr char kTemplate[] =
        R"({"urls": ["google.com"], "components": ["%s"]})";
    auto google_and_one_component =
        AttributesCondition::Create(CreateDict(base::StringPrintf(
            kTemplate, GetComponentMapping(condition_component).c_str())));

    ASSERT_TRUE(google_and_one_component);
    for (Component context_component : kAllComponents) {
      for (const char* url : {kGoogleUrl, kChromiumUrl}) {
        ActionContext context = {.url = GURL(url),
                                 .component = context_component};
        if (context_component == condition_component && url == kGoogleUrl) {
          ASSERT_TRUE(google_and_one_component->IsTriggered(context))
              << "Expected " << GetComponentMapping(context_component)
              << " to trigger for " << url;
        } else {
          ASSERT_FALSE(google_and_one_component->IsTriggered(context))
              << "Expected " << GetComponentMapping(context_component)
              << " to not trigger for " << url;
        }
      }
    }
  }
}
#endif  // BUILDFLAG(IS_CHROMEOS)

}  // namespace data_controls
