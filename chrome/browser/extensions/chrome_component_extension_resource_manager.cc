// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/chrome_component_extension_resource_manager.h"

#include <map>
#include <string>

#include "base/check.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/stl_util.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/grit/chrome_unscaled_resources.h"
#include "chrome/grit/component_extension_resources_map.h"
#include "chrome/grit/theme_resources.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/common/constants.h"
#include "pdf/buildflags.h"
#include "ui/base/resource/resource_bundle.h"

#if defined(OS_CHROMEOS)
#include "ash/keyboard/ui/resources/keyboard_resource_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/file_manager/file_manager_string_util.h"
#include "third_party/ink/grit/ink_resources.h"
#include "ui/file_manager/file_manager_resource_util.h"
#include "ui/file_manager/grit/file_manager_resources.h"
#endif

#if BUILDFLAG(ENABLE_PDF)
#include <utility>
#include "chrome/browser/pdf/pdf_extension_util.h"
#endif

namespace extensions {

class ChromeComponentExtensionResourceManager::Data {
 public:
  using TemplateReplacementMap =
      std::map<std::string, ui::TemplateReplacements>;

  Data();
  Data(const Data&) = delete;
  Data& operator=(const Data&) = delete;
  ~Data() = default;

  const std::map<base::FilePath, int>& path_to_resource_id() const {
    return path_to_resource_id_;
  }

  const TemplateReplacementMap& template_replacements() const {
    return template_replacements_;
  }

 private:
  void AddComponentResourceEntries(const GritResourceMap* entries, size_t size);

  // A map from a resource path to the resource ID. Used by
  // ChromeComponentExtensionResourceManager::IsComponentExtensionResource().
  std::map<base::FilePath, int> path_to_resource_id_;

  // A map from an extension ID to its i18n template replacements.
  TemplateReplacementMap template_replacements_;
};

ChromeComponentExtensionResourceManager::Data::Data() {
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
    {"pdf/ink/wasm_ink.worker.js", IDR_INK_WORKER_JS},
    {"pdf/ink/wasm_ink.wasm", IDR_INK_WASM},
    {"pdf/ink/ink_loader.js", IDR_INK_LOADER_JS},
#endif
  };

  AddComponentResourceEntries(kComponentExtensionResources,
                              kComponentExtensionResourcesSize);
  AddComponentResourceEntries(kExtraComponentExtensionResources,
                              base::size(kExtraComponentExtensionResources));
#if defined(OS_CHROMEOS)
  size_t file_manager_resource_size;
  const GritResourceMap* file_manager_resources =
      file_manager::GetFileManagerResources(&file_manager_resource_size);
  AddComponentResourceEntries(file_manager_resources,
                              file_manager_resource_size);

  // ResourceBundle and g_browser_process are not always initialized in unit
  // tests.
  if (ui::ResourceBundle::HasSharedInstance() && g_browser_process) {
    ui::TemplateReplacements file_manager_replacements;
    ui::TemplateReplacementsFromDictionaryValue(*GetFileManagerStrings(),
                                                &file_manager_replacements);
    template_replacements_[extension_misc::kFilesManagerAppId] =
        std::move(file_manager_replacements);
  }

  size_t keyboard_resource_size;
  const GritResourceMap* keyboard_resources =
      keyboard::GetKeyboardExtensionResources(&keyboard_resource_size);
  AddComponentResourceEntries(keyboard_resources, keyboard_resource_size);
#endif

#if BUILDFLAG(ENABLE_PDF)
  // ResourceBundle is not always initialized in unit tests.
  if (ui::ResourceBundle::HasSharedInstance()) {
    base::Value dict(base::Value::Type::DICTIONARY);
    pdf_extension_util::AddStrings(
        pdf_extension_util::PdfViewerContext::kPdfViewer, &dict);
    pdf_extension_util::AddAdditionalData(&dict);

    ui::TemplateReplacements pdf_viewer_replacements;
    ui::TemplateReplacementsFromDictionaryValue(
        base::Value::AsDictionaryValue(dict), &pdf_viewer_replacements);
    template_replacements_[extension_misc::kPdfExtensionId] =
        std::move(pdf_viewer_replacements);
  }
#endif
}

void ChromeComponentExtensionResourceManager::Data::AddComponentResourceEntries(
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

ChromeComponentExtensionResourceManager::
    ChromeComponentExtensionResourceManager() = default;

ChromeComponentExtensionResourceManager::
    ~ChromeComponentExtensionResourceManager() = default;

bool ChromeComponentExtensionResourceManager::IsComponentExtensionResource(
    const base::FilePath& extension_path,
    const base::FilePath& resource_path,
    int* resource_id) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  base::FilePath directory_path = extension_path;
  base::FilePath resources_dir;
  base::FilePath relative_path;
  if (!base::PathService::Get(chrome::DIR_RESOURCES, &resources_dir) ||
      !resources_dir.AppendRelativePath(directory_path, &relative_path)) {
    return false;
  }
  relative_path = relative_path.Append(resource_path);
  relative_path = relative_path.NormalizePathSeparators();

  LazyInitData();
  auto entry = data_->path_to_resource_id().find(relative_path);
  if (entry == data_->path_to_resource_id().end())
    return false;

  *resource_id = entry->second;
  return true;
}

const ui::TemplateReplacements*
ChromeComponentExtensionResourceManager::GetTemplateReplacementsForExtension(
    const std::string& extension_id) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  LazyInitData();
  auto it = data_->template_replacements().find(extension_id);
  return it != data_->template_replacements().end() ? &it->second : nullptr;
}

void ChromeComponentExtensionResourceManager::LazyInitData() const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!data_)
    data_ = std::make_unique<Data>();
}

}  // namespace extensions
