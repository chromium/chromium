// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/extensions/chrome_component_extension_resource_manager.h"

#include <map>
#include <string>

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/grit/chrome_unscaled_resources.h"
#include "chrome/grit/component_extension_resources_map.h"
#include "chrome/grit/theme_resources.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_id.h"
#include "pdf/buildflags.h"
#include "ppapi/buildflags/buildflags.h"
#include "ui/base/resource/resource_bundle.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/keyboard/ui/resources/keyboard_resource_util.h"
#include "ash/webui/file_manager/untrusted_resources/grit/file_manager_untrusted_resources_map.h"
#include "base/command_line.h"
#include "chrome/browser/ash/file_manager/file_manager_string_util.h"
#include "chrome/browser/browser_process.h"
#include "ui/file_manager/grit/file_manager_gen_resources_map.h"
#include "ui/file_manager/grit/file_manager_resources_map.h"

#if BUILDFLAG(ENABLE_INK)
#include "chromeos/grit/chromeos_media_app_bundle_resources.h"
#endif  // BUILDFLAG(ENABLE_INK)

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(ENABLE_PDF)
#include <utility>
#include "chrome/browser/pdf/pdf_extension_util.h"
#include "chrome/grit/pdf_resources_map.h"
#endif  // BUILDFLAG(ENABLE_PDF)

namespace extensions {

class ChromeComponentExtensionResourceManager::Data {
 public:
  using TemplateReplacementMap =
      std::map<ExtensionId, ui::TemplateReplacements>;

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
  void AddComponentResourceEntries(const webui::ResourcePath* entries,
                                   size_t size);

  // A map from a resource path to the resource ID. Used by
  // ChromeComponentExtensionResourceManager::IsComponentExtensionResource().
  std::map<base::FilePath, int> path_to_resource_id_;

  // A map from an extension ID to its i18n template replacements.
  TemplateReplacementMap template_replacements_;
};

ChromeComponentExtensionResourceManager::Data::Data() {
  static const webui::ResourcePath kExtraComponentExtensionResources[] = {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"web_store/webstore_icon_128.png", IDR_WEBSTORE_APP_ICON_128},
    {"web_store/webstore_icon_16.png", IDR_WEBSTORE_APP_ICON_16},
#else
    {"web_store/webstore_icon_128.png", IDR_WEBSTORE_ICON},
    {"web_store/webstore_icon_16.png", IDR_WEBSTORE_ICON_16},
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
    // These icons may be replaced with "IDR_DEBUG_CHROME_APP_ICON_{32,192}"
    // in "chrome/browser/apps/app_service/app_icon/app_icon_reader.cc"
    // or "chrome/browser/ui/views/frame/browser_view.cc"
    {"chrome_app/chrome_app_icon_32.png", IDR_CHROME_APP_ICON_32},
    {"chrome_app/chrome_app_icon_192.png", IDR_CHROME_APP_ICON_192},
#if BUILDFLAG(ENABLE_INK)
    // Built in go/bbsrc/lib/BUILD
    {"pdf/ink/ink_engine_ink.worker.js",
     IDR_MEDIA_APP_INK_ENGINE_INK_WORKER_JS},
    {"pdf/ink/ink_engine_ink.wasm", IDR_MEDIA_APP_INK_ENGINE_INK_WASM},
    {"pdf/ink/ink_lib_binary.js", IDR_MEDIA_APP_EXPORT_CANVAS_BIN_JS},
    {"pdf/ink/ink_loader.js", IDR_MEDIA_APP_INK_JS},
#endif  // BUILDFLAG(ENABLE_INK)
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  };

  AddComponentResourceEntries(kComponentExtensionResources,
                              kComponentExtensionResourcesSize);
  AddComponentResourceEntries(kExtraComponentExtensionResources,
                              std::size(kExtraComponentExtensionResources));

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Add Files app JS modules resources.
  AddComponentResourceEntries(kFileManagerResources, kFileManagerResourcesSize);
  AddComponentResourceEntries(kFileManagerGenResources,
                              kFileManagerGenResourcesSize);

  // Add Files app resources to display untrusted content in <webview> frames.
  // Files app extension's resource paths need to be prefixed by
  // "file_manager/".
  for (size_t i = 0; i < kFileManagerUntrustedResourcesSize; ++i) {
    base::FilePath resource_path =
        base::FilePath("file_manager")
            .AppendASCII(kFileManagerUntrustedResources[i].path);
    resource_path = resource_path.NormalizePathSeparators();

    DCHECK(!base::Contains(path_to_resource_id_, resource_path));
    path_to_resource_id_[resource_path] = kFileManagerUntrustedResources[i].id;
  }

  // ResourceBundle and g_browser_process are not always initialized in unit
  // tests.
  if (ui::ResourceBundle::HasSharedInstance() && g_browser_process) {
    ui::TemplateReplacements file_manager_replacements;
    ui::TemplateReplacementsFromDictionaryValue(GetFileManagerStrings(),
                                                &file_manager_replacements);
    template_replacements_[extension_misc::kFilesManagerAppId] =
        std::move(file_manager_replacements);
  }

  size_t keyboard_resource_size;
  const webui::ResourcePath* keyboard_resources =
      keyboard::GetKeyboardExtensionResources(&keyboard_resource_size);
  AddComponentResourceEntries(keyboard_resources, keyboard_resource_size);
#endif

#if BUILDFLAG(ENABLE_PDF)
  AddComponentResourceEntries(kPdfResources, kPdfResourcesSize);

  // ResourceBundle is not always initialized in unit tests.
  if (ui::ResourceBundle::HasSharedInstance()) {
    base::Value::Dict dict;
    pdf_extension_util::AddStrings(
        pdf_extension_util::PdfViewerContext::kPdfViewer, &dict);

    ui::TemplateReplacements pdf_viewer_replacements;
    ui::TemplateReplacementsFromDictionaryValue(dict, &pdf_viewer_replacements);
    template_replacements_[extension_misc::kPdfExtensionId] =
        std::move(pdf_viewer_replacements);
  }
#endif
}

void ChromeComponentExtensionResourceManager::Data::AddComponentResourceEntries(
    const webui::ResourcePath* entries,
    size_t size) {
  for (size_t i = 0; i < size; ++i) {
    base::FilePath resource_path =
        base::FilePath().AppendASCII(entries[i].path);
    resource_path = resource_path.NormalizePathSeparators();

    DCHECK(!base::Contains(path_to_resource_id_, resource_path));
    path_to_resource_id_[resource_path] = entries[i].id;
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
    const ExtensionId& extension_id) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  LazyInitData();

#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (extension_id == extension_misc::kFilesManagerAppId) {
    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    // Disable $i18n{} template JS string replacement during JS code coverage.
    base::FilePath devtools_code_coverage_dir_ =
        command_line->GetSwitchValuePath("devtools-code-coverage");
    if (!devtools_code_coverage_dir_.empty())
      return nullptr;
  }
#endif

  auto it = data_->template_replacements().find(extension_id);
  return it != data_->template_replacements().end() ? &it->second : nullptr;
}

void ChromeComponentExtensionResourceManager::LazyInitData() const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!data_)
    data_ = std::make_unique<Data>();
}

}  // namespace extensions
