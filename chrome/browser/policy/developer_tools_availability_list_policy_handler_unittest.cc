// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/developer_tools_availability_list_policy_handler.h"

#include <memory>
#include <string>
#include <utility>

#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "chrome/common/pref_names.h"
#include "components/policy/core/browser/configuration_policy_handler.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"
#include "components/strings/grit/components_strings.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace policy {

class DeveloperToolsAvailabilityListPolicyHandlerTest : public testing::Test {
 public:
  void SetUp() override {
    allowlist_handler_ =
        std::make_unique<DeveloperToolsAvailabilityListPolicyHandler>(
            key::kDeveloperToolsAvailabilityAllowlist,
            prefs::kDeveloperToolsAvailabilityAllowlist);
    blocklist_handler_ =
        std::make_unique<DeveloperToolsAvailabilityListPolicyHandler>(
            key::kDeveloperToolsAvailabilityBlocklist,
            prefs::kDeveloperToolsAvailabilityBlocklist);
  }

 protected:
  void SetPolicy(const std::string& key, base::Value value) {
    policies_.Set(key, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                  POLICY_SOURCE_CLOUD, std::move(value), nullptr);
  }

  base::ListValue GetUrlListWithEntries(size_t len) {
    base::ListValue list;
    for (size_t i = 0; i < len; ++i) {
      list.Append("http://example" + base::NumberToString(i) + ".com");
    }
    return list;
  }

  std::unique_ptr<DeveloperToolsAvailabilityListPolicyHandler>
      allowlist_handler_;
  std::unique_ptr<DeveloperToolsAvailabilityListPolicyHandler>
      blocklist_handler_;
  PolicyErrorMap errors_;
  PolicyMap policies_;
  PrefValueMap prefs_;
};

TEST_F(DeveloperToolsAvailabilityListPolicyHandlerTest,
       CheckPolicySettings_WrongType) {
  SetPolicy(key::kDeveloperToolsAvailabilityAllowlist, base::Value(false));
  EXPECT_TRUE(allowlist_handler_->CheckPolicySettings(policies_, &errors_));
  EXPECT_EQ(1U, errors_.size());

  errors_.Clear();
  SetPolicy(key::kDeveloperToolsAvailabilityBlocklist, base::Value("not-a-list"));
  EXPECT_TRUE(blocklist_handler_->CheckPolicySettings(policies_, &errors_));
  EXPECT_EQ(1U, errors_.size());
}

TEST_F(DeveloperToolsAvailabilityListPolicyHandlerTest,
       CheckPolicySettings_EmptyList) {
  SetPolicy(key::kDeveloperToolsAvailabilityAllowlist,
            base::Value(base::ListValue()));
  EXPECT_TRUE(allowlist_handler_->CheckPolicySettings(policies_, &errors_));
  EXPECT_EQ(0U, errors_.size());
}

TEST_F(DeveloperToolsAvailabilityListPolicyHandlerTest,
       CheckPolicySettings_MaxFiltersLimit) {
  const size_t max_filters = kMaxUrlFiltersPerPolicy;
  SetPolicy(key::kDeveloperToolsAvailabilityAllowlist,
            base::Value(GetUrlListWithEntries(max_filters + 1)));

  EXPECT_TRUE(allowlist_handler_->CheckPolicySettings(policies_, &errors_));
  EXPECT_EQ(1U, errors_.size());

  const std::u16string expected_warning = l10n_util::GetStringFUTF16(
      IDS_POLICY_URL_ALLOW_BLOCK_LIST_MAX_FILTERS_LIMIT_WARNING,
      base::NumberToString16(max_filters));
  const std::u16string actual_error =
      errors_.GetErrorMessages(key::kDeveloperToolsAvailabilityAllowlist);
  EXPECT_NE(std::u16string::npos, actual_error.find(expected_warning));
}

TEST_F(DeveloperToolsAvailabilityListPolicyHandlerTest,
       CheckPolicySettings_WrongElementType) {
  base::ListValue in;
  in.Append(false);
  in.Append(123);
  SetPolicy(key::kDeveloperToolsAvailabilityAllowlist,
            base::Value(std::move(in)));

  EXPECT_TRUE(allowlist_handler_->CheckPolicySettings(policies_, &errors_));
  EXPECT_EQ(1U, errors_.size());
}

TEST_F(DeveloperToolsAvailabilityListPolicyHandlerTest,
       CheckPolicySettings_InvalidHostWithAsterisk) {
  base::ListValue in;
  in.Append("*.developers.com");
  in.Append("developer.*.com");
  SetPolicy(key::kDeveloperToolsAvailabilityAllowlist,
            base::Value(std::move(in)));

  EXPECT_TRUE(allowlist_handler_->CheckPolicySettings(policies_, &errors_));
  EXPECT_EQ(1U, errors_.size());
}

TEST_F(DeveloperToolsAvailabilityListPolicyHandlerTest,
       CheckPolicySettings_ValidEntries) {
  base::ListValue in;
  in.Append("example.com");
  in.Append("https://ssl.server.com");
  in.Append("hosting.com/good_path");
  in.Append("https://server:8080/path");
  in.Append(".exact.hostname.com");
  in.Append("*");
  SetPolicy(key::kDeveloperToolsAvailabilityBlocklist,
            base::Value(std::move(in)));

  EXPECT_TRUE(blocklist_handler_->CheckPolicySettings(policies_, &errors_));
  EXPECT_EQ(0U, errors_.size());
}

TEST_F(DeveloperToolsAvailabilityListPolicyHandlerTest,
       ApplyPolicySettings_NothingSpecified) {
  allowlist_handler_->ApplyPolicySettings(policies_, &prefs_);
  EXPECT_FALSE(
      prefs_.GetValue(prefs::kDeveloperToolsAvailabilityAllowlist, nullptr));

  blocklist_handler_->ApplyPolicySettings(policies_, &prefs_);
  EXPECT_FALSE(
      prefs_.GetValue(prefs::kDeveloperToolsAvailabilityBlocklist, nullptr));
}

TEST_F(DeveloperToolsAvailabilityListPolicyHandlerTest,
       ApplyPolicySettings_WrongType) {
  SetPolicy(key::kDeveloperToolsAvailabilityAllowlist, base::Value(false));
  allowlist_handler_->ApplyPolicySettings(policies_, &prefs_);
  EXPECT_FALSE(
      prefs_.GetValue(prefs::kDeveloperToolsAvailabilityAllowlist, nullptr));
}

TEST_F(DeveloperToolsAvailabilityListPolicyHandlerTest,
       ApplyPolicySettings_EmptyList) {
  SetPolicy(key::kDeveloperToolsAvailabilityAllowlist,
            base::Value(base::ListValue()));
  allowlist_handler_->ApplyPolicySettings(policies_, &prefs_);
  EXPECT_FALSE(
      prefs_.GetValue(prefs::kDeveloperToolsAvailabilityAllowlist, nullptr));
}

TEST_F(DeveloperToolsAvailabilityListPolicyHandlerTest,
       ApplyPolicySettings_WrongElementType) {
  base::ListValue in;
  in.Append(false);
  in.Append(123);
  SetPolicy(key::kDeveloperToolsAvailabilityAllowlist,
            base::Value(std::move(in)));
  allowlist_handler_->ApplyPolicySettings(policies_, &prefs_);
  EXPECT_FALSE(
      prefs_.GetValue(prefs::kDeveloperToolsAvailabilityAllowlist, nullptr));
}

TEST_F(DeveloperToolsAvailabilityListPolicyHandlerTest,
       ApplyPolicySettings_FiltersInvalidUrls) {
  base::ListValue in;
  in.Append("example.com");
  in.Append("*.developers.com");
  in.Append("wsgi:///invalid.com");
  SetPolicy(key::kDeveloperToolsAvailabilityAllowlist,
            base::Value(std::move(in)));
  allowlist_handler_->ApplyPolicySettings(policies_, &prefs_);

  const base::Value* out = nullptr;
  EXPECT_TRUE(
      prefs_.GetValue(prefs::kDeveloperToolsAvailabilityAllowlist, &out));
  ASSERT_TRUE(out && out->is_list());
  ASSERT_EQ(1U, out->GetList().size());
  EXPECT_EQ("example.com", out->GetList()[0].GetString());
}

TEST_F(DeveloperToolsAvailabilityListPolicyHandlerTest,
       ApplyPolicySettings_MaxFiltersLimitExceeded) {
  const size_t max_filters = kMaxUrlFiltersPerPolicy;
  SetPolicy(key::kDeveloperToolsAvailabilityAllowlist,
            base::Value(GetUrlListWithEntries(max_filters + 10)));
  allowlist_handler_->ApplyPolicySettings(policies_, &prefs_);

  const base::Value* out = nullptr;
  EXPECT_TRUE(
      prefs_.GetValue(prefs::kDeveloperToolsAvailabilityAllowlist, &out));
  ASSERT_TRUE(out && out->is_list());
  EXPECT_EQ(max_filters, out->GetList().size());
}

TEST_F(DeveloperToolsAvailabilityListPolicyHandlerTest,
       ApplyPolicySettings_Successful_Allowlist) {
  base::ListValue allowlist;
  allowlist.Append("example.com");
  allowlist.Append("https://chromium.org");
  SetPolicy(key::kDeveloperToolsAvailabilityAllowlist,
            base::Value(std::move(allowlist)));
  allowlist_handler_->ApplyPolicySettings(policies_, &prefs_);

  const base::Value* out = nullptr;
  EXPECT_TRUE(
      prefs_.GetValue(prefs::kDeveloperToolsAvailabilityAllowlist, &out));
  ASSERT_TRUE(out && out->is_list());
  EXPECT_EQ(2U, out->GetList().size());
  EXPECT_EQ("example.com", out->GetList()[0].GetString());
  EXPECT_EQ("https://chromium.org", out->GetList()[1].GetString());
}

TEST_F(DeveloperToolsAvailabilityListPolicyHandlerTest,
       CheckPolicySettings_AllowlistRejectsWildcardAsterisk) {
  base::ListValue in;
  in.Append("*");
  SetPolicy(key::kDeveloperToolsAvailabilityAllowlist,
            base::Value(std::move(in)));

  EXPECT_TRUE(allowlist_handler_->CheckPolicySettings(policies_, &errors_));
  EXPECT_EQ(1U, errors_.size());
}

TEST_F(DeveloperToolsAvailabilityListPolicyHandlerTest,
       ApplyPolicySettings_AllowlistFiltersOutWildcardAsterisk) {
  base::ListValue in;
  in.Append("*");
  SetPolicy(key::kDeveloperToolsAvailabilityAllowlist,
            base::Value(std::move(in)));
  allowlist_handler_->ApplyPolicySettings(policies_, &prefs_);

  EXPECT_FALSE(
      prefs_.GetValue(prefs::kDeveloperToolsAvailabilityAllowlist, nullptr));

  // With a mix of wildcard and valid URLs:
  base::ListValue in_mixed;
  in_mixed.Append("*");
  in_mixed.Append("example.com");
  SetPolicy(key::kDeveloperToolsAvailabilityAllowlist,
            base::Value(std::move(in_mixed)));
  allowlist_handler_->ApplyPolicySettings(policies_, &prefs_);

  const base::Value* out = nullptr;
  EXPECT_TRUE(
      prefs_.GetValue(prefs::kDeveloperToolsAvailabilityAllowlist, &out));
  ASSERT_TRUE(out && out->is_list());
  ASSERT_EQ(1U, out->GetList().size());
  EXPECT_EQ("example.com", out->GetList()[0].GetString());
}

TEST_F(DeveloperToolsAvailabilityListPolicyHandlerTest,
       ApplyPolicySettings_Successful_Blocklist) {
  base::ListValue blocklist;
  blocklist.Append("*");
  blocklist.Append("example.com");
  SetPolicy(key::kDeveloperToolsAvailabilityBlocklist,
            base::Value(std::move(blocklist)));
  blocklist_handler_->ApplyPolicySettings(policies_, &prefs_);

  const base::Value* out = nullptr;
  EXPECT_TRUE(
      prefs_.GetValue(prefs::kDeveloperToolsAvailabilityBlocklist, &out));
  ASSERT_TRUE(out && out->is_list());
  EXPECT_EQ(2U, out->GetList().size());
  EXPECT_EQ("*", out->GetList()[0].GetString());
  EXPECT_EQ("example.com", out->GetList()[1].GetString());
}

}  // namespace policy
