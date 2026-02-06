// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_AUTOFILL_PRIVATE_AUTOFILL_AI_UTIL_H_
#define CHROME_BROWSER_EXTENSIONS_API_AUTOFILL_PRIVATE_AUTOFILL_AI_UTIL_H_

#include <optional>
#include <string_view>

#include "chrome/common/extensions/api/autofill_private.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"

namespace autofill {
class EntityInstance;
}  // namespace autofill

namespace extensions::autofill_ai_util {

// Converts an `autofill::AttributeType::DataType` enum entry to an
// `api::autofill_private::AttributeTypeDataType` enum entry.
api::autofill_private::AttributeTypeDataType
AttributeTypeDataTypeToPrivateApiAttributeTypeDataType(
    autofill::AttributeType::DataType data_type);

// Converts an `api::autofill_private::EntityInstance` object to an
// `autofill::EntityInstance` object, according to a given `app_locale`. Returns
// `std::nullopt` if one of the attribute types or entity types are out of
// bounds of the enum, or if the date value is used incorrectly.
// The `entity_supports_wallet_storage` is used to validate if the entity is
// allowed to be stored in the Google wallet.
std::optional<autofill::EntityInstance>
PrivateApiEntityInstanceToEntityInstance(
    const api::autofill_private::EntityInstance& private_api_entity_instance,
    std::string_view app_locale,
    bool entity_supports_wallet_storage);

// Converts an `autofill::EntityInstance` object to an
// `api::autofill_private::EntityInstance` object, according to a given
// `app_locale`.
api::autofill_private::EntityInstance EntityInstanceToPrivateApiEntityInstance(
    const autofill::EntityInstance& entity_instance,
    std::string_view app_locale,
    bool entity_supports_wallet_storage);

// Converts `autofill::EntityInstance`s to
// `api::autofill_private::EntityInstanceWithLabels`s according to a given
// `app_locale`.
std::vector<api::autofill_private::EntityInstanceWithLabels>
EntityInstancesToPrivateApiEntityInstancesWithLabels(
    base::span<const autofill::EntityInstance> entity_instances,
    bool obfuscate_sensitive_types,
    std::string_view app_locale);

api::autofill_private::EntityType EntityTypeToPrivateApiEntityType(
    autofill::EntityType entity_type,
    bool supports_wallet_storage);

// Returns the import constraints for `entity_type` as a list of API attribute
// types. The returned list represents a disjunction of requirements (OR logic),
// where satisfying any one attribute satisfies the import requirement.
// Note: This enforces that the underlying schema uses singleton groups (e.g.,
// {{A}, {B}}); complex conjunctions will trigger a runtime error.
std::vector<api::autofill_private::AttributeType> GetRequiredAttributesForType(
    autofill::EntityType entity_type);

// Converts a core autofill::AttributeType to the Private API AttributeType.
api::autofill_private::AttributeType AttributeTypeToPrivateApiAttributeType(
    autofill::AttributeType attribute_type);

}  // namespace extensions::autofill_ai_util

#endif  // CHROME_BROWSER_EXTENSIONS_API_AUTOFILL_PRIVATE_AUTOFILL_AI_UTIL_H_
