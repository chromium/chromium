// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/preference/autofill_settings_transformer.h"

#include <string>
#include <utility>

#include "base/values.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {
namespace {

TEST(AutofillSettingsTransformerTest, Transform) {
  AutofillSettingsTransformer transformer;
  std::string error;
  bool bad_message = false;
  auto browser_pref = transformer.ExtensionToBrowserPref(
      base::Value(base::ListValue().Append(
          base::DictValue()
              .Set("urlPattern", "https://example.com")
              .Set("blockedTypes", base::ListValue().Append("contact_info")))),
      error, bad_message);
  ASSERT_TRUE(browser_pref);
  EXPECT_FALSE(bad_message);

  const auto& browser_rule = browser_pref->GetList()[0].GetDict();
  EXPECT_EQ("https://example.com",
            *browser_rule.FindString(
                autofill::prefs::kAutofillBlockedTypesUrlPatternKey));
  EXPECT_EQ("contact_info",
            (*browser_rule.FindList(
                autofill::prefs::kAutofillBlockedTypesBlockedTypesKey))[0]
                .GetString());

  auto back_to_ext = transformer.BrowserToExtensionPref(*browser_pref, false);
  ASSERT_TRUE(back_to_ext);
  const auto& back_rule = back_to_ext->GetList()[0].GetDict();
  EXPECT_EQ("https://example.com", *back_rule.FindString("urlPattern"));
  EXPECT_EQ("contact_info",
            (*back_rule.FindList("blockedTypes"))[0].GetString());
}

TEST(AutofillSettingsTransformerTest, ExtensionToBrowserPref_NotList) {
  AutofillSettingsTransformer transformer;
  std::string error;
  bool bad_message = false;
  EXPECT_FALSE(transformer.ExtensionToBrowserPref(base::Value("invalid_type"),
                                                  error, bad_message));
  EXPECT_TRUE(bad_message);
}

TEST(AutofillSettingsTransformerTest, ExtensionToBrowserPref_ItemNotDict) {
  AutofillSettingsTransformer transformer;
  std::string error;
  bool bad_message = false;
  EXPECT_FALSE(transformer.ExtensionToBrowserPref(
      base::Value(base::ListValue().Append("not_a_dict")), error, bad_message));
  EXPECT_TRUE(bad_message);
}

TEST(AutofillSettingsTransformerTest,
     ExtensionToBrowserPref_MissingProperties) {
  AutofillSettingsTransformer transformer;
  std::string error;
  bool bad_message = false;
  EXPECT_FALSE(transformer.ExtensionToBrowserPref(
      base::Value(base::ListValue().Append(
          base::DictValue().Set("urlPattern", "https://example.com"))),
      error, bad_message));
  EXPECT_TRUE(bad_message);
}

TEST(AutofillSettingsTransformerTest, ExtensionToBrowserPref_MultipleRules) {
  AutofillSettingsTransformer transformer;
  std::string error;
  bool bad_message = false;
  base::ListValue blocked_types1;
  blocked_types1.Append("contact_info");
  blocked_types1.Append("payments");
  base::ListValue blocked_types2;
  blocked_types2.Append("travel");
  blocked_types2.Append("all");

  auto browser_pref = transformer.ExtensionToBrowserPref(
      base::Value(
          base::ListValue()
              .Append(base::DictValue()
                          .Set("urlPattern", "https://a.com")
                          .Set("blockedTypes", std::move(blocked_types1)))
              .Append(base::DictValue()
                          .Set("urlPattern", "https://b.com")
                          .Set("blockedTypes", std::move(blocked_types2)))),
      error, bad_message);
  ASSERT_TRUE(browser_pref);
  EXPECT_FALSE(bad_message);
  ASSERT_EQ(2u, browser_pref->GetList().size());
  EXPECT_EQ("https://a.com",
            *browser_pref->GetList()[0].GetDict().FindString(
                autofill::prefs::kAutofillBlockedTypesUrlPatternKey));
  EXPECT_EQ(2u,
            browser_pref->GetList()[0]
                .GetDict()
                .FindList(autofill::prefs::kAutofillBlockedTypesBlockedTypesKey)
                ->size());
  EXPECT_EQ("https://b.com",
            *browser_pref->GetList()[1].GetDict().FindString(
                autofill::prefs::kAutofillBlockedTypesUrlPatternKey));
  EXPECT_EQ(2u,
            browser_pref->GetList()[1]
                .GetDict()
                .FindList(autofill::prefs::kAutofillBlockedTypesBlockedTypesKey)
                ->size());
  EXPECT_EQ("all",
            (*browser_pref->GetList()[1].GetDict().FindList(
                autofill::prefs::kAutofillBlockedTypesBlockedTypesKey))[1]
                .GetString());
}

TEST(AutofillSettingsTransformerTest, BrowserToExtensionPref_NotList) {
  AutofillSettingsTransformer transformer;
  EXPECT_FALSE(transformer.BrowserToExtensionPref(base::Value(123), false));
}

TEST(AutofillSettingsTransformerTest,
     BrowserToExtensionPref_SkipsInvalidItems) {
  AutofillSettingsTransformer transformer;
  base::ListValue blocked_types;
  blocked_types.Append("payments");

  auto ext_pref = transformer.BrowserToExtensionPref(
      base::Value(base::ListValue().Append(42).Append(
          base::DictValue()
              .Set(autofill::prefs::kAutofillBlockedTypesUrlPatternKey,
                   "https://valid.com")
              .Set(autofill::prefs::kAutofillBlockedTypesBlockedTypesKey,
                   std::move(blocked_types)))),
      false);
  ASSERT_TRUE(ext_pref);
  ASSERT_EQ(1u, ext_pref->GetList().size());
  EXPECT_EQ("https://valid.com",
            *ext_pref->GetList()[0].GetDict().FindString("urlPattern"));
}

TEST(AutofillSettingsTransformerTest,
     ExtensionToBrowserPref_InvalidUrlPattern) {
  AutofillSettingsTransformer transformer;
  std::string error;
  bool bad_message = false;
  auto browser_pref = transformer.ExtensionToBrowserPref(
      base::Value(base::ListValue().Append(
          base::DictValue()
              .Set("urlPattern", "example.com:abc")
              .Set("blockedTypes", base::ListValue().Append("contact_info")))),
      error, bad_message);
  EXPECT_FALSE(browser_pref);
  EXPECT_FALSE(bad_message);
  EXPECT_FALSE(error.empty());
}

}  // namespace
}  // namespace extensions
