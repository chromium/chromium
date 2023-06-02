// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/protected_media_identifier_permission_context.h"

#include "base/memory/raw_ptr.h"
#include "base/test/scoped_command_line.h"
#include "chrome/common/chrome_switches.h"
#include "media/base/media_switches.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chromeos/dbus/constants/dbus_switches.h"  // nogncheck
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_switches.h"
#endif

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/profiles/profile_testing_helper.h"
#endif

class ProtectedMediaIdentifierPermissionContextTest : public testing::Test {
 public:
  ProtectedMediaIdentifierPermissionContextTest()
      : requesting_origin_("https://example.com"),
        requesting_sub_domain_origin_("https://subdomain.example.com") {
    command_line_ = scoped_command_line_.GetProcessCommandLine();
#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_WIN)
    profile_testing_helper_.SetUp();
#endif  // BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_WIN)
#if BUILDFLAG(IS_CHROMEOS_LACROS)
    attestation_enabled_ = true;
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
  }

  bool IsOriginAllowed(const GURL& origin) {
    return ProtectedMediaIdentifierPermissionContext::IsOriginAllowed(origin);
  }

  bool IsProtectedMediaIdentifierEnabled(Profile* profile) {
    return ProtectedMediaIdentifierPermissionContext::
        IsProtectedMediaIdentifierEnabled(profile);
  }

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  void OnAttestationEnabledChanged(base::Value value) {
    return ProtectedMediaIdentifierPermissionContext::
        OnAttestationEnabledChanged(value);
  }
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

  GURL requesting_origin_;
  GURL requesting_sub_domain_origin_;

  base::test::ScopedCommandLine scoped_command_line_;
  raw_ptr<base::CommandLine> command_line_;
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS)
  ProfileTestingHelper profile_testing_helper_;
#endif
};

TEST_F(ProtectedMediaIdentifierPermissionContextTest,
       BypassWithFlagWithSingleDomain) {
  // The request should need to ask for permission
  ASSERT_FALSE(IsOriginAllowed(requesting_origin_));

  // Add the switch value that the
  // ProtectedMediaIdentifierPermissionContext reads from
  command_line_->AppendSwitchASCII(
      switches::kUnsafelyAllowProtectedMediaIdentifierForDomain, "example.com");

  // The request should no longer need to ask for permission
  ASSERT_TRUE(IsOriginAllowed(requesting_origin_));
}

TEST_F(ProtectedMediaIdentifierPermissionContextTest,
       BypassWithFlagWithDomainList) {
  // The request should need to ask for permission
  ASSERT_FALSE(IsOriginAllowed(requesting_origin_));

  // Add the switch value that the
  // ProtectedMediaIdentifierPermissionContext reads from
  command_line_->AppendSwitchASCII(
      switches::kUnsafelyAllowProtectedMediaIdentifierForDomain,
      "example.ca,example.com,example.edu");

  // The request should no longer need to ask for permission
  ASSERT_TRUE(IsOriginAllowed(requesting_origin_));
}

TEST_F(ProtectedMediaIdentifierPermissionContextTest,
       BypassWithFlagAndSubdomain) {
  // The request should need to ask for permission
  ASSERT_FALSE(IsOriginAllowed(requesting_sub_domain_origin_));

  // Add the switch value that the
  // ProtectedMediaIdentifierPermissionContext reads from
  command_line_->AppendSwitchASCII(
      switches::kUnsafelyAllowProtectedMediaIdentifierForDomain, "example.com");

  // The request should no longer need to ask for permission
  ASSERT_TRUE(IsOriginAllowed(requesting_sub_domain_origin_));
}

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS)
TEST_F(ProtectedMediaIdentifierPermissionContextTest,
       ProtectedMediaIdentifierOnDifferentProfiles) {
  ASSERT_FALSE(IsProtectedMediaIdentifierEnabled(
      profile_testing_helper_.incognito_profile()));

  ASSERT_FALSE(IsProtectedMediaIdentifierEnabled(
      profile_testing_helper_.guest_profile()));

  ASSERT_TRUE(IsProtectedMediaIdentifierEnabled(
      profile_testing_helper_.regular_profile()));
}
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_CHROMEOS)
TEST_F(ProtectedMediaIdentifierPermissionContextTest,
       ProtectedMediaIdentifierDisabledOnDevMode) {
  command_line_->AppendSwitch(chromeos::switches::kSystemDevMode);

  // The protected media identifier should not be enabled if the system is on
  // dev mode.
  ASSERT_FALSE(IsProtectedMediaIdentifierEnabled(
      profile_testing_helper_.regular_profile()));
}
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(ProtectedMediaIdentifierPermissionContextTest,
       ProtectedMediaIdentifierEnabledOnDevModeWithAshSwitch) {
  command_line_->AppendSwitch(chromeos::switches::kSystemDevMode);
  command_line_->AppendSwitch(switches::kAllowRAInDevMode);

  // As long as `kAllowRAInDevMode` is appended, then even if system is on dev
  // mode, the protected media identifier should be enabled.
  ASSERT_TRUE(IsProtectedMediaIdentifierEnabled(
      profile_testing_helper_.regular_profile()));
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
TEST_F(ProtectedMediaIdentifierPermissionContextTest,
       ProtectedMediaIdentifierEnterprisePolicyChanges) {
  // As long as `kAllowRAInDevMode` is appended, then even if system is on dev
  // mode, the protected media identifier should be enabled.
  ASSERT_TRUE(IsProtectedMediaIdentifierEnabled(
      profile_testing_helper_.regular_profile()));

  OnAttestationEnabledChanged(base::Value(false));

  ASSERT_FALSE(IsProtectedMediaIdentifierEnabled(
      profile_testing_helper_.regular_profile()));
}
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
