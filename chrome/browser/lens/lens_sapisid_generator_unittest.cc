// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lens/lens_sapisid_generator.h"

#include <optional>
#include <string>

#include "base/time/time.h"
#include "build/branding_buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace lens {

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
TEST(LensSapisidGeneratorTest, GenerateSapisidHash_GoldenTest) {
  // Use fixed inputs to verify the hash algorithm.
  std::string email = "user@gmail.com";
  std::string sapisid = "sapisid_cookie_value";
  std::string origin = "https://www.google.com";
  // 2026-06-12 12:00:00 UTC
  base::Time timestamp;
  ASSERT_TRUE(base::Time::FromUTCString("2026-06-12 12:00:00 UTC", &timestamp));

  // Expected values for testing:
  // timestamp_millis = 1781265600000
  // hash_source = "user@gmail.com 1781265600000 sapisid_cookie_value
  // https://www.google.com" SHA1(hash_source) =
  // 9bd27681bae726e0f13c8da3f7ec536243912710 Expected output: "SAPISIDHASH
  // 1781265600000_9bd27681bae726e0f13c8da3f7ec536243912710_e"

  std::optional<std::string> hash =
      GenerateSapisidHash(email, sapisid, origin, timestamp);
  ASSERT_TRUE(hash.has_value());

  EXPECT_EQ(
      hash.value(),
      "SAPISIDHASH 1781265600000_9bd27681bae726e0f13c8da3f7ec536243912710_e");
}
#endif

}  // namespace lens
