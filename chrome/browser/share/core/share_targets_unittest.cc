// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/share/core/share_targets.h"

#include <string.h>

#include <memory>

#include "base/files/file_path.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/country_codes/country_codes.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::NiceMock;

namespace sharing {

class MockShareTargets : public ShareTargets {
 public:
  MockShareTargets() = default;
  ~MockShareTargets() override = default;

  MOCK_METHOD2(RecordUpdateMetrics, void(UpdateResult, UpdateOrigin));
};

class ShareTargetsTest : public testing::Test {
 protected:
  ShareTargetsTest() = default;
  ~ShareTargetsTest() override = default;

 protected:
  NiceMock<MockShareTargets> targets_;
};

TEST_F(ShareTargetsTest, UnpackResourceBundle) {
  EXPECT_CALL(targets_,
              RecordUpdateMetrics(ShareTargets::UpdateResult::SUCCESS,
                                  ShareTargets::UpdateOrigin::RESOURCE_BUNDLE));
  targets_.PopulateFromResourceBundle();
}

TEST_F(ShareTargetsTest, BadProto) {
  EXPECT_EQ(ShareTargets::UpdateResult::FAILED_EMPTY,
            targets_.PopulateFromBinaryPb(std::string()));

  EXPECT_EQ(ShareTargets::UpdateResult::FAILED_PROTO_PARSE,
            targets_.PopulateFromBinaryPb("foobar"));
}

TEST_F(ShareTargetsTest, BadUpdateFromExisting) {
  // Make a minimum viable config.
  mojom::MapLocaleTargets mlt;
  mlt.set_version_id(2);
  EXPECT_EQ(ShareTargets::UpdateResult::SUCCESS,
            targets_.PopulateFromBinaryPb(mlt.SerializeAsString()));

  // Can't update to the same version.
  EXPECT_EQ(ShareTargets::UpdateResult::SKIPPED_VERSION_CHECK_EQUAL,
            targets_.PopulateFromBinaryPb(mlt.SerializeAsString()));

  // Can't update to an older version.
  mlt.set_version_id(1);
  EXPECT_EQ(ShareTargets::UpdateResult::FAILED_VERSION_CHECK,
            targets_.PopulateFromBinaryPb(mlt.SerializeAsString()));
}

TEST_F(ShareTargetsTest, CountryCodeMatches) {
  EXPECT_EQ("US", targets_.GetCountryStringFromID(
                      country_codes::CountryStringToCountryID("US")));
  EXPECT_EQ("CA", targets_.GetCountryStringFromID(
                      country_codes::CountryStringToCountryID("CA")));
  EXPECT_EQ("RU", targets_.GetCountryStringFromID(
                      country_codes::CountryStringToCountryID("RU")));
  EXPECT_EQ("JP", targets_.GetCountryStringFromID(
                      country_codes::CountryStringToCountryID("JP")));
  EXPECT_NE("JP", targets_.GetCountryStringFromID(
                      country_codes::CountryStringToCountryID("US")));
}
}  // namespace sharing
