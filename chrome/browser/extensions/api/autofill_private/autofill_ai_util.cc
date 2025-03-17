// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/autofill_private/autofill_ai_util.h"

#include <optional>

#include "base/containers/flat_set.h"
#include "base/containers/to_vector.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/types/cxx23_to_underlying.h"
#include "base/uuid.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_structured_address_component.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type_names.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill_private = extensions::api::autofill_private;

namespace extensions::autofill_ai_util {

namespace {

using autofill::AttributeInstance;
using autofill::AttributeType;
using autofill::AttributeTypeName;
using autofill::EntityInstance;
using autofill::EntityType;
using autofill::EntityTypeName;

// Converts an `autofill::EntityInstance` object to an
// `api::autofill_private::EntityInstanceWithLabels` object, according to a
// given `app_locale`.
autofill_private::EntityInstanceWithLabels
EntityInstanceToPrivateApiEntityInstanceWithLabels(
    const EntityInstance& entity_instance,
    const std::string& app_locale) {
  autofill_private::EntityInstanceWithLabels entity_instance_with_labels;
  entity_instance_with_labels.guid = entity_instance.guid().AsLowercaseString();
  // TODO(crbug.com/393318055): Use better labels.
  entity_instance_with_labels.entity_instance_label = base::UTF16ToUTF8(
      entity_instance.attributes()[0].GetCompleteInfo(app_locale));
  entity_instance_with_labels.entity_instance_sub_label =
      base::UTF16ToUTF8(entity_instance.type().GetNameForI18n());
  return entity_instance_with_labels;
}

}  // namespace

std::string GetAddEntityTypeStringForI18n(EntityType entity_type) {
  switch (entity_type.name()) {
    case EntityTypeName::kPassport:
      return l10n_util::GetStringUTF8(IDS_AUTOFILL_AI_ADD_PASSPORT_ENTITY);
    case EntityTypeName::kVehicle:
      return l10n_util::GetStringUTF8(IDS_AUTOFILL_AI_ADD_VEHICLE_ENTITY);
    case EntityTypeName::kDriversLicense:
      return l10n_util::GetStringUTF8(
          IDS_AUTOFILL_AI_ADD_DRIVERS_LICENSE_ENTITY);
  }
  NOTREACHED();
}

std::string GetEditEntityTypeStringForI18n(EntityType entity_type) {
  switch (entity_type.name()) {
    case EntityTypeName::kPassport:
      return l10n_util::GetStringUTF8(IDS_AUTOFILL_AI_EDIT_PASSPORT_ENTITY);
    case EntityTypeName::kVehicle:
      return l10n_util::GetStringUTF8(IDS_AUTOFILL_AI_EDIT_VEHICLE_ENTITY);
    case EntityTypeName::kDriversLicense:
      return l10n_util::GetStringUTF8(
          IDS_AUTOFILL_AI_EDIT_DRIVERS_LICENSE_ENTITY);
  }
  NOTREACHED();
}

api::autofill_private::AttributeTypeDataType
AttributeTypeDataTypeToPrivateApiAttributeTypeDataType(
    autofill::AttributeType::DataType data_type) {
  switch (data_type) {
    case AttributeType::DataType::kCountry:
      return autofill_private::AttributeTypeDataType::kCountry;
    case AttributeType::DataType::kDate:
      return autofill_private::AttributeTypeDataType::kDate;
    case AttributeType::DataType::kName:
    case AttributeType::DataType::kState:
    case AttributeType::DataType::kString:
      return autofill_private::AttributeTypeDataType::kString;
  }
  NOTREACHED();
}

std::optional<EntityInstance> PrivateApiEntityInstanceToEntityInstance(
    const autofill_private::EntityInstance& private_api_entity_instance) {
  base::flat_set<AttributeInstance, AttributeInstance::CompareByType>
      attribute_instances;
  for (const autofill_private::AttributeInstance&
           private_api_attribute_instance :
       private_api_entity_instance.attribute_instances) {
    if (private_api_attribute_instance.type.type_name < 0 ||
        private_api_attribute_instance.type.type_name >
            base::to_underlying(AttributeTypeName::kMaxValue)) {
      return std::nullopt;
    }

    AttributeType attribute_type(
        AttributeTypeName(private_api_attribute_instance.type.type_name));
    AttributeInstance attribute_instance(attribute_type);
    attribute_instance.SetRawInfo(
        attribute_instance.type().field_type(),
        base::UTF8ToUTF16(private_api_attribute_instance.value),
        autofill::VerificationStatus::kUserVerified);
    attribute_instance.FinalizeInfo();
    attribute_instances.emplace(std::move(attribute_instance));
  }

  std::optional<EntityTypeName> entity_type_name =
      autofill::ToSafeEntityTypeName(
          private_api_entity_instance.type.type_name);
  if (!entity_type_name.has_value()) {
    return std::nullopt;
  }
  EntityType entity_type(entity_type_name.value());
  // Newly added entity instances need to have a guid generated for them.
  base::Uuid guid =
      private_api_entity_instance.guid.empty()
          ? base::Uuid::GenerateRandomV4()
          : base::Uuid::ParseLowercase(private_api_entity_instance.guid);
  return EntityInstance(std::move(entity_type), attribute_instances,
                        std::move(guid), private_api_entity_instance.nickname,
                        base::Time::Now());
}

autofill_private::EntityInstance EntityInstanceToPrivateApiEntityInstance(
    const EntityInstance& entity_instance,
    const std::string& app_locale) {
  std::vector<autofill_private::AttributeInstance>
      private_api_attribute_instances;
  for (const AttributeInstance& attribute_instance :
       entity_instance.attributes()) {
    private_api_attribute_instances.emplace_back();
    private_api_attribute_instances.back().type.type_name =
        base::to_underlying(attribute_instance.type().name());
    private_api_attribute_instances.back().type.type_name_as_string =
        base::UTF16ToUTF8(attribute_instance.type().GetNameForI18n());
    private_api_attribute_instances.back().type.data_type =
        AttributeTypeDataTypeToPrivateApiAttributeTypeDataType(
            attribute_instance.type().data_type());
    private_api_attribute_instances.back().value =
        base::UTF16ToUTF8(attribute_instance.GetCompleteInfo(app_locale));
  }

  autofill_private::EntityInstance private_api_entity_instance;
  private_api_entity_instance.type.type_name =
      base::to_underlying(entity_instance.type().name());
  private_api_entity_instance.type.type_name_as_string =
      base::UTF16ToUTF8(entity_instance.type().GetNameForI18n());
  private_api_entity_instance.type.add_entity_type_string =
      GetAddEntityTypeStringForI18n(entity_instance.type());
  private_api_entity_instance.type.edit_entity_type_string =
      GetEditEntityTypeStringForI18n(entity_instance.type());
  private_api_entity_instance.attribute_instances =
      std::move(private_api_attribute_instances);
  private_api_entity_instance.guid = entity_instance.guid().AsLowercaseString();
  private_api_entity_instance.nickname = entity_instance.nickname();
  return private_api_entity_instance;
}

std::vector<autofill_private::EntityInstanceWithLabels>
EntityInstancesToPrivateApiEntityInstancesWithLabels(
    base::span<const EntityInstance> entity_instances,
    const std::string& app_locale) {
  return base::ToVector(
      entity_instances, [&](const EntityInstance& entity_instance) {
        return EntityInstanceToPrivateApiEntityInstanceWithLabels(
            entity_instance, app_locale);
      });
}

}  // namespace extensions::autofill_ai_util
