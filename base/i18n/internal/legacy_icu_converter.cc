// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/i18n/internal/legacy_icu_converter.h"

#include <algorithm>
#include <string>
#include <string_view>
#include <vector>

#include "base/containers/fixed_flat_map.h"
#include "base/strings/strcat.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"

namespace base::i18n::internal {

namespace {

// Mappings from legacy ICU keyword keys to BCP47 Unicode locale extension keys.
// These allow converting legacy ICU locale identifiers (e.g.
// "en_US@calendar=gregorian") to BCP47 identifiers (e.g. "en-US-u-ca-gregory").
// See https://www.unicode.org/reports/tr35/#Key_And_Type_Definitions_
constexpr auto kLegacyIcuToBcp47KeyMap =
    base::MakeFixedFlatMap<std::string_view, std::string_view>({
        {"calendar", "ca"},
        {"colalternate", "ka"},
        {"colbackwards", "kb"},
        {"colcasefirst", "kf"},
        {"colcaselevel", "kc"},
        {"colhiraganaquaternary", "kh"},
        {"collation", "co"},
        {"colnormalization", "kk"},
        {"colnumeric", "kn"},
        {"colreorder", "kr"},
        {"colstrength", "ks"},
        {"currency", "cu"},
        {"hours", "hc"},
        {"measure", "ms"},
        {"numbers", "nu"},
        {"timezone", "tz"},
        {"variabletop", "vt"},
    });

// Mappings from legacy ICU keyword values to BCP47 Unicode locale extension
// types. The keys in this map are in the format
// "lowercase_legacy_key:legacy_value". See
// https://www.unicode.org/reports/tr35/#Key_And_Type_Definitions_
constexpr auto kLegacyIcuToBcp47ValueMap =
    base::MakeFixedFlatMap<std::string_view, std::string_view>({
        {"calendar:ethiopic-amete-alem", "ethioaa"},
        {"calendar:gregorian", "gregory"},
        {"colalternate:non-ignorable", "noignore"},
        {"colbackwards:no", "false"},
        {"colbackwards:yes", "true"},
        {"colcasefirst:no", "false"},
        {"colcaselevel:no", "false"},
        {"colcaselevel:yes", "true"},
        {"colhiraganaquaternary:no", "false"},
        {"colhiraganaquaternary:yes", "true"},
        {"collation:dictionary", "dict"},
        {"collation:gb2312han", "gb2312"},
        {"collation:phonebook", "phonebk"},
        {"collation:traditional", "trad"},
        {"colnormalization:no", "false"},
        {"colnormalization:yes", "true"},
        {"colnumeric:no", "false"},
        {"colnumeric:yes", "true"},
        {"colstrength:identical", "identic"},
        {"colstrength:primary", "level1"},
        {"colstrength:quaternary", "level4"},
        {"colstrength:secondary", "level2"},
        {"colstrength:tertiary", "level3"},
        {"measure:imperial", "uksystem"},
        {"numbers:traditional", "traditio"},
        {"timezone:aqams", "aqmcm"},
        {"timezone:aukns", "auhba"},
        {"timezone:caffs", "cawnp"},
        {"timezone:camtr", "cator"},
        {"timezone:canpg", "cator"},
        {"timezone:capnt", "caiql"},
        {"timezone:cathu", "cator"},
        {"timezone:cayzf", "caedm"},
        {"timezone:cnckg", "cnsha"},
        {"timezone:cnhrb", "cnsha"},
        {"timezone:cnkhg", "cnurc"},
        {"timezone:cst6cdt", "uschi"},
        {"timezone:est5edt", "usnyc"},
        {"timezone:gaza", "gazastrp"},
        {"timezone:mncoq", "mnuln"},
        {"timezone:mst7mdt", "usden"},
        {"timezone:mxstis", "mxtij"},
        {"timezone:pst8pdt", "uslax"},
        {"timezone:uaozh", "uaiev"},
        {"timezone:uauzh", "uaiev"},
        {"timezone:umjon", "ushnl"},
        {"timezone:usnavajo", "usden"},
    });

std::string ConvertLegacyExtensionToUnicode(std::string_view extension) {
  size_t eq_pos = extension.find('=');
  // The case where it is not a key=value.
  if (eq_pos == std::string::npos) {
    // Special case for "valencia" which is a variant.
    // In BCP47 it's mapped to the "va" key.
    // See https://www.unicode.org/reports/tr35/#Key_And_Type_Definitions_
    if (extension == "valencia") {
      return "va-valencia";
    }
    auto it = kLegacyIcuToBcp47KeyMap.find(extension);
    if (it != kLegacyIcuToBcp47KeyMap.end()) {
      return std::string(it->second);
    }
    return std::string(extension);
  }

  // The case where it is a key/value chunk.
  std::string_view key = extension.substr(0, eq_pos);
  std::string_view value = extension.substr(eq_pos + 1);

  std::string bcp47_key;
  std::string lowercase_key = base::ToLowerASCII(key);
  if (auto it = kLegacyIcuToBcp47KeyMap.find(lowercase_key);
      it != kLegacyIcuToBcp47KeyMap.end()) {
    bcp47_key = std::string(it->second);
  } else {
    bcp47_key = std::string(key);
  }

  std::string lookup_key = base::StrCat({lowercase_key, ":", value});
  std::string bcp47_value;
  if (auto it = kLegacyIcuToBcp47ValueMap.find(lookup_key);
      it != kLegacyIcuToBcp47ValueMap.end()) {
    bcp47_value = std::string(it->second);
  } else {
    bcp47_value = std::string(value);
  }

  return base::StrCat({bcp47_key, "-", bcp47_value});
}

std::string ConvertLegacyExtensionsToUnicode(std::string_view extensions) {
  if (extensions.empty()) {
    return "";
  }

  // BCP47 Unicode extensions start with -u-.
  // See https://www.unicode.org/reports/tr35/#Unicode_locale_identifier
  std::string output("-u");

  // Split keywords by ';' or ','
  size_t start = 0;
  size_t end;
  while ((end = extensions.find_first_of(";,", start)) != std::string::npos ||
         start < extensions.size()) {
    std::string_view kv;
    if (end == std::string::npos) {
      kv = extensions.substr(start);
      start = extensions.size();
    } else {
      kv = extensions.substr(start, end - start);
      start = end + 1;
    }

    // if the extension is just a sequence of ';' or ',', nothing is done to
    // prevent a weird unicode extension like "u----".
    if (kv.empty()) {
      continue;
    }

    base::StrAppend(&output, {"-", ConvertLegacyExtensionToUnicode(kv)});
  }

  return output;
}

}  // namespace

std::optional<std::string> ConvertLegacyCodeToBcp47IfNecessary(
    std::string_view code) {
  size_t at_pos = code.find('@');
  size_t underline_pos = code.find('_');
  // Legacy ICU locale IDs use '_' as a separator (e.g., "en_US")
  // and '@' to start the keywords section (e.g., "en_US@calendar=gregorian").
  // BCP47 uses '-' as a separator and "-u-" for Unicode extensions.
  if (at_pos == std::string::npos && underline_pos == std::string::npos) {
    return std::nullopt;
  }

  // We init it with the string that is before the '@' sign.
  std::string normalized_code = std::string(code.substr(0, at_pos));
  if (underline_pos != std::string::npos) {
    // Replace "_" by "-" for BCP47 compatibility.
    std::ranges::replace(normalized_code, '_', '-');
  }

  if (at_pos != std::string::npos) {
    std::string unicode_extension =
        ConvertLegacyExtensionsToUnicode(code.substr(at_pos + 1));
    base::StrAppend(&normalized_code, {unicode_extension});
  }

  return normalized_code;
}

}  // namespace base::i18n::internal
