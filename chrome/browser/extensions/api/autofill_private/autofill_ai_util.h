// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_AUTOFILL_PRIVATE_AUTOFILL_AI_UTIL_H_
#define CHROME_BROWSER_EXTENSIONS_API_AUTOFILL_PRIVATE_AUTOFILL_AI_UTIL_H_

#include <optional>

#include "chrome/common/extensions/api/autofill_private.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"

namespace autofill {
class EntityInstance;
}  // namespace autofill

namespace extensions::autofill_ai_util {

// Returns the i18n string representation of "Add <entity type>". For example,
// for a passport for "en-US", this function should return "Add passport".
std::string GetAddEntityTypeStringForI18n(autofill::EntityType entity_type);

// Returns the i18n string representation of "Edit <entity type>". For example,
// for a passport for "en-US", this function should return "Edit passport".
std::string GetEditEntityTypeStringForI18n(autofill::EntityType entity_type);

// Returns the i18n string representation of "Delete <entity type>". For
// example, for a passport for "en-US", this function should return "Delete
// passport".
std::string GetDeleteEntityTypeStringForI18n(autofill::EntityType entity_type);

// Converts an `autofill::AttributeType::DataType` enum entry to an
// `api::autofill_private::AttributeTypeDataType` enum entry.
api::autofill_private::AttributeTypeDataType
AttributeTypeDataTypeToPrivateApiAttributeTypeDataType(
    autofill::AttributeType::DataType data_type);

// Converts an `api::autofill_private::EntityInstance` object to an
// `autofill::EntityInstance` object, according to a given `app_locale`. Returns
// `std::nullopt` if one of the attribute types or entity types are out of
// bounds of the enum, or if the date value is used incorrectly.
std::optional<autofill::EntityInstance>
PrivateApiEntityInstanceToEntityInstance(
    const api::autofill_private::EntityInstance& private_api_entity_instance,
    const std::string& app_locale);

// Converts an `autofill::EntityInstance` object to an
// `api::autofill_private::EntityInstance` object, according to a given
// `app_locale`.
api::autofill_private::EntityInstance EntityInstanceToPrivateApiEntityInstance(
    const autofill::EntityInstance& entity_instance,
    const std::string& app_locale);

// Converts `autofill::EntityInstance`s to
// `api::autofill_private::EntityInstanceWithLabels`s according to a given
// `app_locale`.
std::vector<api::autofill_private::EntityInstanceWithLabels>
EntityInstancesToPrivateApiEntityInstancesWithLabels(
    base::span<const autofill::EntityInstance> entity_instances,
    const std::string& app_locale);

api::autofill_private::EntityType EntityTypeToPrivateApiEntityType(
    const autofill::EntityType& entity_type,
    bool supports_wallet_storage);

}  // namespace extensions::autofill_ai_util

#endif  // CHROME_BROWSER_EXTENSIONS_API_AUTOFILL_PRIVATE_AUTOFILL_AI_UTIL_H_
