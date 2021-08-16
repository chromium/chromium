// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/borealis/borealis_util.h"

#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "chrome/browser/ash/guest_os/guest_os_registry_service.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "net/base/url_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace borealis {
namespace {

class BorealisUtilTest : public testing::Test {
 protected:
  GURL GetFeedbackFormUrl(
      const guest_os::GuestOsRegistryService* registry_service,
      const std::string& app_id,
      const std::string& window_title) {
    base::RunLoop run_loop;
    GURL returned_url;
    FeedbackFormUrl(registry_service, app_id, window_title,
                    base::BindLambdaForTesting([&](GURL url) {
                      returned_url = url;
                      run_loop.Quit();
                    }));
    run_loop.Run();
    return returned_url;
  }

  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(BorealisUtilTest, GetBorealisAppIdReturnsEmptyOnFailure) {
  EXPECT_EQ(GetBorealisAppId("foo"), absl::nullopt);
}

TEST_F(BorealisUtilTest, GetBorealisAppIdReturnsId) {
  EXPECT_EQ(GetBorealisAppId("borealis/123").value(), 123);
}

TEST_F(BorealisUtilTest, FeedbackFormUrlExcludesNonGames) {
  TestingProfile profile;
  guest_os::GuestOsRegistryService registry(&profile);

  EXPECT_FALSE(GetFeedbackFormUrl(&registry,
                                  "borealisanon:org.chromium.borealis.xid.100",
                                  "CoolApp")
                   .is_valid());
}

TEST_F(BorealisUtilTest, FeedbackFormUrlPrefillsWindowTitle) {
  TestingProfile profile;
  guest_os::GuestOsRegistryService registry(&profile);

  EXPECT_THAT(
      GetFeedbackFormUrl(&registry, "borealisanon:org.chromium.borealis.app",
                         "CoolApp")
          .spec(),
      testing::HasSubstr("=CoolApp"));
}

TEST_F(BorealisUtilTest, FeedbackFormUrlIsPrefilled) {
  TestingProfile profile;
  guest_os::GuestOsRegistryService registry(&profile);

  GURL url = GetFeedbackFormUrl(
      &registry, "borealisanon:org.chromium.borealis.app", "CoolApp");

  // Count the number of query parameters beginning with "entry"; these are
  // form fields that we're prefilling.
  int entries = 0;
  net::QueryIterator it(url);
  while (!it.IsAtEnd()) {
    if (base::StartsWith(it.GetKey(), "entry")) {
      ++entries;

      // All prefilled entries should have a value.
      EXPECT_THAT(it.GetValue(), testing::Not(testing::IsEmpty()));
    }
    it.Advance();
  }

  EXPECT_EQ(entries, 4);  // we currently prefill this many form fields
}

}  // namespace
}  // namespace borealis
