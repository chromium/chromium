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
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type_names.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

namespace {

// ID of the dummy profile used for filling in tests.
constexpr char kGuid[] = "00000000-0000-0000-0000-000000000001";

class EntityInstanceAndroidTest : public testing::Test {
 protected:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(EntityInstanceAndroidTest, ToEntityInstance_BasicConversion) {
  EntityType entity_type(EntityTypeName::kPassport);
  EntityTypeAndroid entity_type_android(entity_type);

  AttributeType attribute_type(AttributeTypeName::kPassportName);
  AttributeTypeAndroid passport_name_attribute_type_android(attribute_type);

  std::u16string passport_name = u"John Doe";
  AttributeInstanceAndroid attribute_instance_android(
      passport_name_attribute_type_android, passport_name);
  EntityInstanceAndroid entity_instance_android(
      entity_type_android, kGuid, EntityInstance::RecordType::kLocal,
      {attribute_instance_android},
      EntityMetadataAndroid(base::Time::Now(), 0));

  EntityInstance entity_instance =
      entity_instance_android.ToEntityInstance(std::nullopt);

  EXPECT_EQ(entity_instance.type(), entity_type);
  EXPECT_EQ(entity_instance.guid().value(), kGuid);
  EXPECT_EQ(entity_instance.record_type(), EntityInstance::RecordType::kLocal);
  ASSERT_EQ(entity_instance.attributes().size(), 1u);
  EXPECT_EQ(entity_instance.attributes()[0].type(), attribute_type);
  EXPECT_EQ(entity_instance.attributes()[0].GetCompleteRawInfo(),
            passport_name);
}

// Test that if an existing entity attribute did not change when converting an
// Instance from Java to C++, simply copy its value from the existing entity to
// the new entity.
TEST_F(EntityInstanceAndroidTest, ToEntityInstance_ReuseExistingAttribute) {
  EntityType entity_type(EntityTypeName::kPassport);
  EntityTypeAndroid entity_type_android(entity_type);

  AttributeType attribute_type(AttributeTypeName::kPassportName);
  AttributeTypeAndroid password_name_attribute_type_android(attribute_type);

  std::u16string passport_name = u"John Doe";

  // Create an existing entity with the same attribute value.
  AttributeInstance existing_attribute(attribute_type);
  existing_attribute.SetRawInfo(attribute_type.field_type(), passport_name,
                                VerificationStatus::kObserved);

  EntityInstance existing_entity(
      entity_type, {existing_attribute}, EntityInstance::EntityId(kGuid),
      /*nickname=*/"", base::Time::Now(), /*use_count=*/1, base::Time::Now(),
      EntityInstance::RecordType::kLocal,
      EntityInstance::AreAttributesReadOnly(false),
      /*frecency_override=*/"");

  // Create an Android entity with the same attribute value.
  AttributeInstanceAndroid attribute_instance_android(
      password_name_attribute_type_android, passport_name);
  EntityInstanceAndroid entity_instance_android(
      entity_type_android, kGuid, EntityInstance::RecordType::kLocal,
      {attribute_instance_android},
      EntityMetadataAndroid(base::Time::Now(), 0));

  EntityInstance converted_entity =
      entity_instance_android.ToEntityInstance(existing_entity);

  ASSERT_EQ(converted_entity.attributes().size(), 1u);
  // The attribute should be reused, including its verification status.
  EXPECT_EQ(converted_entity.attributes()[0].GetVerificationStatus(
                attribute_type.field_type()),
            VerificationStatus::kObserved);
}

// Test that if an existing entity attribute did change when converting an
// Instance from Java to C++, this updated attribute is used when creating the
// new entity. Otherwise the attribute is copied from the original entity.
TEST_F(EntityInstanceAndroidTest, ToEntityInstance_UpdateExistingAttribute) {
  EntityType entity_type(EntityTypeName::kPassport);
  EntityTypeAndroid entity_type_android(entity_type);

  // First create an attribute for an existing entity with a
  // value differently from what is received from Java. Meaning it is different
  // than what will be set in `EntityInstanceAndroid`. In a real world scenario,
  // this means the entity coming from Java will have the attribute modified by
  // the user via the management page.
  AttributeType passport_name_attribute_type(AttributeTypeName::kPassportName);
  AttributeTypeAndroid passport_name_attribute_type_android(
      passport_name_attribute_type);

  std::u16string old_passport_name = u"John Doe";

  AttributeInstance existing_passport_name_attribute(
      passport_name_attribute_type);
  existing_passport_name_attribute.SetRawInfo(
      passport_name_attribute_type.field_type(), old_passport_name,
      VerificationStatus::kObserved);

  // Now create an attribute for an existing entity with a
  // value that matches what is received from Java. In a real world scenario,
  // this means the entity coming from Java did not have the attribute modified
  // by the user via the management page.
  AttributeType passport_number_attribute_type(
      AttributeTypeName::kPassportNumber);
  AttributeTypeAndroid passport_number_attribute_type_android(
      passport_number_attribute_type);

  std::u16string number = u"123456";

  AttributeInstance existing_passport_number_attribute(
      passport_number_attribute_type);
  existing_passport_number_attribute.SetRawInfo(
      passport_number_attribute_type.field_type(), number,
      VerificationStatus::kNoStatus);

  EntityInstance existing_entity(
      entity_type,
      {existing_passport_name_attribute, existing_passport_number_attribute},
      EntityInstance::EntityId(kGuid),
      /*nickname=*/"", base::Time::Now(), /*use_count=*/1, base::Time::Now(),
      EntityInstance::RecordType::kLocal,
      EntityInstance::AreAttributesReadOnly(false),
      /*frecency_override=*/"");

  // Create an Android entity with the attributes (note that only the first one
  // was really modified).
  std::u16string new_passport_name = u"Jane Doe";
  AttributeInstanceAndroid passport_name_attribute_instance_android(
      passport_name_attribute_type_android, new_passport_name);
  AttributeInstanceAndroid passport_number_attribute_instance_android(
      passport_number_attribute_type_android, number);
  EntityInstanceAndroid entity_instance_android(
      entity_type_android, kGuid, EntityInstance::RecordType::kLocal,
      {passport_name_attribute_instance_android,
       passport_number_attribute_instance_android},
      EntityMetadataAndroid(base::Time::Now(), 0));

  EntityInstance converted_entity =
      entity_instance_android.ToEntityInstance(existing_entity);

  ASSERT_EQ(converted_entity.attributes().size(), 2u);

  // The passport name attribute was updated, therefore it should be recreated
  // from scratch, including its verification status.
  const base::optional_ref<const AttributeInstance>
      updated_entity_passport_name =
          converted_entity.attribute(passport_name_attribute_type);
  EXPECT_EQ(updated_entity_passport_name->GetCompleteRawInfo(),
            new_passport_name);
  EXPECT_EQ(updated_entity_passport_name->GetVerificationStatus(
                passport_name_attribute_type.field_type()),
            VerificationStatus::kUserVerified);

  // The number attribute was not updated, therefore it should be copied from
  // the existing entity, including its verification status.
  const base::optional_ref<const AttributeInstance>
      updated_entity_passport_number =
          converted_entity.attribute(passport_number_attribute_type);
  EXPECT_EQ(updated_entity_passport_number->GetCompleteRawInfo(), number);
  // The following condition checks that the number attribute was copied from
  // the original/existing entity.
  EXPECT_EQ(updated_entity_passport_number->GetVerificationStatus(
                passport_number_attribute_type.field_type()),
            VerificationStatus::kNoStatus);
}

}  // namespace

}  // namespace autofill
