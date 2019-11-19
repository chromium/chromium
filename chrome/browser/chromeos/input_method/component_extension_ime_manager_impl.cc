// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/input_method/component_extension_ime_manager_impl.h"

#include <stddef.h>

#include <algorithm>

#include "base/files/file_util.h"
#include "base/json/json_string_value_serializer.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/system/sys_info.h"
#include "base/task/post_task.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/grit/browser_resources.h"
#include "extensions/browser/extension_pref_value_map.h"
#include "extensions/browser/extension_pref_value_map_factory.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_constants.h"
#include "ui/base/ime/chromeos/extension_ime_util.h"
#include "ui/base/resource/resource_bundle.h"

namespace chromeos {

namespace {

struct WhitelistedComponentExtensionIME {
  const char* id;
  int manifest_resource_id;
} whitelisted_component_extensions[] = {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
    {
        // Official Google XKB Input.
        extension_ime_util::kXkbExtensionId,
        IDR_GOOGLE_XKB_MANIFEST,
    },
#else
    {
        // Open-sourced ChromeOS xkb extension.
        extension_ime_util::kXkbExtensionId,
        IDR_XKB_MANIFEST,
    },
    {
        // Open-sourced ChromeOS Keyboards extension.
        extension_ime_util::kM17nExtensionId,
        IDR_M17N_MANIFEST,
    },
    {
        // Open-sourced Pinyin Chinese Input Method.
        extension_ime_util::kChinesePinyinExtensionId,
        IDR_PINYIN_MANIFEST,
    },
    {
        // Open-sourced Zhuyin Chinese Input Method.
        extension_ime_util::kChineseZhuyinExtensionId,
        IDR_ZHUYIN_MANIFEST,
    },
    {
        // Open-sourced Cangjie Chinese Input Method.
        extension_ime_util::kChineseCangjieExtensionId,
        IDR_CANGJIE_MANIFEST,
    },
    {
        // Open-sourced Japanese Mozc Input.
        extension_ime_util::kMozcExtensionId,
        IDR_MOZC_MANIFEST,
    },
    {
        // Open-sourced Hangul Input.
        extension_ime_util::kHangulExtensionId,
        IDR_HANGUL_MANIFEST,
    },
#endif
    {
        // Braille hardware keyboard IME that works together with ChromeVox.
        extension_ime_util::kBrailleImeExtensionId,
        IDR_BRAILLE_MANIFEST,
    },
};

const char kImePathKeyName[] = "ime_path";

extensions::ComponentLoader* GetComponentLoader(Profile* profile) {
  extensions::ExtensionSystem* extension_system =
      extensions::ExtensionSystem::Get(profile);
  extensions::ExtensionService* extension_service =
      extension_system->extension_service();
  return extension_service->component_loader();
}

void DoLoadExtension(Profile* profile,
                     const std::string& extension_id,
                     const std::string& manifest,
                     const base::FilePath& file_path) {
  extensions::ExtensionRegistry* extension_registry =
      extensions::ExtensionRegistry::Get(profile);
  DCHECK(extension_registry);
  if (extension_registry->GetExtensionById(
          extension_id, extensions::ExtensionRegistry::ENABLED)) {
    VLOG(1) << "the IME extension(id=\"" << extension_id
            << "\") is already enabled";
    return;
  }
  const std::string loaded_extension_id =
      GetComponentLoader(profile)->Add(manifest, file_path);
  if (loaded_extension_id.empty()) {
    LOG(ERROR) << "Failed to add an IME extension(id=\"" << extension_id
               << ", path=\"" << file_path.LossyDisplayName()
               << "\") to ComponentLoader";
    return;
  }
  // Register IME extension with ExtensionPrefValueMap.
  ExtensionPrefValueMapFactory::GetForBrowserContext(profile)
      ->RegisterExtension(extension_id,
                          base::Time(),  // install_time.
                          true,          // is_enabled.
                          true);         // is_incognito_enabled.
  DCHECK_EQ(loaded_extension_id, extension_id);
  extensions::ExtensionSystem* extension_system =
      extensions::ExtensionSystem::Get(profile);
  extensions::ExtensionService* extension_service =
      extension_system->extension_service();
  DCHECK(extension_service);
  if (!extension_service->IsExtensionEnabled(loaded_extension_id)) {
    LOG(ERROR) << "An IME extension(id=\"" << loaded_extension_id
               << "\") is not enabled after loading";
  }
}

bool CheckFilePath(const base::FilePath* file_path) {
  return base::PathExists(*file_path);
}

void OnFilePathChecked(Profile* profile,
                       const std::string* extension_id,
                       const std::string* manifest,
                       const base::FilePath* file_path,
                       bool result) {
  if (result) {
    DoLoadExtension(profile, *extension_id, *manifest, *file_path);
  } else {
    LOG_IF(ERROR, base::SysInfo::IsRunningOnChromeOS())
        << "IME extension file path does not exist: " << file_path->value();
  }
}

}  // namespace

ComponentExtensionIMEManagerImpl::ComponentExtensionIMEManagerImpl() {
  ReadComponentExtensionsInfo(&component_extension_list_);
}

ComponentExtensionIMEManagerImpl::~ComponentExtensionIMEManagerImpl() = default;

std::vector<ComponentExtensionIME> ComponentExtensionIMEManagerImpl::ListIME() {
  return component_extension_list_;
}

void ComponentExtensionIMEManagerImpl::Load(Profile* profile,
                                            const std::string& extension_id,
                                            const std::string& manifest,
                                            const base::FilePath& file_path) {
  // Check the existence of file path to avoid unnecessary extension loading
  // and InputMethodEngine creation, so that the virtual keyboard web content
  // url won't be override by IME component extensions.
  auto* copied_file_path = new base::FilePath(file_path);
  base::PostTaskAndReplyWithResult(
      // USER_BLOCKING because it is on the critical path of displaying the
      // virtual keyboard. See https://crbug.com/976542
      FROM_HERE,
      {base::ThreadPool(), base::MayBlock(), base::TaskPriority::USER_BLOCKING},
      base::Bind(&CheckFilePath, base::Unretained(copied_file_path)),
      base::Bind(&OnFilePathChecked, base::Unretained(profile),
                 base::Owned(new std::string(extension_id)),
                 base::Owned(new std::string(manifest)),
                 base::Owned(copied_file_path)));
}

void ComponentExtensionIMEManagerImpl::Unload(Profile* profile,
                                              const std::string& extension_id,
                                              const base::FilePath& file_path) {
  // Remove(extension_id) does nothing when the extension has already been
  // removed or not been registered.
  GetComponentLoader(profile)->Remove(extension_id);
}

std::unique_ptr<base::DictionaryValue>
ComponentExtensionIMEManagerImpl::GetManifest(
    const std::string& manifest_string) {
  std::string error;
  JSONStringValueDeserializer deserializer(manifest_string);
  std::unique_ptr<base::Value> manifest =
      deserializer.Deserialize(NULL, &error);
  if (!manifest.get())
    LOG(ERROR) << "Failed at getting manifest";

  return std::unique_ptr<base::DictionaryValue>(
      static_cast<base::DictionaryValue*>(manifest.release()));
}

// static
bool ComponentExtensionIMEManagerImpl::IsIMEExtensionID(const std::string& id) {
  for (auto& extension : whitelisted_component_extensions) {
    if (base::LowerCaseEqualsASCII(id, extension.id))
      return true;
  }
  return false;
}

// static
bool ComponentExtensionIMEManagerImpl::ReadEngineComponent(
    const ComponentExtensionIME& component_extension,
    const base::DictionaryValue& dict,
    ComponentExtensionEngine* out) {
  DCHECK(out);
  std::string type;
  if (!dict.GetString(extensions::manifest_keys::kType, &type))
    return false;
  if (type != "ime")
    return false;
  if (!dict.GetString(extensions::manifest_keys::kId, &out->engine_id))
    return false;
  if (!dict.GetString(extensions::manifest_keys::kName, &out->display_name))
    return false;
  if (!dict.GetString(extensions::manifest_keys::kIndicator, &out->indicator))
    out->indicator = "";

  std::set<std::string> languages;
  const base::Value* language_value = NULL;
  if (dict.Get(extensions::manifest_keys::kLanguage, &language_value)) {
    if (language_value->is_string()) {
      std::string language_str;
      language_value->GetAsString(&language_str);
      languages.insert(language_str);
    } else if (language_value->is_list()) {
      const base::ListValue* language_list = NULL;
      language_value->GetAsList(&language_list);
      for (size_t j = 0; j < language_list->GetSize(); ++j) {
        std::string language_str;
        if (language_list->GetString(j, &language_str))
          languages.insert(language_str);
      }
    }
  }
  DCHECK(!languages.empty());
  out->language_codes.assign(languages.begin(), languages.end());

  const base::ListValue* layouts = NULL;
  if (!dict.GetList(extensions::manifest_keys::kLayouts, &layouts))
    return false;

  for (size_t i = 0; i < layouts->GetSize(); ++i) {
    std::string buffer;
    if (layouts->GetString(i, &buffer))
      out->layouts.push_back(buffer);
  }

  std::string url_string;
  if (dict.GetString(extensions::manifest_keys::kInputView,
                     &url_string)) {
    GURL url = extensions::Extension::GetResourceURL(
        extensions::Extension::GetBaseURLFromExtensionId(
            component_extension.id),
        url_string);
    if (!url.is_valid())
      return false;
    out->input_view_url = url;
  }

  if (dict.GetString(extensions::manifest_keys::kOptionsPage,
                     &url_string)) {
    GURL url = extensions::Extension::GetResourceURL(
        extensions::Extension::GetBaseURLFromExtensionId(
            component_extension.id),
        url_string);
    if (!url.is_valid())
      return false;
    out->options_page_url = url;
  } else {
    // Fallback to extension level options page.
    out->options_page_url = component_extension.options_page_url;
  }

  return true;
}

// static
bool ComponentExtensionIMEManagerImpl::ReadExtensionInfo(
    const base::DictionaryValue& manifest,
    const std::string& extension_id,
    ComponentExtensionIME* out) {
  if (!manifest.GetString(extensions::manifest_keys::kDescription,
                          &out->description))
    return false;
  std::string path;
  if (manifest.GetString(kImePathKeyName, &path))
    out->path = base::FilePath(path);
  std::string url_string;
  if (manifest.GetString(extensions::manifest_keys::kOptionsPage,
                         &url_string)) {
    GURL url = extensions::Extension::GetResourceURL(
        extensions::Extension::GetBaseURLFromExtensionId(extension_id),
        url_string);
    if (!url.is_valid())
      return false;
    out->options_page_url = url;
  }
  // It's okay to return true on no option page and/or input view page case.
  return true;
}

// static
void ComponentExtensionIMEManagerImpl::ReadComponentExtensionsInfo(
    std::vector<ComponentExtensionIME>* out_imes) {
  DCHECK(out_imes);
  for (auto& extension : whitelisted_component_extensions) {
    ComponentExtensionIME component_ime;
    ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
    component_ime.manifest =
        rb.GetRawDataResource(extension.manifest_resource_id).as_string();

    if (component_ime.manifest.empty()) {
      LOG(ERROR) << "Couldn't get manifest from resource_id("
                 << extension.manifest_resource_id << ")";
      continue;
    }

    std::unique_ptr<base::DictionaryValue> manifest =
        GetManifest(component_ime.manifest);
    if (!manifest.get()) {
      LOG(ERROR) << "Failed to load invalid manifest: "
                 << component_ime.manifest;
      continue;
    }

    if (!ReadExtensionInfo(*manifest.get(), extension.id, &component_ime)) {
      LOG(ERROR) << "manifest doesn't have needed information for IME.";
      continue;
    }

    component_ime.id = extension.id;

    if (!component_ime.path.IsAbsolute()) {
      base::FilePath resources_path;
      if (!base::PathService::Get(chrome::DIR_RESOURCES, &resources_path))
        NOTREACHED();
      component_ime.path = resources_path.Append(component_ime.path);
    }

    const base::ListValue* component_list;
    if (!manifest->GetList(extensions::manifest_keys::kInputComponents,
                           &component_list)) {
      LOG(ERROR) << "No input_components is found in manifest.";
      continue;
    }

    for (size_t i = 0; i < component_list->GetSize(); ++i) {
      const base::DictionaryValue* dictionary;
      if (!component_list->GetDictionary(i, &dictionary))
        continue;

      ComponentExtensionEngine engine;
      ReadEngineComponent(component_ime, *dictionary, &engine);
      component_ime.engines.push_back(engine);
    }
    out_imes->push_back(component_ime);
  }
}

}  // namespace chromeos
