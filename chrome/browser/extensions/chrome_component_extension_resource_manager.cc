// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/chrome_component_extension_resource_manager.h"

#include "base/logging.h"
#include "base/path_service.h"
#include "base/stl_util.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/grit/chrome_unscaled_resources.h"
#include "chrome/grit/component_extension_resources_map.h"
#include "chrome/grit/theme_resources.h"

#if defined(OS_CHROMEOS)
#include "ash/keyboard/ui/resources/keyboard_resource_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/file_manager/file_manager_string_util.h"
#include "extensions/common/constants.h"
#include "third_party/ink/grit/ink_resources.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/file_manager/file_manager_resource_util.h"
#include "ui/file_manager/grit/file_manager_resources.h"
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
    {"pdf/ink/ink_lib_binary.js", IDR_INK_LIB_BINARY_JS},
    {"pdf/ink/pthread-main.js", IDR_INK_PTHREAD_MAIN_JS},
    {"pdf/ink/glcore_base.js.mem", IDR_INK_GLCORE_BASE_JS_MEM},
    {"pdf/ink/glcore_base.wasm", IDR_INK_GLCORE_BASE_WASM},
    {"pdf/ink/glcore_wasm_bootstrap_compiled.js",
     IDR_INK_GLCORE_WASM_BOOTSTRAP_COMPILED_JS},
#endif
  };

  AddComponentResourceEntries(
      kComponentExtensionResources,
      kComponentExtensionResourcesSize);
  AddComponentResourceEntries(kExtraComponentExtensionResources,
                              base::size(kExtraComponentExtensionResources));
#if defined(OS_CHROMEOS)
  size_t file_manager_resource_size;
  const GritResourceMap* file_manager_resources =
      file_manager::GetFileManagerResources(&file_manager_resource_size);
  AddComponentResourceEntries(
      file_manager_resources,
      file_manager_resource_size);

  // ResourceBundle and g_browser_process are not always initialized in unit
  // tests.
  if (ui::ResourceBundle::HasSharedInstance() && g_browser_process) {
    ui::TemplateReplacements file_manager_replacements;
    ui::TemplateReplacementsFromDictionaryValue(*GetFileManagerStrings(),
                                                &file_manager_replacements);
    extension_template_replacements_[extension_misc::kFilesManagerAppId] =
        std::move(file_manager_replacements);
  }

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
  if (entry != path_to_resource_id_.end()) {
    *resource_id = entry->second;
    return true;
  }

  return false;
}

const ui::TemplateReplacements*
ChromeComponentExtensionResourceManager::GetTemplateReplacementsForExtension(
    const std::string& extension_id) const {
  auto it = extension_template_replacements_.find(extension_id);
  if (it == extension_template_replacements_.end()) {
    return nullptr;
  }
  return &it->second;
}

void ChromeComponentExtensionResourceManager::AddComponentResourceEntries(
    const GritResourceMap* entries,
    size_t size) {
  base::FilePath gen_folder_path = base::FilePath().AppendASCII(
      "@out_folder@/gen/chrome/browser/resources/");
  gen_folder_path = gen_folder_path.NormalizePathSeparators();

  for (size_t i = 0; i < size; ++i) {
    base::FilePath resource_path = base::FilePath().AppendASCII(
        entries[i].name);
    resource_path = resource_path.NormalizePathSeparators();

    if (!gen_folder_path.IsParent(resource_path)) {
      DCHECK(!base::Contains(path_to_resource_id_, resource_path));
      path_to_resource_id_[resource_path] = entries[i].value;
    } else {
      // If the resource is a generated file, strip the generated folder's path,
      // so that it can be served from a normal URL (as if it were not
      // generated).
      base::FilePath effective_path =
          base::FilePath().AppendASCII(resource_path.AsUTF8Unsafe().substr(
              gen_folder_path.value().length()));
      DCHECK(!base::Contains(path_to_resource_id_, effective_path));
      path_to_resource_id_[effective_path] = entries[i].value;
    }
  }
}

}  // namespace extensions
