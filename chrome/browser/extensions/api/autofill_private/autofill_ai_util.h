// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_AUTOFILL_PRIVATE_AUTOFILL_AI_UTIL_H_
#define CHROME_BROWSER_EXTENSIONS_API_AUTOFILL_PRIVATE_AUTOFILL_AI_UTIL_H_

#include <optional>

#include "chrome/common/extensions/api/autofill_private.h"

namespace autofill {
class EntityInstance;
class EntityType;
}  // namespace autofill

namespace extensions::autofill_ai_util {

// Returns the i18n string representation of "Add <entity>". For example, for a
// passport for "en-US", this function should return "Add passport".
std::string GetAddEntityStringForI18n(autofill::EntityType entity_type);

// Returns the i18n string representation of "Edit <entity>". For example, for a
// passport for "en-US", this function should return "Edit passport".
std::string GetEditEntityStringForI18n(autofill::EntityType entity_type);

// Converts an `api::autofill_private::EntityInstance` object to an
// `autofill::EntityInstance` object. Returns `std::nullopt` if one of the
// attribute types or entity types are out of bounds of the enum.
std::optional<autofill::EntityInstance>
PrivateApiEntityInstanceToEntityInstance(
    const api::autofill_private::EntityInstance& private_api_entity_instance);

// Converts an `autofill::EntityInstance` object to an
// `api::autofill_private::EntityInstance` object, according to a given
// `app_locale`.
api::autofill_private::EntityInstance EntityInstanceToPrivateApiEntityInstance(
    const autofill::EntityInstance& entity_instance,
    const std::string& app_locale);

// Converts an `autofill::EntityInstance` object to an
// `api::autofill_private::EntityInstanceWithLabels` object, according to a
// given `app_locale`.
api::autofill_private::EntityInstanceWithLabels
EntityInstanceToPrivateApiEntityInstanceWithLabels(
    const autofill::EntityInstance& entity_instance,
    const std::string& app_locale);

}  // namespace extensions::autofill_ai_util

#endif  // CHROME_BROWSER_EXTENSIONS_API_AUTOFILL_PRIVATE_AUTOFILL_AI_UTIL_H_
