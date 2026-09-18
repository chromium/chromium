// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/android/entity_instance_android.h"

#include <string>
#include <vector>

#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/types/optional_ref.h"
#include "chrome/browser/autofill/android/attribute_instance_android.h"
#include "chrome/browser/autofill/android/attribute_type_android.h"
#include "chrome/browser/autofill/android/entity_type_android.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_structured_address_component.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type_names.h"
#include "components/autofill/core/browser/test_utils/entity_data_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

namespace {

// ID of the dummy profile used for filling in tests.
constexpr char kGuid[] = "00000000-0000-0000-0000-000000000001";
constexpr char kNickname[] = "Nickname";
constexpr char16_t kFullName[] = u"John Doe";

AttributeInstanceAndroid CreateAttributeInstanceAndroid(
    AttributeTypeName type_name,
    std::u16string value,
    VerificationStatus status = VerificationStatus::kNoStatus) {
  return AttributeInstanceAndroid(
      AttributeTypeAndroid(AttributeType(type_name)), std::move(value), status);
}

EntityInstanceAndroid CreatePassportEntityInstanceAndroid(
    std::vector<AttributeInstanceAndroid> attributes =
        {CreateAttributeInstanceAndroid(AttributeTypeName::kPassportName,
                                        kFullName)},
    EntityInstance::RecordType record_type =
        EntityInstance::RecordType::kLocal) {
  EntityTypeAndroid entity_type_android(
      EntityType(EntityTypeName::kPassport),
      /*is_enabled=*/true,
      /*is_eligible_for_wallet_storage=*/false,
      /*is_masked_storage_supported=*/true);
  return EntityInstanceAndroid(
      std::move(entity_type_android), record_type, std::move(attributes),
      kNickname,
      EntityMetadataAndroid(kGuid, base::Time::Now(), 0, base::Time::Now()),
      /*requires_reauth_to_see=*/false, /*is_masked_server_entity=*/false);
}

class EntityInstanceAndroidTest : public testing::Test {
 protected:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(EntityInstanceAndroidTest, ToEntityInstance_BasicConversion) {
  EntityInstanceAndroid entity_instance_android =
      CreatePassportEntityInstanceAndroid();

  EntityInstance entity_instance = entity_instance_android.ToEntityInstance(
      /*existing_entity=*/std::nullopt);

  EXPECT_EQ(entity_instance.type(), EntityType(EntityTypeName::kPassport));
  EXPECT_EQ(entity_instance.guid().value(), kGuid);
  EXPECT_EQ(entity_instance.record_type(), EntityInstance::RecordType::kLocal);
  ASSERT_EQ(entity_instance.attributes().size(), 1u);
  EXPECT_EQ(entity_instance.attributes()[0].type(),
            AttributeType(AttributeTypeName::kPassportName));
  EXPECT_EQ(entity_instance.attributes()[0].GetCompleteRawInfo(), kFullName);
  EXPECT_FALSE(entity_instance.IsMaskedEntity());
  EXPECT_FALSE(entity_instance.IsServerInstance());
}

// Test that if an existing entity attribute did not change when converting an
// Instance from Java to C++, simply copy its value from the existing entity to
// the new entity.
TEST_F(EntityInstanceAndroidTest, ToEntityInstance_ReuseExistingAttribute) {
  // Create an existing entity with the same attribute value.
  EntityInstance existing_entity = test::GetEntityInstance(
      {test::GetAttributeInstance(AttributeTypeName::kPassportName, kFullName,
                                  VerificationStatus::kObserved)},
      {.guid = kGuid});

  // Create an Android entity with the same attribute value.
  EntityInstanceAndroid entity_instance_android =
      CreatePassportEntityInstanceAndroid();

  EntityInstance converted_entity =
      entity_instance_android.ToEntityInstance(existing_entity);

  ASSERT_EQ(converted_entity.attributes().size(), 1u);
  // The attribute should be reused, including its verification status.
  EXPECT_EQ(converted_entity.attributes()[0].GetVerificationStatus(
                AttributeType(AttributeTypeName::kPassportName).field_type()),
            VerificationStatus::kObserved);
}

// Test that if an existing entity attribute did change when converting an
// Instance from Java to C++, this updated attribute is used when creating the
// new entity. Otherwise the attribute is copied from the original entity.
TEST_F(EntityInstanceAndroidTest, ToEntityInstance_UpdateExistingAttribute) {
  std::u16string number = u"123456";
  EntityInstance existing_entity = test::GetEntityInstance(
      {test::GetAttributeInstance(AttributeTypeName::kPassportName, kFullName,
                                  VerificationStatus::kObserved),
       test::GetAttributeInstance(AttributeTypeName::kPassportNumber, number,
                                  VerificationStatus::kNoStatus)},
      {.guid = kGuid});

  // Create an Android entity with the attributes (note that only the first one
  // was really modified).
  std::u16string new_passport_name = u"Jane Doe";
  EntityInstanceAndroid entity_instance_android =
      CreatePassportEntityInstanceAndroid(
          {CreateAttributeInstanceAndroid(AttributeTypeName::kPassportName,
                                          new_passport_name,
                                          VerificationStatus::kUserVerified),
           CreateAttributeInstanceAndroid(AttributeTypeName::kPassportNumber,
                                          number,
                                          VerificationStatus::kUserVerified)});

  EntityInstance converted_entity =
      entity_instance_android.ToEntityInstance(existing_entity);

  ASSERT_EQ(converted_entity.attributes().size(), 2u);

  // The passport name attribute was updated, therefore it should be recreated
  // from scratch, including its verification status.
  const base::optional_ref<const AttributeInstance>
      updated_entity_passport_name = converted_entity.attribute(
          AttributeType(AttributeTypeName::kPassportName));
  EXPECT_EQ(updated_entity_passport_name->GetCompleteRawInfo(),
            new_passport_name);
  EXPECT_EQ(updated_entity_passport_name->GetVerificationStatus(
                AttributeType(AttributeTypeName::kPassportName).field_type()),
            VerificationStatus::kUserVerified);

  // The number attribute was not updated, therefore it should be copied from
  // the existing entity, including its verification status.
  const base::optional_ref<const AttributeInstance>
      updated_entity_passport_number = converted_entity.attribute(
          AttributeType(AttributeTypeName::kPassportNumber));
  EXPECT_EQ(updated_entity_passport_number->GetCompleteRawInfo(), number);
  // The following condition checks that the number attribute was copied from
  // the original/existing entity.
  EXPECT_EQ(updated_entity_passport_number->GetVerificationStatus(
                AttributeType(AttributeTypeName::kPassportNumber).field_type()),
            VerificationStatus::kNoStatus);
}

// Makes sure the `EntityInstance` C++ -> Java -> C++ conversion results in the
// object equal to the initial entity. This process has known caveats:
// * The name attributes have a parsed substructure in C++, which is not stored
//   in Java. An existing entity instance is provided to the conversion method
//   to keep the substructure.
// * The dates (modified date and use date) are stored with microsecond
//   precision in C++ and millisecond precision in Java. This means the test
//   will work as long as the dates have zero microseconds.
TEST_F(EntityInstanceAndroidTest, DoubleConversion) {
  EntityInstance passport = test::GetPassportEntityInstance(
      {.record_type = EntityInstance::WalletRecordTypePayload{
           .management_url =
               "https://wallet.google.com/synthetic_pass?id=fake123"}});
  EntityInstanceAndroid entity_instance_android(
      passport, /*is_enabled=*/true, /*is_eligible_for_wallet_storage=*/true,
      /*requires_reauth_to_see=*/true);

  JNIEnv* env = jni_zero::AttachCurrentThread();
  jni_zero::ScopedJavaLocalRef<jobject> java_instance =
      jni_zero::ToJniType<EntityInstanceAndroid>(env, entity_instance_android);

  EntityInstanceAndroid converted_entity =
      jni_zero::FromJniType<EntityInstanceAndroid>(env, java_instance);

  EntityInstance converted_passport =
      converted_entity.ToEntityInstance(passport);

  EXPECT_EQ(passport, converted_passport);
}

// Test that the Wallet record type payload is initialized with default data
// when converting an EntityInstanceAndroid back to an EntityInstance
// without providing an existing entity instance (i.e. when creating a new
// entity instance).
TEST_F(EntityInstanceAndroidTest,
       ToEntityInstance_NewWalletEntityInitializesPayload) {
  EntityInstanceAndroid entity_instance_android =
      CreatePassportEntityInstanceAndroid(
          {CreateAttributeInstanceAndroid(AttributeTypeName::kPassportName,
                                          kFullName)},
          EntityInstance::RecordType::kServerWallet);

  EntityInstance entity_instance = entity_instance_android.ToEntityInstance(
      /*existing_entity=*/std::nullopt);

  EXPECT_EQ(entity_instance.record_type(),
            EntityInstance::RecordType::kServerWallet);
  EXPECT_EQ(entity_instance.record_type_data(),
            EntityInstance::RecordTypeData(
                EntityInstance::WalletRecordTypePayload{.management_url = ""}));
}

// Test that the Wallet record type payload is added when converting an
// EntityInstanceAndroid back to an EntityInstance with ToEntityInstance().
TEST_F(EntityInstanceAndroidTest,
       ToEntityInstance_ExistingWalletEntityCopiesPayload) {
  EntityInstance existing_entity = test::GetPassportEntityInstance(
      {.record_type = EntityInstance::WalletRecordTypePayload{
           .management_url =
               "https://wallet.google.com/synthetic_pass?id=fake123"}});

  EntityInstance entity_without_payload = test::GetPassportEntityInstance();
  EntityInstanceAndroid entity_instance_android(
      entity_without_payload, /*is_enabled=*/true,
      /*is_eligible_for_wallet_storage=*/true,
      /*requires_reauth_to_see=*/false);

  EntityInstance entity_instance =
      entity_instance_android.ToEntityInstance(existing_entity);

  EXPECT_EQ(entity_instance.record_type(),
            EntityInstance::RecordType::kServerWallet);
  // The record type data has been copied from existing_entity.
  EXPECT_EQ(entity_instance.record_type_data(),
            existing_entity.record_type_data());
}

}  // namespace

}  // namespace autofill
