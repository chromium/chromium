// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/glic_pwc_policy_delegate.h"

#include <memory>
#include <string>

#include "base/command_line.h"
#include "base/test/scoped_command_line.h"
#include "chrome/browser/pwc/pwc_component_policy.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace glic {
namespace {

class GlicPwcPolicyDelegateTest : public testing::Test {
 protected:
  pwc::PwcComponentPolicy MakePolicy() {
    return pwc::PwcComponentPolicy(
        pwc::PrivilegedComponent::kGlic,
        std::make_unique<GlicPwcPolicyDelegate>(&profile_));
  }

  void EnableGlicDev() {
    command_line_.GetProcessCommandLine()->AppendSwitch(::switches::kGlicDev);
  }

  void EnableGeicAllowAnyGuestUrl() {
    command_line_.GetProcessCommandLine()->AppendSwitch(
        ::switches::kGeicAllowAnyGuestURL);
  }

  void SetGuestUrl(const std::string& url) {
    command_line_.GetProcessCommandLine()->AppendSwitchASCII(
        ::switches::kGlicGuestURL, url);
  }

  void SetAllowedOrigins(const std::string& origins) {
    command_line_.GetProcessCommandLine()->AppendSwitchASCII(
        ::switches::kGlicAllowedOrigins, origins);
  }

  static url::Origin HttpDevOrigin() {
    return url::Origin::Create(GURL("http://localhost:8080"));
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedCommandLine command_line_;
  TestingProfile profile_;
};

TEST_F(GlicPwcPolicyDelegateTest, HttpDeniedWithoutGlicDev) {
  pwc::PwcComponentPolicy policy = MakePolicy();
  EXPECT_FALSE(policy.AllowsInsecureDevOrigin(HttpDevOrigin()));
  EXPECT_FALSE(policy.IsNavigationAllowed(HttpDevOrigin()));
}

TEST_F(GlicPwcPolicyDelegateTest, HttpAllowedWithGlicDev) {
  EnableGlicDev();
  pwc::PwcComponentPolicy policy = MakePolicy();
  EXPECT_TRUE(policy.AllowsInsecureDevOrigin(HttpDevOrigin()));
  EXPECT_TRUE(policy.IsNavigationAllowed(HttpDevOrigin()));
}

// --geic-allow-any-guest-url lifts the HTTPS guardrail for the guest origin.
TEST_F(GlicPwcPolicyDelegateTest,
       HttpGuestOriginAllowedWithGeicAllowAnyGuestUrl) {
  EnableGeicAllowAnyGuestUrl();
  SetGuestUrl("http://localhost:8080/side-panel");
  pwc::PwcComponentPolicy policy = MakePolicy();
  EXPECT_TRUE(policy.AllowsInsecureDevOrigin(HttpDevOrigin()));
  EXPECT_TRUE(policy.IsNavigationAllowed(HttpDevOrigin()));
}

// --geic-allow-any-guest-url doesn't lift the HTTPS guardrail for other
// origins, even if they are on the allowed origins list.
TEST_F(GlicPwcPolicyDelegateTest,
       GeicAllowAnyGuestUrlDoesNotAllowOtherHttpOrigins) {
  EnableGeicAllowAnyGuestUrl();
  SetGuestUrl("http://localhost:8080/side-panel");
  SetAllowedOrigins("http://other.test:8080 https://other.test");
  pwc::PwcComponentPolicy policy = MakePolicy();

  const url::Origin http_other =
      url::Origin::Create(GURL("http://other.test:8080"));
  EXPECT_FALSE(policy.AllowsInsecureDevOrigin(http_other));
  EXPECT_FALSE(policy.IsNavigationAllowed(http_other));

  // The HTTPS origin on the same list is still allowed.
  EXPECT_TRUE(policy.IsNavigationAllowed(
      url::Origin::Create(GURL("https://other.test"))));
}

// --glic-dev only lifts the HTTPS guardrail for plain HTTP; the policy still
// rejects other insecure schemes before consulting the delegate.
TEST_F(GlicPwcPolicyDelegateTest, GlicDevDoesNotBlessOtherSchemes) {
  EnableGlicDev();
  pwc::PwcComponentPolicy policy = MakePolicy();
  EXPECT_FALSE(
      policy.IsNavigationAllowed(url::Origin::Create(GURL("file:///tmp/a"))));
  EXPECT_FALSE(policy.IsNavigationAllowed(url::Origin()));
}

}  // namespace
}  // namespace glic
