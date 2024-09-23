// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_management_internal.h"

#include <stdint.h>

#include <optional>
#include <string_view>

#include "base/json/json_reader.h"
#include "base/values.h"
#include "third_party/icu/fuzzers/fuzzer_utils.h"

using extensions::internal::IndividualSettings;

namespace {

constexpr IndividualSettings::ParsingScope kAllParsingScopes[] = {
    IndividualSettings::SCOPE_DEFAULT,
    IndividualSettings::SCOPE_UPDATE_URL,
    IndividualSettings::SCOPE_INDIVIDUAL,
};

// Performs common initialization that's shared between all runs.
struct Environment {
  IcuEnvironment icu_environment;
};

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  static Environment env;

  // Avoid out-of-memory failures on excessively large inputs (the exact
  // threshold is semi-arbitrary).
  if (size > 100 * 1024)
    return 0;

  std::string_view json(reinterpret_cast<const char*>(data), size);
  std::optional<base::Value> value = base::JSONReader::Read(json);
  if (!value || !value->is_dict())
    return 0;

  for (auto parsing_scope : kAllParsingScopes) {
    IndividualSettings settings;
    settings.Parse(value->GetDict(), parsing_scope);
  }
  return 0;
}
