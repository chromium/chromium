// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/chrome_component_extension_resource_manager.h"

#include "base/logging.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "build/build_config.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/grit/chrome_unscaled_resources.h"
#include "chrome/grit/component_extension_resources_map.h"
#include "chrome/grit/theme_resources.h"

#if defined(OS_CHROMEOS)
#include "ui/file_manager/file_manager_resource_util.h"
#include "ui/file_manager/grit/file_manager_resources.h"
#include "ui/keyboard/keyboard_resource_util.h"
#endif

namespace extensions {

ChromeComponentExtensionResourceManager::
ChromeComponentExtensionResourceManager() {
  static const GritResourceMap kExtraComponentExtensionResources[] = {
#if defined(OS_CHROMEOS)
    {"web_store/webstore_icon_128.png", IDR_WEBSTORE_APP_ICON_128},
    {"web_store/webstore_icon_16.png", IDR_WEBSTORE_APP_ICON_16},
#else
    {"web_store/webstore_icon_128.png", IDR_WEBSTORE_ICON},
    {"web_store/webstore_icon_16.png", IDR_WEBSTORE_ICON_16},
#endif

#if defined(OS_CHROMEOS)
    {"chrome_app/chrome_app_icon_32.png", IDR_CHROME_APP_ICON_32},
    {"chrome_app/chrome_app_icon_192.png", IDR_CHROME_APP_ICON_192},
#endif
  };

  AddComponentResourceEntries(
      kComponentExtensionResources,
      kComponentExtensionResourcesSize);
  AddComponentResourceEntries(
      kExtraComponentExtensionResources,
      arraysize(kExtraComponentExtensionResources));
#if defined(OS_CHROMEOS)
  size_t file_manager_resource_size;
  const GritResourceMap* file_manager_resources =
      file_manager::GetFileManagerResources(&file_manager_resource_size);
  AddComponentResourceEntries(
      file_manager_resources,
      file_manager_resource_size);

  size_t keyboard_resource_size;
  const GritResourceMap* keyboard_resources =
      keyboard::GetKeyboardExtensionResources(&keyboard_resource_size);
  AddComponentResourceEntries(
      keyboard_resources,
      keyboard_resource_size);
#endif
}

ChromeComponentExtensionResourceManager::
~ChromeComponentExtensionResourceManager() {}

bool ChromeComponentExtensionResourceManager::IsComponentExtensionResource(
    const base::FilePath& extension_path,
    const base::FilePath& resource_path,
    int* resource_id) const {
  base::FilePath directory_path = extension_path;
  base::FilePath resources_dir;
  base::FilePath relative_path;
  if (!base::PathService::Get(chrome::DIR_RESOURCES, &resources_dir) ||
      !resources_dir.AppendRelativePath(directory_path, &relative_path)) {
    return false;
  }
  relative_path = relative_path.Append(resource_path);
  relative_path = relative_path.NormalizePathSeparators();

  auto entry = path_to_resource_id_.find(relative_path);
  if (entry != path_to_resource_id_.end())
    *resource_id = entry->second;

  return entry != path_to_resource_id_.end();
}

void ChromeComponentExtensionResourceManager::AddComponentResourceEntries(
    const GritResourceMap* entries,
    size_t size) {
  for (size_t i = 0; i < size; ++i) {
    base::FilePath resource_path = base::FilePath().AppendASCII(
        entries[i].name);
    resource_path = resource_path.NormalizePathSeparators();

    DCHECK(path_to_resource_id_.find(resource_path) ==
        path_to_resource_id_.end());
    path_to_resource_id_[resource_path] = entries[i].value;
  }
}

}  // namespace extensions
