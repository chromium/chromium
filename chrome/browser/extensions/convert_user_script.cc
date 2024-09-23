// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/convert_user_script.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/json/json_file_value_serializer.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/common/chrome_paths.h"
#include "crypto/sha2.h"
#include "extensions/browser/extension_user_script_loader.h"
#include "extensions/common/api/content_scripts.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/file_util.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/mojom/run_location.mojom-shared.h"
#include "extensions/common/user_script.h"
#include "extensions/common/utils/extension_types_utils.h"
#include "url/gurl.h"

namespace extensions {

scoped_refptr<Extension> ConvertUserScriptToExtension(
    const base::FilePath& user_script_path,
    const GURL& original_url,
    const base::FilePath& extensions_dir,
    std::u16string* error) {
  using ContentScript = api::content_scripts::ContentScript;

  std::string content;
  if (!base::ReadFileToString(user_script_path, &content)) {
    *error = u"Could not read source file.";
    return nullptr;
  }

  if (!base::IsStringUTF8(content)) {
    *error = u"User script must be UTF8 encoded.";
    return nullptr;
  }

  UserScript script;
  if (!UserScriptLoader::ParseMetadataHeader(content, &script)) {
    *error = u"Invalid script header.";
    return nullptr;
  }

  base::FilePath install_temp_dir =
      file_util::GetInstallTempDir(extensions_dir);
  if (install_temp_dir.empty()) {
    *error = u"Could not get path to profile temporary directory.";
    return nullptr;
  }

  base::ScopedTempDir temp_dir;
  if (!temp_dir.CreateUniqueTempDirUnderPath(install_temp_dir)) {
    *error = u"Could not create temporary directory.";
    return nullptr;
  }

  // Create the manifest
  base::Value::Dict root;
  std::string script_name;
  if (!script.name().empty() && !script.name_space().empty())
    script_name = script.name_space() + "/" + script.name();
  else
    script_name = original_url.spec();

  // Create the public key.
  // User scripts are not signed, but the public key for an extension doubles as
  // its unique identity, and we need one of those. A user script's unique
  // identity is its namespace+name, so we hash that to create a public key.
  // There will be no corresponding private key, which means user scripts cannot
  // be auto-updated, or claimed in the gallery.
  uint8_t raw[crypto::kSHA256Length] = {0};
  crypto::SHA256HashString(script_name, raw, crypto::kSHA256Length);
  std::string key = base::Base64Encode(raw);

  // The script may not have a name field, but we need one for an extension. If
  // it is missing, use the filename of the original URL.
  if (!script.name().empty())
    root.Set(manifest_keys::kName, script.name());
  else
    root.Set(manifest_keys::kName, original_url.ExtractFileName());

  // Not all scripts have a version, but we need one. Default to 1.0 if it is
  // missing.
  if (!script.version().empty())
    root.Set(manifest_keys::kVersion, script.version());
  else
    root.Set(manifest_keys::kVersion, "1.0");

  root.Set(manifest_keys::kDescription, script.description());
  root.Set(manifest_keys::kPublicKey, key);
  root.Set(manifest_keys::kConvertedFromUserScript, true);

  // If the script provides its own match patterns, we use those. Otherwise, we
  // generate some using the include globs.
  std::vector<std::string> matches;
  if (!script.url_patterns().is_empty()) {
    matches.reserve(script.url_patterns().size());
    for (const URLPattern& pattern : script.url_patterns())
      matches.push_back(pattern.GetAsString());
  } else {
    // TODO(aa): Derive tighter matches where possible.
    matches.push_back("http://*/*");
    matches.push_back("https://*/*");
  }

  // Read the exclude matches, if any are present.
  std::vector<std::string> exclude_matches;
  exclude_matches.reserve(script.exclude_url_patterns().size());
  for (const URLPattern& pattern : script.exclude_url_patterns())
    exclude_matches.push_back(pattern.GetAsString());

  ContentScript content_script;
  content_script.matches = std::move(matches);
  content_script.exclude_matches = std::move(exclude_matches);
  content_script.include_globs = script.globs();
  content_script.exclude_globs = script.exclude_globs();

  content_script.js.emplace();
  content_script.js->push_back("script.js");

  content_script.run_at = ConvertRunLocationForAPI(script.run_location());

  base::Value::List content_scripts;
  content_scripts.Append(content_script.ToValue());
  root.Set(api::content_scripts::ManifestKeys::kContentScripts,
           std::move(content_scripts));

  base::FilePath manifest_path = temp_dir.GetPath().Append(kManifestFilename);
  JSONFileValueSerializer serializer(manifest_path);
  if (!serializer.Serialize(root)) {
    *error = u"Could not write JSON.";
    return nullptr;
  }

  // Write the script file.
  if (!base::CopyFile(user_script_path,
                      temp_dir.GetPath().AppendASCII("script.js"))) {
    *error = u"Could not copy script file.";
    return nullptr;
  }

  // TODO(rdevlin.cronin): Continue removing std::string errors and replacing
  // with std::u16string
  std::string utf8_error;
  scoped_refptr<Extension> extension =
      Extension::Create(temp_dir.GetPath(), mojom::ManifestLocation::kInternal,
                        root, Extension::NO_FLAGS, &utf8_error);
  *error = base::UTF8ToUTF16(utf8_error);
  if (!extension.get()) {
    NOTREACHED_IN_MIGRATION() << "Could not init extension " << *error;
    return nullptr;
  }

  temp_dir.Take();  // The caller takes ownership of the directory.
  return extension;
}

}  // namespace extensions
