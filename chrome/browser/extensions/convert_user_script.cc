// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/file_util.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/user_script.h"
#include "url/gurl.h"

namespace extensions {

scoped_refptr<Extension> ConvertUserScriptToExtension(
    const base::FilePath& user_script_path, const GURL& original_url,
    const base::FilePath& extensions_dir, base::string16* error) {
  std::string content;
  if (!base::ReadFileToString(user_script_path, &content)) {
    *error = base::ASCIIToUTF16("Could not read source file.");
    return nullptr;
  }

  if (!base::IsStringUTF8(content)) {
    *error = base::ASCIIToUTF16("User script must be UTF8 encoded.");
    return nullptr;
  }

  UserScript script;
  if (!UserScriptLoader::ParseMetadataHeader(content, &script)) {
    *error = base::ASCIIToUTF16("Invalid script header.");
    return nullptr;
  }

  base::FilePath install_temp_dir =
      file_util::GetInstallTempDir(extensions_dir);
  if (install_temp_dir.empty()) {
    *error = base::ASCIIToUTF16(
        "Could not get path to profile temporary directory.");
    return nullptr;
  }

  base::ScopedTempDir temp_dir;
  if (!temp_dir.CreateUniqueTempDirUnderPath(install_temp_dir)) {
    *error = base::ASCIIToUTF16("Could not create temporary directory.");
    return nullptr;
  }

  // Create the manifest
  std::unique_ptr<base::DictionaryValue> root(new base::DictionaryValue);
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
  char raw[crypto::kSHA256Length] = {0};
  std::string key;
  crypto::SHA256HashString(script_name, raw, crypto::kSHA256Length);
  base::Base64Encode(base::StringPiece(raw, crypto::kSHA256Length), &key);

  // The script may not have a name field, but we need one for an extension. If
  // it is missing, use the filename of the original URL.
  if (!script.name().empty())
    root->SetString(manifest_keys::kName, script.name());
  else
    root->SetString(manifest_keys::kName, original_url.ExtractFileName());

  // Not all scripts have a version, but we need one. Default to 1.0 if it is
  // missing.
  if (!script.version().empty())
    root->SetString(manifest_keys::kVersion, script.version());
  else
    root->SetString(manifest_keys::kVersion, "1.0");

  root->SetString(manifest_keys::kDescription, script.description());
  root->SetString(manifest_keys::kPublicKey, key);
  root->SetBoolean(manifest_keys::kConvertedFromUserScript, true);

  auto js_files = std::make_unique<base::ListValue>();
  js_files->AppendString("script.js");

  // If the script provides its own match patterns, we use those. Otherwise, we
  // generate some using the include globs.
  auto matches = std::make_unique<base::ListValue>();
  if (!script.url_patterns().is_empty()) {
    for (auto i = script.url_patterns().begin();
         i != script.url_patterns().end(); ++i) {
      matches->AppendString(i->GetAsString());
    }
  } else {
    // TODO(aa): Derive tighter matches where possible.
    matches->AppendString("http://*/*");
    matches->AppendString("https://*/*");
  }

  // Read the exclude matches, if any are present.
  auto exclude_matches = std::make_unique<base::ListValue>();
  if (!script.exclude_url_patterns().is_empty()) {
    for (auto i = script.exclude_url_patterns().begin();
         i != script.exclude_url_patterns().end(); ++i) {
      exclude_matches->AppendString(i->GetAsString());
    }
  }

  auto includes = std::make_unique<base::ListValue>();
  for (size_t i = 0; i < script.globs().size(); ++i)
    includes->AppendString(script.globs().at(i));

  auto excludes = std::make_unique<base::ListValue>();
  for (size_t i = 0; i < script.exclude_globs().size(); ++i)
    excludes->AppendString(script.exclude_globs().at(i));

  auto content_script = std::make_unique<base::DictionaryValue>();
  content_script->Set(manifest_keys::kMatches, std::move(matches));
  content_script->Set(manifest_keys::kExcludeMatches,
                      std::move(exclude_matches));
  content_script->Set(manifest_keys::kIncludeGlobs, std::move(includes));
  content_script->Set(manifest_keys::kExcludeGlobs, std::move(excludes));
  content_script->Set(manifest_keys::kJs, std::move(js_files));

  if (script.run_location() == UserScript::DOCUMENT_START)
    content_script->SetString(manifest_keys::kRunAt,
                              manifest_values::kRunAtDocumentStart);
  else if (script.run_location() == UserScript::DOCUMENT_END)
    content_script->SetString(manifest_keys::kRunAt,
                              manifest_values::kRunAtDocumentEnd);
  else if (script.run_location() == UserScript::DOCUMENT_IDLE)
    // This is the default, but store it just in case we change that.
    content_script->SetString(manifest_keys::kRunAt,
                              manifest_values::kRunAtDocumentIdle);

  auto content_scripts = std::make_unique<base::ListValue>();
  content_scripts->Append(std::move(content_script));

  root->Set(manifest_keys::kContentScripts, std::move(content_scripts));

  base::FilePath manifest_path = temp_dir.GetPath().Append(kManifestFilename);
  JSONFileValueSerializer serializer(manifest_path);
  if (!serializer.Serialize(*root)) {
    *error = base::ASCIIToUTF16("Could not write JSON.");
    return nullptr;
  }

  // Write the script file.
  if (!base::CopyFile(user_script_path,
                      temp_dir.GetPath().AppendASCII("script.js"))) {
    *error = base::ASCIIToUTF16("Could not copy script file.");
    return nullptr;
  }

  // TODO(rdevlin.cronin): Continue removing std::string errors and replacing
  // with base::string16
  std::string utf8_error;
  scoped_refptr<Extension> extension =
      Extension::Create(temp_dir.GetPath(), Manifest::INTERNAL, *root,
                        Extension::NO_FLAGS, &utf8_error);
  *error = base::UTF8ToUTF16(utf8_error);
  if (!extension.get()) {
    NOTREACHED() << "Could not init extension " << *error;
    return nullptr;
  }

  temp_dir.Take();  // The caller takes ownership of the directory.
  return extension;
}

}  // namespace extensions
