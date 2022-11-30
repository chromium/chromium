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

class ProtectedMediaIdentifierPermissionContextTest : public testing::Test {
 public:
  ProtectedMediaIdentifierPermissionContextTest()
      : requesting_origin_("https://example.com"),
        requesting_sub_domain_origin_("https://subdomain.example.com") {
    command_line_ = scoped_command_line_.GetProcessCommandLine();
  }

  bool IsOriginAllowed(const GURL& origin) {
    return ProtectedMediaIdentifierPermissionContext::IsOriginAllowed(origin);
  }

  GURL requesting_origin_;
  GURL requesting_sub_domain_origin_;

  base::test::ScopedCommandLine scoped_command_line_;
  raw_ptr<base::CommandLine> command_line_;
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
