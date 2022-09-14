// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/component_extension_ime_manager_delegate_impl.h"

#include <stddef.h>

#include <algorithm>

#include "ash/constants/ash_features.h"
#include "base/feature_list.h"
#include "base/files/file_util.h"
#include "base/json/json_string_value_serializer.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/system/sys_info.h"
#include "base/task/thread_pool.h"
#include "base/trace_event/trace_event.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chromeos/ime/input_methods.h"
#include "extensions/browser/extension_pref_value_map.h"
#include "extensions/browser/extension_pref_value_map_factory.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_constants.h"
#include "ui/base/ime/ash/extension_ime_util.h"
#include "ui/base/resource/resource_bundle.h"

namespace ash {
namespace input_method {

namespace {

struct AllowlistedComponentExtensionIME {
  const char* id;
  int manifest_resource_id;
} allowlisted_component_extensions[] = {
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
  TRACE_EVENT1("ime", "DoLoadExtension", "ext_id", extension_id);
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

ComponentExtensionIMEManagerDelegateImpl::
    ComponentExtensionIMEManagerDelegateImpl() {
  ReadComponentExtensionsInfo(&component_extension_list_);
  login_layout_set_.insert(
      std::begin(chromeos::input_method::kLoginXkbLayoutIds),
      std::end(chromeos::input_method::kLoginXkbLayoutIds));
}

ComponentExtensionIMEManagerDelegateImpl::
    ~ComponentExtensionIMEManagerDelegateImpl() = default;

std::vector<ComponentExtensionIME>
ComponentExtensionIMEManagerDelegateImpl::ListIME() {
  return component_extension_list_;
}

void ComponentExtensionIMEManagerDelegateImpl::Load(
    Profile* profile,
    const std::string& extension_id,
    const std::string& manifest,
    const base::FilePath& file_path) {
  TRACE_EVENT0("ime", "ComponentExtensionIMEManagerDelegateImpl::Load");
  std::string* manifest_cp = new std::string(manifest);
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  // Skip checking the path of the Chrome OS IME component extension when it's
  // Google Chrome brand, since it is bundled resource on Chrome OS image. This
  // will improve the IME extension load latency a lot.
  // See http://b/192032670 for more details.
  if (extension_id == extension_ime_util::kXkbExtensionId) {
    DoLoadExtension(profile, extension_id, *manifest_cp, file_path);
    return;
  }
#endif
  // Check the existence of file path to avoid unnecessary extension loading
  // and InputMethodEngine creation, so that the virtual keyboard web content
  // url won't be override by IME component extensions.
  auto* copied_file_path = new base::FilePath(file_path);
  base::ThreadPool::PostTaskAndReplyWithResult(
      // USER_BLOCKING because it is on the critical path of displaying the
      // virtual keyboard. See https://crbug.com/976542
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_BLOCKING},
      base::BindOnce(&CheckFilePath, base::Unretained(copied_file_path)),
      base::BindOnce(&OnFilePathChecked, base::Unretained(profile),
                     base::Owned(new std::string(extension_id)),
                     base::Owned(manifest_cp), base::Owned(copied_file_path)));
}

bool ComponentExtensionIMEManagerDelegateImpl::IsInLoginLayoutAllowlist(
    const std::string& layout) {
  return login_layout_set_.find(layout) != login_layout_set_.end();
}

std::unique_ptr<base::DictionaryValue>
ComponentExtensionIMEManagerDelegateImpl::GetManifest(
    const std::string& manifest_string) {
  std::string error;
  JSONStringValueDeserializer deserializer(manifest_string);
  std::unique_ptr<base::Value> manifest =
      deserializer.Deserialize(nullptr, &error);
  if (!manifest.get())
    LOG(ERROR) << "Failed at getting manifest";

  std::unique_ptr<base::DictionaryValue> ret(
      static_cast<base::DictionaryValue*>(manifest.release()));
  return ret;
}

// static
bool ComponentExtensionIMEManagerDelegateImpl::IsIMEExtensionID(
    const std::string& id) {
  for (auto& extension : allowlisted_component_extensions) {
    if (base::EqualsCaseInsensitiveASCII(id, extension.id))
      return true;
  }
  return false;
}

// static
bool ComponentExtensionIMEManagerDelegateImpl::ReadEngineComponent(
    const ComponentExtensionIME& component_extension,
    const base::DictionaryValue& dict,
    ComponentExtensionEngine* out) {
  DCHECK(out);
  const std::string* engine_id =
      dict.FindStringKey(extensions::manifest_keys::kId);
  if (!engine_id)
    return false;
  out->engine_id = *engine_id;

  const std::string* display_name =
      dict.FindStringKey(extensions::manifest_keys::kName);
  if (!display_name)
    return false;
  out->display_name = *display_name;

  const std::string* indicator =
      dict.FindStringKey(extensions::manifest_keys::kIndicator);
  out->indicator = indicator ? *indicator : "";

  std::set<std::string> languages;
  const base::Value* language_value =
      dict.FindKey(extensions::manifest_keys::kLanguage);
  if (language_value) {
    if (language_value->is_string()) {
      languages.insert(language_value->GetString());
    } else if (language_value->is_list()) {
      for (const base::Value& elem : language_value->GetListDeprecated()) {
        if (elem.is_string())
          languages.insert(elem.GetString());
      }
    }
  }
  DCHECK(!languages.empty());
  out->language_codes.assign(languages.begin(), languages.end());

  // For legacy reasons, multiple physical keyboard XKB layouts can be specified
  // in the IME extension manifest for each input method. However, CrOS only
  // supports one layout per input method. Thus use the "first" layout if
  // specified, else default to "us". CrOS IME extension manifests should
  // specify one and only one layout per input method to avoid confusion.
  const base::ListValue* layouts = nullptr;
  if (!dict.GetList(extensions::manifest_keys::kLayouts, &layouts))
    return false;

  base::Value::ConstListView layouts_list = layouts->GetListDeprecated();
  if (!layouts_list.empty() && layouts_list[0].is_string())
    out->layout = layouts_list[0].GetString();
  else
    out->layout = "us";

  std::string url_string;
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  // Information is managed on VK extension side so just use a default value
  // here.
  GURL url = extensions::Extension::GetResourceURL(
      extensions::Extension::GetBaseURLFromExtensionId(component_extension.id),
      "inputview.html#id=default");
  if (!url.is_valid())
    return false;
  out->input_view_url = url;
#else
  const std::string* input_view =
      dict.FindStringKey(extensions::manifest_keys::kInputView);
  if (input_view) {
    url_string = *input_view;
    GURL url = extensions::Extension::GetResourceURL(
        extensions::Extension::GetBaseURLFromExtensionId(
            component_extension.id),
        url_string);
    if (!url.is_valid())
      return false;
    out->input_view_url = url;
  }
#endif

  const std::string* option_page =
      dict.FindStringKey(extensions::manifest_keys::kOptionsPage);
  if (option_page) {
    url_string = *option_page;
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
bool ComponentExtensionIMEManagerDelegateImpl::ReadExtensionInfo(
    const base::DictionaryValue& manifest,
    const std::string& extension_id,
    ComponentExtensionIME* out) {
  const std::string* description =
      manifest.FindStringKey(extensions::manifest_keys::kDescription);
  if (!description)
    return false;
  out->description = *description;

  const std::string* path = manifest.FindStringKey(kImePathKeyName);
  if (path)
    out->path = base::FilePath(*path);
  const std::string* url_string =
      manifest.FindStringKey(extensions::manifest_keys::kOptionsPage);
  if (url_string) {
    GURL url = extensions::Extension::GetResourceURL(
        extensions::Extension::GetBaseURLFromExtensionId(extension_id),
        *url_string);
    if (!url.is_valid())
      return false;
    out->options_page_url = url;
  }
  // It's okay to return true on no option page and/or input view page case.
  return true;
}

// static
void ComponentExtensionIMEManagerDelegateImpl::ReadComponentExtensionsInfo(
    std::vector<ComponentExtensionIME>* out_imes) {
  DCHECK(out_imes);
  for (auto& extension : allowlisted_component_extensions) {
    ComponentExtensionIME component_ime;
    ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
    component_ime.manifest =
        std::string(rb.GetRawDataResource(extension.manifest_resource_id));

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

    for (const base::Value& value : component_list->GetListDeprecated()) {
      if (!value.is_dict())
        continue;

      const base::DictionaryValue& dictionary =
          base::Value::AsDictionaryValue(value);
      ComponentExtensionEngine engine;
      ReadEngineComponent(component_ime, dictionary, &engine);

      if (base::StartsWith(engine.engine_id, "experimental_",
                           base::CompareCase::SENSITIVE) &&
          !base::FeatureList::IsEnabled(features::kMultilingualTyping)) {
        continue;
      }

      if (engine.engine_id == "vkd_hi_inscript" &&
          !base::FeatureList::IsEnabled(features::kHindiInscriptLayout)) {
        continue;
      }

      component_ime.engines.push_back(engine);
    }
    out_imes->push_back(component_ime);
  }
}

}  // namespace input_method
}  // namespace ash
