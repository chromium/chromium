// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_actuator/local_actuation/local_actuation_peer_verifier.h"

#include <mach/mach.h>

#include <optional>
#include <string_view>

#include "base/apple/mach_logging.h"
#include "base/containers/span.h"
#include "build/branding_buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace browser_actuator {
namespace {

std::optional<audit_token_t> GetOwnAuditToken() {
  audit_token_t token;
  mach_msg_type_number_t count = TASK_AUDIT_TOKEN_COUNT;
  kern_return_t kr = task_info(mach_task_self(), TASK_AUDIT_TOKEN,
                               reinterpret_cast<task_info_t>(&token), &count);
  if (kr != KERN_SUCCESS) {
    MACH_LOG(ERROR, kr) << "task_info";
    return std::nullopt;
  }
  return token;
}

TEST(LocalActuationPeerVerifierTest, RejectsEmptyInfoPlist) {
  std::optional<audit_token_t> token = GetOwnAuditToken();
  ASSERT_TRUE(token);
  EXPECT_FALSE(LocalActuationPeerVerifier().Verify(*token, {}));
}

TEST(LocalActuationPeerVerifierTest, VerifiesThisProcess) {
  std::optional<audit_token_t> token = GetOwnAuditToken();
  ASSERT_TRUE(token);
  constexpr std::string_view kPlist = "<plist>test</plist>";
  const bool verified =
      LocalActuationPeerVerifier().Verify(*token, base::as_byte_span(kPlist));
  // A branded verifier accepts every sender when its own process is unsigned
  // or ad-hoc signed, as test binaries are. An unbranded verifier rejects
  // every sender.
  EXPECT_EQ(verified, BUILDFLAG(GOOGLE_CHROME_BRANDING) != 0);
}

}  // namespace
}  // namespace browser_actuator
