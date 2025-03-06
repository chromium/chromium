// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/autofill_private/autofill_ai_util.h"

#include <optional>

#include "base/containers/flat_set.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/types/cxx23_to_underlying.h"
#include "base/uuid.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_structured_address_component.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type_names.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill_private = extensions::api::autofill_private;

namespace extensions::autofill_ai_util {

using autofill::AttributeInstance;
using autofill::AttributeTypeName;
using autofill::EntityInstance;
using autofill::EntityType;
using autofill::EntityTypeName;

std::string GetAddEntityStringForI18n(EntityType entity_type) {
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

std::string GetEditEntityStringForI18n(EntityType entity_type) {
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

std::optional<EntityInstance> PrivateApiEntityInstanceToEntityInstance(
    const autofill_private::EntityInstance& private_api_entity_instance) {
  base::flat_set<AttributeInstance, AttributeInstance::CompareByType>
      attributes;
  for (const autofill_private::AttributeInstance&
           private_api_attribute_instance :
       private_api_entity_instance.attributes) {
    if (private_api_attribute_instance.type.type_name < 0 ||
        private_api_attribute_instance.type.type_name >
            base::to_underlying(AttributeTypeName::kMaxValue)) {
      return std::nullopt;
    }

    autofill::AttributeType attribute_type(
        AttributeTypeName(private_api_attribute_instance.type.type_name));
    AttributeInstance attribute(attribute_type);
    attribute.SetRawInfo(
        attribute.type().field_type(),
        base::UTF8ToUTF16(private_api_attribute_instance.value),
        autofill::VerificationStatus::kUserVerified);
    attribute.FinalizeInfo();
    attributes.emplace(std::move(attribute));
  }

  std::optional<EntityTypeName> entity_type_name =
      autofill::ToSafeEntityTypeName(
          private_api_entity_instance.type.type_name);
  if (!entity_type_name.has_value()) {
    return std::nullopt;
  }
  EntityType entity_type(entity_type_name.value());
  // Newly added entities need to have a guid generated for them.
  base::Uuid guid =
      private_api_entity_instance.guid.empty()
          ? base::Uuid::GenerateRandomV4()
          : base::Uuid::ParseLowercase(private_api_entity_instance.guid);
  return EntityInstance(std::move(entity_type), attributes, std::move(guid),
                        private_api_entity_instance.nickname,
                        base::Time::Now());
}

autofill_private::EntityInstance EntityInstanceToPrivateApiEntityInstance(
    const EntityInstance& entity_instance,
    const std::string& app_locale) {
  std::vector<autofill_private::AttributeInstance> private_api_attributes;
  for (const AttributeInstance& attribute_instance :
       entity_instance.attributes()) {
    private_api_attributes.emplace_back();
    private_api_attributes.back().type.type_name =
        base::to_underlying(attribute_instance.type().name());
    private_api_attributes.back().type.type_name_as_string =
        base::UTF16ToUTF8(attribute_instance.type().GetNameForI18n());
    private_api_attributes.back().value =
        base::UTF16ToUTF8(attribute_instance.GetCompleteInfo(app_locale));
  }

  autofill_private::EntityInstance private_api_entity_instance;
  private_api_entity_instance.type.type_name =
      base::to_underlying(entity_instance.type().name());
  private_api_entity_instance.type.type_name_as_string =
      base::UTF16ToUTF8(entity_instance.type().GetNameForI18n());
  private_api_entity_instance.type.add_entity_string =
      GetAddEntityStringForI18n(entity_instance.type());
  private_api_entity_instance.type.edit_entity_string =
      GetEditEntityStringForI18n(entity_instance.type());
  private_api_entity_instance.attributes = std::move(private_api_attributes);
  private_api_entity_instance.guid = entity_instance.guid().AsLowercaseString();
  private_api_entity_instance.nickname = entity_instance.nickname();
  return private_api_entity_instance;
}

autofill_private::EntityInstanceWithLabels
EntityInstanceToPrivateApiEntityInstanceWithLabels(
    const EntityInstance& entity_instance,
    const std::string& app_locale) {
  autofill_private::EntityInstanceWithLabels entity_instance_with_labels;
  entity_instance_with_labels.guid = entity_instance.guid().AsLowercaseString();
  // TODO(crbug.com/393318055): Use better labels;
  entity_instance_with_labels.entity_label = base::UTF16ToUTF8(
      entity_instance.attributes()[0].GetCompleteInfo(app_locale));
  entity_instance_with_labels.entity_sub_label =
      base::UTF16ToUTF8(entity_instance.type().GetNameForI18n());
  return entity_instance_with_labels;
}

}  // namespace extensions::autofill_ai_util
