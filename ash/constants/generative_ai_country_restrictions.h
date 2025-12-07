// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CONSTANTS_GENERATIVE_AI_COUNTRY_RESTRICTIONS_H_
#define ASH_CONSTANTS_GENERATIVE_AI_COUNTRY_RESTRICTIONS_H_

#include <string>
#include <vector>

#include "base/component_export.h"

namespace ash {

// If the country code isn't recognized, false will be returned.
COMPONENT_EXPORT(ASH_CONSTANTS)
bool IsGenerativeAiAllowedForCountry(std::string_view country_code);

COMPONENT_EXPORT(ASH_CONSTANTS)
std::vector<std::string> GetGenerativeAiCountryAllowlist();

}  // namespace ash

#endif  // ASH_CONSTANTS_GENERATIVE_AI_COUNTRY_RESTRICTIONS_H_
