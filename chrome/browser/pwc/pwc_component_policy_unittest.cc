// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/pwc/pwc_component_policy.h"

#include <memory>
#include <string_view>
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

// A delegate that allows everything *and* opts HTTP origins in to navigation
// for local development. Models a component in its dev mode.
class InsecureDevDelegate : public AllowEverythingDelegate {
 public:
  bool AllowsInsecureDevOrigin(const url::Origin& origin) const override {
    return true;
  }
};

// Fixture for the delegate-driven insecure-origin exemption. Each test asserts
// one property via IsAllowed()/IsDenied(), which require all three policy
// predicates (AllowsInsecureDevOrigin, IsNavigationAllowed,
// IsCapabilityOrigin) to agree so a regression in any layer is caught.
class PwcInsecureDevOriginTest : public testing::Test {
 protected:
  static url::Origin Origin(std::string_view url) {
    return url::Origin::Create(GURL(url));
  }
  static url::Origin HttpOrigin() { return Origin("http://dev.example.com"); }
  static url::Origin HttpsOrigin() { return Origin("https://dev.example.com"); }

  template <typename Delegate>
  static PwcComponentPolicy MakePolicy(PrivilegedComponent component) {
    return PwcComponentPolicy(component, std::make_unique<Delegate>());
  }

  // Success iff all three policy predicates return `expected` for `origin`.
  // Returning an AssertionResult keeps the failure attributed to the calling
  // EXPECT_TRUE line while still naming the disagreeing predicate.
  static testing::AssertionResult OriginDecisionIs(
      const PwcComponentPolicy& policy,
      const url::Origin& origin,
      bool expected) {
    const struct {
      const char* name;
      bool actual;
    } checks[] = {
        {"AllowsInsecureDevOrigin", policy.AllowsInsecureDevOrigin(origin)},
        {"IsNavigationAllowed", policy.IsNavigationAllowed(origin)},
        {"IsCapabilityOrigin", policy.IsCapabilityOrigin(origin)},
    };
    for (const auto& check : checks) {
      if (check.actual != expected) {
        return testing::AssertionFailure()
               << check.name << "(" << origin << ") was " << check.actual
               << ", expected " << expected;
      }
    }
    return testing::AssertionSuccess();
  }
  static testing::AssertionResult IsAllowed(const PwcComponentPolicy& policy,
                                            const url::Origin& origin) {
    return OriginDecisionIs(policy, origin, /*expected=*/true);
  }
  static testing::AssertionResult IsDenied(const PwcComponentPolicy& policy,
                                           const url::Origin& origin) {
    return OriginDecisionIs(policy, origin, /*expected=*/false);
  }
};

// The default delegate never opts in, so HTTP stays denied for every
// component even when the delegate would otherwise allow everything.
TEST_F(PwcInsecureDevOriginTest, DeniedByDefault) {
  for (PrivilegedComponent component :
       {PrivilegedComponent::kGlic, PrivilegedComponent::kGeic,
        PrivilegedComponent::kTestComponent}) {
    SCOPED_TRACE(static_cast<int>(component));
    EXPECT_TRUE(
        IsDenied(MakePolicy<AllowEverythingDelegate>(component), HttpOrigin()));
  }
}

// A delegate that opts in lifts the HTTPS guardrail for HTTP origins, for
// whichever component it is attached to.
TEST_F(PwcInsecureDevOriginTest, AllowedWhenDelegateOptsIn) {
  for (PrivilegedComponent component :
       {PrivilegedComponent::kGlic, PrivilegedComponent::kGeic,
        PrivilegedComponent::kTestComponent}) {
    SCOPED_TRACE(static_cast<int>(component));
    PwcComponentPolicy policy = MakePolicy<InsecureDevDelegate>(component);
    EXPECT_TRUE(IsAllowed(policy, HttpOrigin()));
    EXPECT_TRUE(IsAllowed(policy, Origin("http://localhost:8080")));
  }
}

// Only plain HTTP is eligible; the opt-in cannot bless other insecure schemes
// or opaque origins, and HTTPS is unaffected by it.
TEST_F(PwcInsecureDevOriginTest, OptInIsLimitedToHttp) {
  PwcComponentPolicy policy =
      MakePolicy<InsecureDevDelegate>(PrivilegedComponent::kTestComponent);
  EXPECT_TRUE(IsDenied(policy, url::Origin()));  // opaque
  EXPECT_TRUE(IsDenied(policy, Origin("file:///tmp/index.html")));
  EXPECT_TRUE(IsDenied(policy, Origin("ftp://dev.example.com")));

  // HTTPS never consults the opt-in, so AllowsInsecureDevOrigin is false while
  // navigation and capability are still granted by the delegate.
  EXPECT_FALSE(policy.AllowsInsecureDevOrigin(HttpsOrigin()));
  EXPECT_TRUE(policy.IsNavigationAllowed(HttpsOrigin()));
  EXPECT_TRUE(policy.IsCapabilityOrigin(HttpsOrigin()));
}

// The opt-in only lifts the HTTPS guardrail; the delegate's navigation and
// capability answers still apply on top of it.
TEST_F(PwcInsecureDevOriginTest, DelegateStillNarrows) {
  class NavigationOnlyDevDelegate : public FixedPwcPolicyDelegate {
   public:
    NavigationOnlyDevDelegate()
        : FixedPwcPolicyDelegate(std::vector<url::Origin>{HttpOrigin()},
                                 std::vector<url::Origin>{}) {}
    bool AllowsInsecureDevOrigin(const url::Origin& origin) const override {
      return true;
    }
  };
  PwcComponentPolicy policy = MakePolicy<NavigationOnlyDevDelegate>(
      PrivilegedComponent::kTestComponent);
  EXPECT_TRUE(policy.AllowsInsecureDevOrigin(HttpOrigin()));
  EXPECT_TRUE(policy.IsNavigationAllowed(HttpOrigin()));
  EXPECT_FALSE(policy.IsCapabilityOrigin(HttpOrigin()));

  // An HTTP origin outside the delegate's navigation list is opted in to the
  // guardrail exemption but still denied navigation and capability.
  const url::Origin other = Origin("http://other.example.com");
  EXPECT_TRUE(policy.AllowsInsecureDevOrigin(other));
  EXPECT_FALSE(policy.IsNavigationAllowed(other));
  EXPECT_FALSE(policy.IsCapabilityOrigin(other));
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

TEST(PwcComponentPolicyTest, ContentEnforcementBitsAreFixedPerComponent) {
  PwcComponentPolicy test_policy(PrivilegedComponent::kTestComponent,
                                 MakeTestDelegate());
  EXPECT_TRUE(test_policy.disallow_service_worker_control());
  EXPECT_FALSE(test_policy.disallow_shared_workers());

  PwcComponentPolicy glic_policy(PrivilegedComponent::kGlic,
                                 MakeTestDelegate());
  EXPECT_TRUE(glic_policy.disallow_service_worker_control());
  EXPECT_TRUE(glic_policy.disallow_shared_workers());
}

TEST(PwcComponentPolicyTest, ContentFeatureIdIsDerivedFromComponent) {
  PwcComponentPolicy test_policy(PrivilegedComponent::kTestComponent,
                                 MakeTestDelegate());
  PwcComponentPolicy glic_policy(PrivilegedComponent::kGlic,
                                 MakeTestDelegate());
  // The id is the enum value itself, so it is distinct per component by
  // construction and cannot drift from the component it names.
  EXPECT_EQ(test_policy.content_feature_id(),
            static_cast<int32_t>(PrivilegedComponent::kTestComponent));
  EXPECT_EQ(glic_policy.content_feature_id(),
            static_cast<int32_t>(PrivilegedComponent::kGlic));
  EXPECT_NE(test_policy.content_feature_id(), glic_policy.content_feature_id());
}

}  // namespace
}  // namespace pwc
