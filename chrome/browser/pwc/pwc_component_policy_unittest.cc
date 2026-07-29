// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/pwc/pwc_component_policy.h"

#include <memory>
#include <utility>
#include <vector>

#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace pwc {
namespace {

url::Origin TestOrigin() {
  return url::Origin::Create(GURL("https://pwc-test.example.com"));
}

url::Origin NavigationOnlyOrigin() {
  return url::Origin::Create(GURL("https://navigation-only.example.com"));
}

std::unique_ptr<FixedPwcPolicyDelegate> MakeTestDelegate() {
  return std::make_unique<FixedPwcPolicyDelegate>(
      std::vector<url::Origin>{TestOrigin(), NavigationOnlyOrigin()},
      std::vector<url::Origin>{TestOrigin()});
}

// A delegate that (incorrectly) allows everything. Used to prove the
// structural guardrails in PwcComponentPolicy hold regardless of delegate
// behavior.
class AllowEverythingDelegate : public PwcPolicyDelegate {
 public:
  bool IsNavigationAllowed(const url::Origin& origin) const override {
    return true;
  }
  bool IsCapabilityOrigin(const url::Origin& origin) const override {
    return true;
  }
};

// A delegate that grants capability without granting navigation.
class CapabilityWithoutNavigationDelegate : public PwcPolicyDelegate {
 public:
  bool IsNavigationAllowed(const url::Origin& origin) const override {
    return false;
  }
  bool IsCapabilityOrigin(const url::Origin& origin) const override {
    return true;
  }
};

TEST(PwcComponentPolicyTest, FixedDelegateAnswersFromItsLists) {
  PwcComponentPolicy policy(PrivilegedComponent::kTestComponent,
                            MakeTestDelegate());
  EXPECT_EQ(policy.component(), PrivilegedComponent::kTestComponent);

  EXPECT_TRUE(policy.IsNavigationAllowed(TestOrigin()));
  EXPECT_TRUE(policy.IsCapabilityOrigin(TestOrigin()));

  const url::Origin unlisted =
      url::Origin::Create(GURL("https://unlisted.example.com"));
  EXPECT_FALSE(policy.IsNavigationAllowed(unlisted));
  EXPECT_FALSE(policy.IsCapabilityOrigin(unlisted));
}

TEST(PwcComponentPolicyTest, NavigationOnlyOriginGetsNoCapability) {
  PwcComponentPolicy policy(PrivilegedComponent::kTestComponent,
                            MakeTestDelegate());
  EXPECT_TRUE(policy.IsNavigationAllowed(NavigationOnlyOrigin()));
  EXPECT_FALSE(policy.IsCapabilityOrigin(NavigationOnlyOrigin()));
}

// The HTTPS guardrail is enforced before the delegate is consulted: even a
// delegate that allows everything cannot bless insecure or opaque origins.
TEST(PwcComponentPolicyTest, NonHttpsDeniedRegardlessOfDelegate) {
  PwcComponentPolicy policy(PrivilegedComponent::kTestComponent,
                            std::make_unique<AllowEverythingDelegate>());

  EXPECT_FALSE(policy.IsNavigationAllowed(
      url::Origin::Create(GURL("http://pwc-test.example.com"))));
  EXPECT_FALSE(policy.IsCapabilityOrigin(
      url::Origin::Create(GURL("http://pwc-test.example.com"))));
  EXPECT_FALSE(policy.IsNavigationAllowed(url::Origin()));
  EXPECT_FALSE(policy.IsCapabilityOrigin(url::Origin()));

  // An HTTPS origin is passed through to the delegate.
  EXPECT_TRUE(policy.IsNavigationAllowed(TestOrigin()));
  EXPECT_TRUE(policy.IsCapabilityOrigin(TestOrigin()));
}

// The two-tier guardrail is structural: capability requires navigability, so
// a delegate granting capability alone grants nothing.
TEST(PwcComponentPolicyTest, CapabilityRequiresNavigation) {
  PwcComponentPolicy policy(
      PrivilegedComponent::kTestComponent,
      std::make_unique<CapabilityWithoutNavigationDelegate>());
  EXPECT_FALSE(policy.IsNavigationAllowed(TestOrigin()));
  EXPECT_FALSE(policy.IsCapabilityOrigin(TestOrigin()));
}

TEST(PwcComponentPolicyTest, NewWindowPolicyIsFixedPerComponent) {
  PwcComponentPolicy test_policy(PrivilegedComponent::kTestComponent,
                                 MakeTestDelegate());
  EXPECT_EQ(test_policy.new_window_policy(),
            PwcComponentPolicy::NewWindowPolicy::kDrop);

  PwcComponentPolicy glic_policy(PrivilegedComponent::kGlic,
                                 MakeTestDelegate());
  EXPECT_EQ(glic_policy.new_window_policy(),
            PwcComponentPolicy::NewWindowPolicy::kOpenAsUnrelatedTab);
}

}  // namespace
}  // namespace pwc
