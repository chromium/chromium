// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/values.h"
#include "chrome/browser/extensions/api/chrome_extensions_api_client.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/api/messaging/messaging_delegate.h"

namespace policy {

namespace {
extensions::MessagingDelegate::PolicyPermission IsNativeMessagingHostAllowed(
    content::BrowserContext* browser_context,
    const std::string& native_host_name) {
  extensions::MessagingDelegate* messaging_delegate =
      extensions::ExtensionsAPIClient::Get()->GetMessagingDelegate();
  EXPECT_NE(messaging_delegate, nullptr);
  return messaging_delegate->IsNativeMessagingHostAllowed(browser_context,
                                                          native_host_name);
}
}  // namespace

IN_PROC_BROWSER_TEST_F(PolicyTest, NativeMessagingBlocklistSelective) {
  base::Value::List blocklist;
  blocklist.Append("host.name");
  PolicyMap policies;
  policies.Set(key::kNativeMessagingBlocklist, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               base::Value(std::move(blocklist)), nullptr);
  UpdateProviderPolicy(policies);

  EXPECT_EQ(extensions::MessagingDelegate::PolicyPermission::DISALLOW,
            IsNativeMessagingHostAllowed(browser()->profile(), "host.name"));
  EXPECT_EQ(
      extensions::MessagingDelegate::PolicyPermission::ALLOW_ALL,
      IsNativeMessagingHostAllowed(browser()->profile(), "other.host.name"));
}

IN_PROC_BROWSER_TEST_F(PolicyTest, NativeMessagingBlocklistWildcard) {
  base::Value::List blocklist;
  blocklist.Append("*");
  PolicyMap policies;
  policies.Set(key::kNativeMessagingBlocklist, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               base::Value(std::move(blocklist)), nullptr);
  UpdateProviderPolicy(policies);

  EXPECT_EQ(extensions::MessagingDelegate::PolicyPermission::DISALLOW,
            IsNativeMessagingHostAllowed(browser()->profile(), "host.name"));
  EXPECT_EQ(
      extensions::MessagingDelegate::PolicyPermission::DISALLOW,
      IsNativeMessagingHostAllowed(browser()->profile(), "other.host.name"));
}

IN_PROC_BROWSER_TEST_F(PolicyTest, NativeMessagingAllowlist) {
  base::Value::List blocklist;
  blocklist.Append("*");
  base::Value::List allowlist;
  allowlist.Append("host.name");
  PolicyMap policies;
  policies.Set(key::kNativeMessagingBlocklist, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               base::Value(std::move(blocklist)), nullptr);
  policies.Set(key::kNativeMessagingAllowlist, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               base::Value(std::move(allowlist)), nullptr);
  UpdateProviderPolicy(policies);

  EXPECT_EQ(extensions::MessagingDelegate::PolicyPermission::ALLOW_ALL,
            IsNativeMessagingHostAllowed(browser()->profile(), "host.name"));
  EXPECT_EQ(
      extensions::MessagingDelegate::PolicyPermission::DISALLOW,
      IsNativeMessagingHostAllowed(browser()->profile(), "other.host.name"));
}

}  // namespace policy
