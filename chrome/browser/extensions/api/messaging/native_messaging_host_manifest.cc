// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/messaging/native_messaging_host_manifest.h"

#include <stddef.h>

#include "base/json/json_file_value_serializer.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/values.h"
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
      deserializer.Deserialize(NULL, error_message);
  if (!parsed) {
    return std::unique_ptr<NativeMessagingHostManifest>();
  }

  base::DictionaryValue* dictionary;
  if (!parsed->GetAsDictionary(&dictionary)) {
    *error_message = "Invalid manifest file.";
    return std::unique_ptr<NativeMessagingHostManifest>();
  }

  std::unique_ptr<NativeMessagingHostManifest> result(
      new NativeMessagingHostManifest());
  if (!result->Parse(dictionary, error_message)) {
    return std::unique_ptr<NativeMessagingHostManifest>();
  }

  return result;
}

NativeMessagingHostManifest::NativeMessagingHostManifest() {
}

bool NativeMessagingHostManifest::Parse(base::DictionaryValue* dictionary,
                                        std::string* error_message) {
  if (!dictionary->GetString("name", &name_) ||
      !IsValidName(name_)) {
    *error_message = "Invalid value for name.";
    return false;
  }

  if (!dictionary->GetString("description", &description_) ||
      description_.empty()) {
    *error_message = "Invalid value for description.";
    return false;
  }

  std::string type;
  // stdio is the only host type that's currently supported.
  if (!dictionary->GetString("type", &type) ||
      type != "stdio") {
    *error_message = "Invalid value for type.";
    return false;
  }
  interface_ = HOST_INTERFACE_STDIO;

  std::string path;
  // JSON parsed checks that all strings are valid UTF8.
  if (!dictionary->GetString("path", &path) ||
      (path_ = base::FilePath::FromUTF8Unsafe(path)).empty()) {
    *error_message = "Invalid value for path.";
    return false;
  }

  const base::ListValue* allowed_origins_list;
  if (!dictionary->GetList("allowed_origins", &allowed_origins_list)) {
    *error_message =
        "Invalid value for allowed_origins. Expected a list of strings.";
    return false;
  }
  allowed_origins_.ClearPatterns();
  for (auto it = allowed_origins_list->begin();
       it != allowed_origins_list->end(); ++it) {
    std::string pattern_string;
    if (!it->GetAsString(&pattern_string)) {
      *error_message = "allowed_origins must be list of strings.";
      return false;
    }

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
            dictionary->FindKey("supports_native_initiated_connections")) {
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
