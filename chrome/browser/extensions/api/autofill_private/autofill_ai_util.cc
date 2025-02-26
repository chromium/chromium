// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/autofill_private/autofill_ai_util.h"

#include <optional>

#include "base/containers/flat_set.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/types/cxx23_to_underlying.h"
#include "base/uuid.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_structured_address_component.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type_names.h"

namespace autofill_private = extensions::api::autofill_private;

namespace extensions::autofill_ai_util {

using autofill::AttributeInstance;
using autofill::AttributeTypeName;
using autofill::EntityInstance;
using autofill::EntityTypeName;

std::optional<EntityInstance> PrivateApiEntityInstanceToEntityInstance(
    const autofill_private::EntityInstance& private_api_entity_instance) {
  base::flat_set<AttributeInstance, AttributeInstance::CompareByType>
      attributes;
  for (const autofill_private::AttributeInstance&
           private_api_attribute_instance :
       private_api_entity_instance.attributes) {
    if (private_api_attribute_instance.type < 0 ||
        private_api_attribute_instance.type >
            base::to_underlying(AttributeTypeName::kMaxValue)) {
      return std::nullopt;
    }

    autofill::AttributeType attribute_type(
        AttributeTypeName(private_api_attribute_instance.type));
    AttributeInstance attribute(attribute_type);
    attribute.SetInfoWithVerificationStatus(
        attribute.GetTopLevelType(),
        base::UTF8ToUTF16(private_api_attribute_instance.value),
        autofill::VerificationStatus::kUserVerified);
    attributes.emplace(std::move(attribute));
  }

  if (private_api_entity_instance.type < 0 ||
      private_api_entity_instance.type >
          base::to_underlying(EntityTypeName::kMaxValue)) {
    return std::nullopt;
  }
  std::optional<EntityTypeName> entity_type_name =
      autofill::ToSafeEntityTypeName(private_api_entity_instance.type);
  if (!entity_type_name.has_value()) {
    return std::nullopt;
  }
  autofill::EntityType entity_type(entity_type_name.value());
  return EntityInstance(
      std::move(entity_type), attributes,
      base::Uuid::ParseLowercase(private_api_entity_instance.guid),
      private_api_entity_instance.nickname, base::Time::Now());
}

autofill_private::EntityInstance EntityInstanceToPrivateApiEntityInstance(
    const EntityInstance& entity_instance) {
  std::vector<autofill_private::AttributeInstance> private_api_attributes;
  for (const AttributeInstance& attribute_instance :
       entity_instance.attributes()) {
    private_api_attributes.emplace_back();
    private_api_attributes.back().type =
        base::to_underlying(attribute_instance.type().name());
    private_api_attributes.back().value =
        base::UTF16ToUTF8(attribute_instance.value());
  }

  autofill_private::EntityInstance private_api_entity_instance;
  private_api_entity_instance.type =
      base::to_underlying(entity_instance.type().name());
  private_api_entity_instance.attributes = std::move(private_api_attributes);
  private_api_entity_instance.guid = entity_instance.guid().AsLowercaseString();
  private_api_entity_instance.nickname = entity_instance.nickname();
  return private_api_entity_instance;
}

}  // namespace extensions::autofill_ai_util
