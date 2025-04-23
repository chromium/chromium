// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/autofill_private/autofill_ai_util.h"

#include <optional>
#include <string>

#include "base/containers/flat_set.h"
#include "base/containers/to_vector.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/types/cxx23_to_underlying.h"
#include "base/uuid.h"
#include "base/values.h"
#include "chrome/common/extensions/api/autofill_private.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_structured_address_component.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type_names.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill_ai/core/browser/autofill_ai_utils.h"
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

// Converts a vector of `autofill::EntityInstance` objects into a vector of
// `api::autofill_private::EntityInstanceWithLabels`, according to a
// given `app_locale`. Appends the result to `output`.
// This function works as follows:
//
// 1. Retrieve all available labels for each entity using
//    `GetLabelsForEntities()`.
//
// 2. Creates for each entity an
//    `api::autofill_private::EntityInstanceWithLabels`,
//    setting its label (first line in the settings page list) as the entity
//    name, and its sublabel (second line) as the concatenation of the final
//    list of labels defined in 3#. Appends
//    `api::autofill_private::EntityInstanceWithLabels` to the output.
void EntityInstanceToPrivateApiEntityInstanceWithLabels(
    base::span<const EntityInstance*> entity_instances,
    const std::string& app_locale,
    std::vector<autofill_private::EntityInstanceWithLabels>& output) {
  // Step 1#, get all available labels for `entity_instances`.
  const autofill_ai::EntitiesLabels labels_for_entities =
      autofill_ai::GetLabelsForEntities(
          entity_instances,
          /*allow_only_disambiguating_types=*/false,
          /*return_at_least_one_label=*/true, app_locale);

  // Step 2#
  // Update the `output` with each entity's respective
  // `autofill_private::EntityInstanceWithLabels`, making sure to set the label
  //  and sublabels. In the context of the settings page,
  // `autofill_private::EntityInstanceWithLabels::entity_instance_label` is the
  // first line of each entities list (the equivalent of a filling suggestion
  // main text) while
  // `autofill_private::EntityInstanceWithLabels::entity_instance_sub_label` is
  // the second line.
  std::vector<autofill_private::EntityInstanceWithLabels>
      entities_instances_with_labels;
  CHECK_EQ(entity_instances.size(), labels_for_entities->size());
  for (size_t i = 0; i < entity_instances.size(); i++) {
    const EntityInstance& entity_instance = *entity_instances[i];
    autofill_private::EntityInstanceWithLabels& entity_instance_with_labels =
        output.emplace_back();
    entity_instance_with_labels.guid =
        entity_instance.guid().AsLowercaseString();
    entity_instance_with_labels.entity_instance_label =
        base::UTF16ToUTF8(entity_instance.type().GetNameForI18n());
    entity_instance_with_labels.entity_instance_sub_label =
        base::UTF16ToUTF8(base::JoinString((*labels_for_entities)[i],
                                           autofill_ai::kLabelSeparator));
  }
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

std::string GetDeleteEntityTypeStringForI18n(EntityType entity_type) {
  switch (entity_type.name()) {
    case EntityTypeName::kPassport:
      return l10n_util::GetStringUTF8(IDS_AUTOFILL_AI_DELETE_PASSPORT_ENTITY);
    case EntityTypeName::kVehicle:
      return l10n_util::GetStringUTF8(IDS_AUTOFILL_AI_DELETE_VEHICLE_ENTITY);
    case EntityTypeName::kDriversLicense:
      return l10n_util::GetStringUTF8(
          IDS_AUTOFILL_AI_DELETE_DRIVERS_LICENSE_ENTITY);
  }
  NOTREACHED();
}

api::autofill_private::AttributeTypeDataType
AttributeTypeDataTypeToPrivateApiAttributeTypeDataType(
    AttributeType::DataType data_type) {
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
    const autofill_private::EntityInstance& private_api_entity_instance,
    const std::string& app_locale) {
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
    AttributeInstance attribute_instance(std::move(attribute_type));

    if (attribute_instance.type().data_type() ==
        AttributeType::DataType::kDate) {
      const std::optional<autofill_private::DateValue>& date =
          private_api_attribute_instance.value.as_date_value;
      if (!date.has_value()) {
        return std::nullopt;
      }
      if (date->month.empty() || date->day.empty() || date->year.empty()) {
        return std::nullopt;
      }

      attribute_instance.SetInfo(attribute_instance.type().field_type(),
                                 base::UTF8ToUTF16(date->month), app_locale,
                                 u"M",
                                 autofill::VerificationStatus::kUserVerified);
      attribute_instance.SetInfo(attribute_instance.type().field_type(),
                                 base::UTF8ToUTF16(date->day), app_locale, u"D",
                                 autofill::VerificationStatus::kUserVerified);
      attribute_instance.SetInfo(
          attribute_instance.type().field_type(), base::UTF8ToUTF16(date->year),
          app_locale, u"YYYY", autofill::VerificationStatus::kUserVerified);
    } else {
      if (!private_api_attribute_instance.value.as_string.has_value()) {
        return std::nullopt;
      }
      attribute_instance.SetRawInfo(
          attribute_instance.type().field_type(),
          base::UTF8ToUTF16(
              private_api_attribute_instance.value.as_string.value()),
          autofill::VerificationStatus::kUserVerified);
    }
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
                        base::Time::Now(), /*use_count=*/0,
                        /*use_date=*/base::Time::Now());
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

    AttributeType::DataType data_type = attribute_instance.type().data_type();
    private_api_attribute_instances.back().type.data_type =
        AttributeTypeDataTypeToPrivateApiAttributeTypeDataType(data_type);

    if (data_type == AttributeType::DataType::kDate) {
      autofill::FieldType field_type = attribute_instance.type().field_type();
      base::DictValue date_value;
      date_value.SetByDottedPath(
          "month", base::UTF16ToUTF8(attribute_instance.GetInfo(
                       field_type, app_locale, std::u16string(u"M"))));
      date_value.SetByDottedPath(
          "day", base::UTF16ToUTF8(attribute_instance.GetInfo(
                     field_type, app_locale, std::u16string(u"D"))));
      date_value.SetByDottedPath(
          "year", base::UTF16ToUTF8(attribute_instance.GetInfo(
                      field_type, app_locale, std::u16string(u"YYYY"))));
      autofill_private::AttributeInstance::Value::Populate(
          base::Value(std::move(date_value)),
          private_api_attribute_instances.back().value);
    } else {
      autofill_private::AttributeInstance::Value::Populate(
          base::Value(base::UTF16ToUTF8(
              attribute_instance.GetCompleteInfo(app_locale))),
          private_api_attribute_instances.back().value);
    }
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
  private_api_entity_instance.type.delete_entity_type_string =
      GetDeleteEntityTypeStringForI18n(entity_instance.type());
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
  // Entity labels should be generated based on other entities of the same
  // type. This is because the disambiguation values of attributes are only
  // relevant inside a specific entity type.
  std::map<EntityType, std::vector<const EntityInstance*>> entities_per_type;
  for (const EntityInstance& entity : entity_instances) {
    entities_per_type[entity.type()].push_back(&entity);
  }
  std::vector<autofill_private::EntityInstanceWithLabels> response;
  response.reserve(entity_instances.size());
  for (auto& [entity_type, entities] : entities_per_type) {
    EntityInstanceToPrivateApiEntityInstanceWithLabels(entities, app_locale,
                                                       response);
  }
  return response;
}
}  // namespace extensions::autofill_ai_util
