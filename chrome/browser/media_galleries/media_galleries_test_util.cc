// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media_galleries/media_galleries_test_util.h"

#include <tuple>
#include <utility>

#include "base/files/file_path.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "components/crx_file/id_util.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registrar.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/manifest_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

scoped_refptr<extensions::Extension> AddMediaGalleriesApp(
    const std::string& name,
    const std::vector<std::string>& media_galleries_permissions,
    Profile* profile) {
  base::DictValue manifest;
  manifest.Set(extensions::manifest_keys::kName, name);
  manifest.Set(extensions::manifest_keys::kVersion, "0.1");
  manifest.Set(extensions::manifest_keys::kManifestVersion, 2);
  base::ListValue background_script_list;
  background_script_list.Append("background.js");
  manifest.SetByDottedPath(
      extensions::manifest_keys::kPlatformAppBackgroundScripts,
      std::move(background_script_list));

  base::ListValue permission_detail_list;
  for (const auto& permission : media_galleries_permissions)
    permission_detail_list.Append(permission);
  base::DictValue media_galleries_permission;
  media_galleries_permission.Set("mediaGalleries",
                                 std::move(permission_detail_list));
  base::ListValue permission_list;
  permission_list.Append(std::move(media_galleries_permission));
  manifest.Set(extensions::manifest_keys::kPermissions,
               std::move(permission_list));

  extensions::ExtensionPrefs* extension_prefs =
      extensions::ExtensionPrefs::Get(profile);
  base::FilePath path = extension_prefs->install_directory().AppendASCII(name);
  std::u16string utf16_error;
  scoped_refptr<extensions::Extension> extension =
      extensions::Extension::Create(
          path, extensions::mojom::ManifestLocation::kInternal, manifest,
          extensions::Extension::NO_FLAGS, std::string(), &utf16_error);
  EXPECT_TRUE(extension.get() != nullptr) << utf16_error;
  EXPECT_TRUE(crx_file::id_util::IdIsValid(extension->id()));
  if (!extension.get() || !crx_file::id_util::IdIsValid(extension->id()))
    return nullptr;

  extension_prefs->OnExtensionInstalled(
      extension.get(),
      /*disable_reasons=*/{}, syncer::StringOrdinal::CreateInitialOrdinal(),
      std::string());
  auto* registrar = extensions::ExtensionRegistrar::Get(profile);
  registrar->AddExtension(extension);
  registrar->EnableExtension(extension->id());

  return extension;
}

base::FilePath MakeMediaGalleriesTestingPath(const std::string& dir) {
  return base::FilePath(FILE_PATH_LITERAL("/")).Append(dir);
}
