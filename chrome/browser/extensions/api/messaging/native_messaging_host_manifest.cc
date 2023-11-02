// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/messaging/native_messaging_host_manifest.h"

#include <stddef.h>

#include "base/check.h"
#include "base/json/json_file_value_serializer.h"
#include "base/strings/string_util.h"
#include "chrome/common/chrome_features.h"

namespace extensions {

NativeMessagingHostManifest::~NativeMessagingHostManifest() {}

// static
bool NativeMessagingHostManifest::IsValidName(const std::string& name) {
  if (name.empty()) {
    return false;
  }

  for (size_t i = 0; i < name.size(); ++i) {
    char c = name[i];

    // Verify that only the following characters are used: [a-z0-9._].
    if (!(base::IsAsciiLower(c) || base::IsAsciiDigit(c) || c == '.' ||
          c == '_')) {
      return false;
    }

    // Verify that dots are separated by other characters and that string
    // doesn't begin or end with a dot.
    if (c == '.' && (i == 0 || name[i - 1] == '.' || i == name.size() - 1)) {
      return false;
    }
  }

  return true;
}

// static
std::unique_ptr<NativeMessagingHostManifest> NativeMessagingHostManifest::Load(
    const base::FilePath& file_path,
    std::string* error_message) {
  DCHECK(error_message);

  JSONFileValueDeserializer deserializer(file_path);
  std::unique_ptr<base::Value> parsed =
      deserializer.Deserialize(nullptr, error_message);
  if (!parsed) {
    return nullptr;
  }

  if (!parsed->is_dict()) {
    *error_message = "Invalid manifest file.";
    return nullptr;
  }
  const base::Value::Dict& dict = parsed->GetDict();

  std::unique_ptr<NativeMessagingHostManifest> result(
      new NativeMessagingHostManifest());
  if (!result->Parse(dict, error_message)) {
    return nullptr;
  }

  return result;
}

NativeMessagingHostManifest::NativeMessagingHostManifest() {
}

bool NativeMessagingHostManifest::Parse(const base::Value::Dict& dict,
                                        std::string* error_message) {
  const std::string* name_str = dict.FindString("name");
  if (!name_str || !IsValidName(*name_str)) {
    *error_message = "Invalid value for name.";
    return false;
  }
  name_ = *name_str;

  const std::string* desc_str = dict.FindString("description");
  if (!desc_str || desc_str->empty()) {
    *error_message = "Invalid value for description.";
    return false;
  }
  description_ = *desc_str;

  const std::string* type = dict.FindString("type");
  // stdio is the only host type that's currently supported.
  if (!type || *type != "stdio") {
    *error_message = "Invalid value for type.";
    return false;
  }
  interface_ = HOST_INTERFACE_STDIO;

  const std::string* path = dict.FindString("path");
  // JSON parsed checks that all strings are valid UTF8.
  if (!path || (path_ = base::FilePath::FromUTF8Unsafe(*path)).empty()) {
    *error_message = "Invalid value for path.";
    return false;
  }

  const base::Value::List* allowed_origins_list =
      dict.FindList("allowed_origins");
  if (!allowed_origins_list) {
    *error_message =
        "Invalid value for allowed_origins. Expected a list of strings.";
    return false;
  }
  allowed_origins_.ClearPatterns();
  for (const auto& entry : *allowed_origins_list) {
    if (!entry.is_string()) {
      *error_message = "allowed_origins must be list of strings.";
      return false;
    }
    std::string pattern_string = entry.GetString();
    URLPattern pattern(URLPattern::SCHEME_EXTENSION);
    URLPattern::ParseResult result = pattern.Parse(pattern_string);
    if (result != URLPattern::ParseResult::kSuccess) {
      *error_message = "Failed to parse pattern \"" + pattern_string +
          "\": " + URLPattern::GetParseResultString(result);
      return false;
    }

    // Disallow patterns that are too broad. Set of allowed origins must be a
    // fixed list of extensions.
    if (pattern.match_all_urls() || pattern.match_subdomains()) {
      *error_message = "Pattern \"" + pattern_string + "\" is not allowed.";
      return false;
    }

    allowed_origins_.AddPattern(pattern);
  }

  if (base::FeatureList::IsEnabled(features::kOnConnectNative)) {
    if (const base::Value* supports_native_initiated_connections =
            dict.Find("supports_native_initiated_connections")) {
      if (!supports_native_initiated_connections->is_bool()) {
        *error_message =
            "supports_native_initiated_connections must be a boolean.";
        return false;
      }
      supports_native_initiated_connections_ =
          supports_native_initiated_connections->GetBool();
    }
  }

  return true;
}

}  // namespace extensions
