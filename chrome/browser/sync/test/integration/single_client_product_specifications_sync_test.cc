// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sstream>

#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "chrome/browser/commerce/product_specifications/product_specifications_service_factory.h"
#include "chrome/browser/sync/test/integration/product_specifications_helper.h"
#include "chrome/browser/sync/test/integration/sync_integration_test_util.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/commerce/core/product_specifications/product_specifications_service.h"
#include "components/commerce/core/product_specifications/product_specifications_set.h"
#include "components/sync/base/model_type.h"
#include "components/sync/base/time.h"
#include "components/sync/protocol/entity_specifics.pb.h"
#include "components/sync/protocol/product_comparison_specifics.pb.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/protobuf/src/google/protobuf/io/zero_copy_stream_impl_lite.h"

namespace {

const char kNonUniqueName[] = "non_unique_name";
const char kUuid[] = "e22e29ba-135a-46ea-969a-ece45f979784";
const char kName[] = "name";
const char kNewName[] = "my_new_name";
const std::vector<std::string> kUrls = {"https://product_one.com/",
                                        "https://product_two.com/"};
const int64_t kCreationTimeEpochMicros = 1712162260;
const int64_t kUpdateTimeEpochMicros = 1713162260;
using testing::UnorderedElementsAre;

void FillInSpecifics(
    sync_pb::ProductComparisonSpecifics* product_comparison_specifics,
    const std::string& uuid,
    const std::string& name,
    const std::vector<std::string>& urls) {
  product_comparison_specifics->set_uuid(uuid);
  product_comparison_specifics->set_name(name);
  product_comparison_specifics->set_creation_time_unix_epoch_micros(
      base::Time::Now().ToDeltaSinceWindowsEpoch().InMicroseconds());
  product_comparison_specifics->set_update_time_unix_epoch_micros(
      base::Time::Now().ToDeltaSinceWindowsEpoch().InMicroseconds());
  for (const std::string& url : urls) {
    sync_pb::ComparisonData* data = product_comparison_specifics->add_data();
    data->set_url(url);
  }
}

std::string CreateSerializedProtoField(int field_number,
                                       const std::string& value) {
  std::string result;
  google::protobuf::io::StringOutputStream string_stream(&result);
  google::protobuf::io::CodedOutputStream output(&string_stream);
  google::protobuf::internal::WireFormatLite::WriteTag(
      field_number,
      google::protobuf::internal::WireFormatLite::WIRETYPE_LENGTH_DELIMITED,
      &output);
  output.WriteVarint32(value.size());
  output.WriteString(value);
  return result;
}

MATCHER_P2(HasUnknownFields, uuid, unknown_fields, "") {
  return arg.specifics().product_comparison().uuid() == uuid &&
         arg.specifics().product_comparison().unknown_fields() ==
             unknown_fields;
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

  commerce::ProductSpecificationsService* GetService() {
    return commerce::ProductSpecificationsServiceFactory::GetForBrowserContext(
        GetProfile(0));
  }

  const sync_pb::ProductComparisonSpecifics ToProto(
      const commerce::ProductSpecificationsSet& set) const {
    return set.ToProto();
  }

 private:
  base::test::ScopedFeatureList features_override_{
      commerce::kProductSpecifications};
};

IN_PROC_BROWSER_TEST_F(SingleClientProductSpecificationsSyncTest,
                       DownloadWhenSyncEnabled) {
  sync_pb::EntitySpecifics entity_specifics;
  sync_pb::ProductComparisonSpecifics* product_comparison_specifics =
      entity_specifics.mutable_product_comparison();
  FillInSpecifics(product_comparison_specifics, kUuid, kName, kUrls);

  InjectEntityToServer(entity_specifics);
  ASSERT_TRUE(SetupSync());
  EXPECT_TRUE(commerce::ProductSpecificationsChecker(
                  GetService(), *product_comparison_specifics)
                  .Wait());
}

IN_PROC_BROWSER_TEST_F(SingleClientProductSpecificationsSyncTest,
                       PreservesUnsupportedFieldsDataOnCommits) {
  const std::string kUnsupportedField = CreateSerializedProtoField(
      /*field_number=*/99999, "unknown_field_value");
  sync_pb::EntitySpecifics entity_specifics;
  sync_pb::ProductComparisonSpecifics* product_comparison_specifics =
      entity_specifics.mutable_product_comparison();
  FillInSpecifics(product_comparison_specifics, kUuid, kName, kUrls);
  *product_comparison_specifics->mutable_unknown_fields() = kUnsupportedField;

  InjectEntityToServer(entity_specifics);
  ASSERT_TRUE(SetupSync());
  commerce::ProductSpecificationsService* service = GetService();
  ASSERT_TRUE(commerce::ProductSpecificationsChecker(
                  service, *product_comparison_specifics)
                  .Wait());

  std::optional<commerce::ProductSpecificationsSet> updated_set =
      service->SetName(base::Uuid::ParseLowercase(kUuid), kNewName);
  ASSERT_TRUE(updated_set.has_value());

  ASSERT_TRUE(commerce::ProductSpecificationsChecker(
                  service, ToProto(updated_set.value()))
                  .Wait());

  std::optional<commerce::ProductSpecificationsSet>
      new_product_specifications_set = service->AddProductSpecificationsSet(
          kNewName, {GURL("https://foo.com/"), GURL("https://bar.com/")});

  ASSERT_TRUE(
      ServerCountMatchStatusChecker(syncer::PRODUCT_COMPARISON, 2).Wait());

  // product_comparison_specifics should preserve the unknown filed
  // new_product_specifications_set should not have an unknown field as it
  // originated from a client unaware of the new field.
  EXPECT_THAT(
      fake_server_->GetSyncEntitiesByModelType(syncer::PRODUCT_COMPARISON),
      UnorderedElementsAre(
          HasUnknownFields(kUuid, kUnsupportedField),
          HasUnknownFields(
              new_product_specifications_set.value().uuid().AsLowercaseString(),
              "")));
}

}  // namespace
