// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_browser_test_util.h"

#include "base/files/file_util.h"
#include "base/json/json_file_value_serializer.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/api/web_accessible_resources.h"
#include "extensions/common/api/web_accessible_resources_mv2.h"
#include "extensions/common/constants.h"
#include "extensions/common/file_util.h"
#include "extensions/common/manifest_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace extensions::browser_test_util {
namespace {

// Creates a copy of `source` within `temp_dir` and populates `out` with the
// destination path. Returns true on success.
bool CreateTempDirectoryCopy(const base::FilePath& temp_dir,
                             const base::FilePath& source,
                             base::FilePath* out) {
  base::ScopedAllowBlockingForTesting allow_blocking;

  base::FilePath temp_subdir;
  if (!base::CreateTemporaryDirInDir(temp_dir, base::FilePath::StringType(),
                                     &temp_subdir)) {
    ADD_FAILURE() << "Could not create temporary dir for test under "
                  << temp_dir;
    return false;
  }

  // Copy all files from `source` to `temp_subdir`.
  if (!base::CopyDirectory(source, temp_subdir, true /* recursive */)) {
    ADD_FAILURE() << source.value() << " could not be copied to "
                  << temp_subdir.value();
    return false;
  }

  *out = temp_subdir.Append(source.BaseName());
  return true;
}

// Moves match patterns from the `permissions_key` list to the
// `host_permissions_key` list.
void DoMoveHostPermissions(base::Value::Dict& manifest_dict,
                           const char* permissions_key,
                           const char* host_permissions_key) {
  base::Value::List* const permissions =
      manifest_dict.FindList(permissions_key);
  if (!permissions) {
    return;
  }

  // Add the permissions to the appropriate destinations, then update/add
  // the target lists as appropriate.
  base::Value::List permissions_list;
  base::Value::List host_permissions_list;
  for (auto& value : *permissions) {
    CHECK(value.is_string());
    const std::string& str_value = value.GetString();
    if (str_value == "<all_urls>" ||
        str_value.find("://") != std::string::npos) {
      host_permissions_list.Append(std::move(value));
    } else {
      permissions_list.Append(std::move(value));
    }
  }

  if (permissions_list.empty()) {
    manifest_dict.Remove(permissions_key);
  } else {
    *permissions = std::move(permissions_list);
  }

  if (!host_permissions_list.empty()) {
    manifest_dict.Set(host_permissions_key, std::move(host_permissions_list));
  }
}

// Moves match patterns from permissions/optional_permissions to the
// host_permissions/optional_host_permissions.
void MoveHostPermissions(base::Value::Dict& manifest_dict) {
  DoMoveHostPermissions(manifest_dict, manifest_keys::kPermissions,
                        manifest_keys::kHostPermissions);
  DoMoveHostPermissions(manifest_dict, manifest_keys::kOptionalPermissions,
                        manifest_keys::kOptionalHostPermissions);
}

using web_accessible_resource =
    api::web_accessible_resources::WebAccessibleResource;

// Upgrades MV2 format to MV3 format.
void UpgradeWebAccessibleResources(base::Value::Dict& manifest_dict) {
  base::Value::List* const web_accessible_resources = manifest_dict.FindList(
      api::web_accessible_resources::ManifestKeys::kWebAccessibleResources);
  if (!web_accessible_resources) {
    return;
  }

  // Copy all of the entries to a single dictionary entry that matches all
  // URLs.
  auto war_dict = base::Value::Dict()
                      .Set(web_accessible_resource::kResources,
                           web_accessible_resources->Clone())
                      .Set(web_accessible_resource::kMatches,
                           base::Value::List().Append("<all_urls>"));

  // Clear the list and append the dictionary.
  web_accessible_resources->clear();
  web_accessible_resources->Append(std::move(war_dict));
}

// Modifies `manifest_dict` changing its manifest version to 3.
bool ModifyManifestForManifestVersion3(base::Value::Dict& manifest_dict) {
  // This should only be used for manifest v2 extension.
  std::optional<int> current_manifest_version =
      manifest_dict.FindInt(manifest_keys::kManifestVersion);
  if (!current_manifest_version || *current_manifest_version != 2) {
    ADD_FAILURE() << manifest_dict << " should have a manifest version of 2.";
    return false;
  }

  UpgradeWebAccessibleResources(manifest_dict);
  MoveHostPermissions(manifest_dict);

  manifest_dict.Set(manifest_keys::kManifestVersion, 3);

  return true;
}

// Modifies extension at `extension_root` and its `manifest_dict` converting it
// to a service worker based extension.
// NOTE: The conversion works only for extensions with background.scripts and
// requires the background.persistent key. The background.page key is not
// supported.
bool ModifyExtensionForServiceWorker(const base::FilePath& extension_root,
                                     base::Value::Dict& manifest_dict) {
  base::ScopedAllowBlockingForTesting allow_blocking;

  // Retrieve the value of the `background` key and verify that it has
  // the `persistent` key and specifies JS files.
  // Background pages that specify HTML files are not supported.
  base::Value::Dict* background_dict = manifest_dict.FindDict("background");
  if (!background_dict) {
    ADD_FAILURE() << extension_root.value()
                  << " 'background' key not found in manifest.json";
    return false;
  }
  {
    std::optional<bool> background_persistent =
        background_dict->FindBool("persistent");
    if (!background_persistent.has_value()) {
      ADD_FAILURE() << extension_root.value()
                    << ": The \"persistent\" key must be specified to run as a "
                       "Service Worker-based extension.";
      return false;
    }
  }
  base::Value::List* background_scripts_list =
      background_dict->FindList("scripts");
  // Number of JS scripts must be >= 1.
  if (!background_scripts_list || background_scripts_list->empty()) {
    ADD_FAILURE() << extension_root.value()
                  << ": Only extensions with JS script(s) can be loaded "
                     "as a sw-based extension.";
    return false;
  }

  std::string service_worker_script;

  // If there's just one script, use it directly in the "service_worker" key.
  if (background_scripts_list->size() == 1) {
    service_worker_script = (*background_scripts_list)[0].GetString();
  } else {
    // Otherwise, generate a script that uses importScripts() to import
    // all of the scripts.
    service_worker_script = "generated_service_worker__.js";
    std::vector<std::string> script_filenames;
    for (const base::Value& script : *background_scripts_list) {
      script_filenames.push_back(base::StrCat({"'", script.GetString(), "'"}));
    }

    base::FilePath combined_script_filepath =
        extension_root.AppendASCII(service_worker_script);
    // Collision with generated script filename.
    if (base::PathExists(combined_script_filepath)) {
      ADD_FAILURE() << combined_script_filepath.value()
                    << " already exists, make sure " << extension_root.value()
                    << " does not contained file named "
                    << service_worker_script;
      return false;
    }
    std::string generated_sw_script_content = base::StringPrintf(
        "importScripts(%s);", base::JoinString(script_filenames, ",").c_str());
    if (!base::WriteFile(combined_script_filepath,
                         generated_sw_script_content)) {
      ADD_FAILURE() << "Could not write combined Service Worker script to: "
                    << combined_script_filepath.value();
      return false;
    }
  }

  // Remove the existing background specification and replace it with a service
  // worker.
  background_dict->Remove("persistent");
  background_dict->Remove("scripts");
  background_dict->Set("service_worker", std::move(service_worker_script));

  return true;
}

}  // namespace

bool IsServiceWorkerContext(ContextType context_type) {
  return context_type == ContextType::kServiceWorker ||
         context_type == ContextType::kServiceWorkerMV2;
}

bool ModifyExtensionIfNeeded(const LoadOptions& options,
                             ContextType context_type,
                             size_t test_pre_count,
                             const base::FilePath& temp_dir_path,
                             const base::FilePath& input_path,
                             base::FilePath* out_path) {
  base::ScopedAllowBlockingForTesting scoped_allow_blocking;

  const ContextType context_type_to_use =
      options.context_type == ContextType::kNone ? context_type
                                                 : options.context_type;

  // Use context_type_ if LoadOptions.context_type is unspecified.
  // Otherwise, use LoadOptions.context_type.
  const bool load_as_service_worker =
      IsServiceWorkerContext(context_type_to_use);

  // Early return if no modification is needed.
  if (!load_as_service_worker && !options.load_as_manifest_version_3) {
    *out_path = input_path;
    return true;
  }

  // Tests that have a PRE_ stage need to exist in a temporary directory that
  // persists after the test fixture is destroyed. The test bots are configured
  // to use a unique temp directory that's cleaned up after the tests run, so
  // this won't pollute the system tmp directory.
  base::FilePath temp_dir;
  if (test_pre_count == 0) {
    temp_dir = temp_dir_path;
  } else if (!base::GetTempDir(&temp_dir)) {
    ADD_FAILURE() << "Could not get temporary dir for test.";
    return false;
  }

  base::FilePath extension_root;
  if (!CreateTempDirectoryCopy(temp_dir, input_path, &extension_root)) {
    return false;
  }

  std::string error;
  std::optional<base::Value::Dict> manifest_dict =
      extensions::file_util::LoadManifest(extension_root, &error);
  if (!manifest_dict) {
    ADD_FAILURE() << extension_root.value()
                  << " could not load manifest: " << error;
    return false;
  }

  if (load_as_service_worker &&
      !ModifyExtensionForServiceWorker(extension_root, *manifest_dict)) {
    return false;
  }

  const bool is_service_worker_mv3 =
      context_type_to_use == ContextType::kServiceWorker;

  // Update the manifest if converting to a service worker MV3-based
  // extension or if requested in `options`.
  if ((is_service_worker_mv3 || options.load_as_manifest_version_3) &&
      !ModifyManifestForManifestVersion3(*manifest_dict)) {
    return false;
  }

  // Write out manifest.json.
  base::FilePath manifest_path = extension_root.Append(kManifestFilename);
  if (!JSONFileValueSerializer(manifest_path).Serialize(*manifest_dict)) {
    ADD_FAILURE() << "Could not write manifest file to "
                  << manifest_path.value();
    return false;
  }

  *out_path = extension_root;
  return true;
}

}  // namespace extensions::browser_test_util
