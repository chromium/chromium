// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sstream>

#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "chrome/browser/commerce/product_specifications/product_specifications_service_factory.h"
#include "chrome/browser/sync/test/integration/product_specifications_helper.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/sync/base/time.h"
#include "components/sync/protocol/compare_specifics.pb.h"
#include "components/sync/protocol/entity_specifics.pb.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace {

const char kNonUniqueName[] = "non_unique_name";
const char kUuid[] = "e22e29ba-135a-46ea-969a-ece45f979784";
const char kName[] = "name";
constexpr std::pair<std::string_view, std::string_view> kUrls = {
    "https://product_one.com/", "https://product_two.com/"};
const int64_t kCreationTimeEpochMicros = 1712162260;
const int64_t kUpdateTimeEpochMicros = 1713162260;

void FillInSpecifics(
    sync_pb::CompareSpecifics* compare_specifics,
    const std::string& uuid,
    const std::string& name,
    const std::pair<std::string_view, std::string_view>& urls) {
  compare_specifics->set_uuid(uuid);
  compare_specifics->set_name(name);
  compare_specifics->set_creation_time_unix_epoch_micros(
      base::Time::Now().ToDeltaSinceWindowsEpoch().InMicroseconds());
  compare_specifics->set_update_time_unix_epoch_micros(
      base::Time::Now().ToDeltaSinceWindowsEpoch().InMicroseconds());
  sync_pb::ComparisonData* data = compare_specifics->add_data();
  data->set_url(urls.first.data());
  data = compare_specifics->add_data();
  data->set_url(urls.second.data());
}

class SingleClientProductSpecificationsSyncTest : public SyncTest {
 public:
  SingleClientProductSpecificationsSyncTest() : SyncTest(SINGLE_CLIENT) {}

  SingleClientProductSpecificationsSyncTest(
      const SingleClientProductSpecificationsSyncTest&) = delete;
  SingleClientProductSpecificationsSyncTest& operator=(
      const SingleClientProductSpecificationsSyncTest&) = delete;

  ~SingleClientProductSpecificationsSyncTest() override = default;

  void InjectEntityToServer(const sync_pb::EntitySpecifics& specifics) {
    fake_server_->InjectEntity(
        syncer::PersistentUniqueClientEntity::CreateFromSpecificsForTesting(
            kNonUniqueName, kUuid, specifics,
            /*creation_time=*/kCreationTimeEpochMicros,
            /*last_modified_time=*/kUpdateTimeEpochMicros));
  }

 private:
  base::test::ScopedFeatureList features_override_{
      commerce::kProductSpecificationsSync};
};

IN_PROC_BROWSER_TEST_F(SingleClientProductSpecificationsSyncTest,
                       DownloadWhenSyncEnabled) {
  sync_pb::EntitySpecifics entity_specifics;
  sync_pb::CompareSpecifics* compare_specifics =
      entity_specifics.mutable_compare();
  FillInSpecifics(compare_specifics, kUuid, kName, kUrls);

  InjectEntityToServer(entity_specifics);
  ASSERT_TRUE(SetupSync());
  EXPECT_TRUE(
      commerce::ProductSpecificationsChecker(
          commerce::ProductSpecificationsServiceFactory::GetForBrowserContext(
              GetProfile(0)),
          compare_specifics)
          .Wait());
}

}  // namespace
