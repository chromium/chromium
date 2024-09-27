// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/component_extension_ime_manager_delegate_impl.h"

#include <stddef.h>

#include <algorithm>
#include <optional>
#include <string_view>

#include "ash/constants/ash_features.h"
#include "base/feature_list.h"
#include "base/files/file_util.h"
#include "base/json/json_string_value_serializer.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/system/sys_info.h"
#include "base/task/thread_pool.h"
#include "base/trace_event/trace_event.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/ime/input_methods.h"
#include "extensions/browser/extension_pref_value_map.h"
#include "extensions/browser/extension_pref_value_map_factory.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_constants.h"
#include "net/base/url_util.h"
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
        // Official Google ChromeOS 1P Input.
        extension_ime_util::kXkbExtensionId,
        IDR_GOOGLE_XKB_MANIFEST,
    },
#else
    {
        // Open-sourced ChromiumOS xkb extension.
        extension_ime_util::kXkbExtensionId,
        IDR_XKB_MANIFEST,
    },
    {
        // Open-sourced ChromiumOS Keyboards extension.
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
  if (extension_registry->enabled_extensions().GetByID(extension_id)) {
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

std::optional<base::Value::Dict>
ComponentExtensionIMEManagerDelegateImpl::ParseManifest(
    std::string_view manifest_string) {
  base::JSONReader::Result result =
      base::JSONReader::ReadAndReturnValueWithError(manifest_string);
  if (!result.has_value()) {
    LOG(ERROR) << "Failed to parse manifest: " << result.error().message
               << " at line " << result.error().line << " column "
               << result.error().column;
    return std::nullopt;
  }
  if (!result.value().is_dict()) {
    LOG(ERROR) << "Failed to parse manifest: parsed value is not a dictionary";
    return std::nullopt;
  }
  return std::make_optional(std::move(result.value()).TakeDict());
}

// static
bool ComponentExtensionIMEManagerDelegateImpl::IsIMEExtensionID(
    const std::string& id) {
  for (auto& extension : allowlisted_component_extensions) {
    if (base::EqualsCaseInsensitiveASCII(id, extension.id)) {
      return true;
    }
  }
  return false;
}

// static
bool ComponentExtensionIMEManagerDelegateImpl::ReadEngineComponent(
    const ComponentExtensionIME& component_extension,
    const base::Value::Dict& dict,
    ComponentExtensionEngine* out) {
  DCHECK(out);
  const std::string* engine_id =
      dict.FindString(extensions::manifest_keys::kId);
  if (!engine_id) {
    return false;
  }
  out->engine_id = *engine_id;

  const std::string* display_name =
      dict.FindString(extensions::manifest_keys::kName);
  if (!display_name) {
    return false;
  }
  out->display_name = *display_name;

  const std::string* indicator =
      dict.FindString(extensions::manifest_keys::kIndicator);
  out->indicator = indicator ? *indicator : "";

  std::set<std::string> languages;
  const base::Value* language_value =
      dict.Find(extensions::manifest_keys::kLanguage);
  if (language_value) {
    if (language_value->is_string()) {
      languages.insert(language_value->GetString());
    } else if (language_value->is_list()) {
      for (const base::Value& elem : language_value->GetList()) {
        if (elem.is_string()) {
          languages.insert(elem.GetString());
        }
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
  const base::Value::List* layouts =
      dict.FindList(extensions::manifest_keys::kLayouts);
  if (!layouts) {
    return false;
  }

  if (*engine_id == "ko-t-i0-und" &&
      base::FeatureList::IsEnabled(
          features::kImeKoreanOnlyModeSwitchOnRightAlt)) {
    out->layout = "kr(cros)";
  } else if (!layouts->empty() && layouts->front().is_string()) {
    out->layout = layouts->front().GetString();
  } else {
    out->layout = "us";
  }

  std::string url_string;
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  bool is_global_emoji_preferences_enabled = base::FeatureList::IsEnabled(
      features::kVirtualKeyboardGlobalEmojiPreferences);
  GURL url = extensions::Extension::GetResourceURL(
      extensions::Extension::GetBaseURLFromExtensionId(component_extension.id),
      "inputview.html");
  url = net::AppendOrReplaceQueryParameter(url, "jelly", "true");
  url = net::AppendOrReplaceQueryParameter(
      url, "globalemojipreferences",
      is_global_emoji_preferences_enabled ? "true" : "false");
  // Information is managed on VK extension side so just use a default value
  // here.
  url = net::AppendOrReplaceRef(url, "id=default");
  if (!url.is_valid()) {
    return false;
  }
  out->input_view_url = url;
#else
  const std::string* input_view =
      dict.FindString(extensions::manifest_keys::kInputView);
  if (input_view) {
    url_string = *input_view;
    GURL url = extensions::Extension::GetResourceURL(
        extensions::Extension::GetBaseURLFromExtensionId(
            component_extension.id),
        url_string);
    if (!url.is_valid()) {
      return false;
    }
    out->input_view_url = url;
  }
#endif

  const std::string* option_page =
      dict.FindString(extensions::manifest_keys::kOptionsPage);

  bool flag_allows_settings_page =
      (*engine_id != "vkd_vi_vni" && *engine_id != "vkd_vi_telex") ||
      base::FeatureList::IsEnabled(features::kFirstPartyVietnameseInput);

  if (option_page && flag_allows_settings_page) {
    url_string = *option_page;
    GURL options_page_url = extensions::Extension::GetResourceURL(
        extensions::Extension::GetBaseURLFromExtensionId(
            component_extension.id),
        url_string);
    if (!options_page_url.is_valid()) {
      return false;
    }
    out->options_page_url = options_page_url;
  } else {
    // Fallback to extension level options page.
    out->options_page_url = component_extension.options_page_url;
  }

  const std::string* handwriting_language =
      dict.FindString(extensions::manifest_keys::kHandwritingLanguage);

  if (handwriting_language != nullptr) {
    out->handwriting_language = *handwriting_language;
  } else {
    out->handwriting_language = std::nullopt;
  }

  return true;
}

// static
bool ComponentExtensionIMEManagerDelegateImpl::ReadExtensionInfo(
    const base::Value::Dict& manifest,
    const std::string& extension_id,
    ComponentExtensionIME* out) {
  const std::string* description =
      manifest.FindString(extensions::manifest_keys::kDescription);
  if (!description) {
    return false;
  }
  out->description = *description;

  const std::string* path = manifest.FindString(kImePathKeyName);
  if (path) {
    out->path = base::FilePath(*path);
  }
  const std::string* url_string =
      manifest.FindString(extensions::manifest_keys::kOptionsPage);
  if (url_string) {
    GURL url = extensions::Extension::GetResourceURL(
        extensions::Extension::GetBaseURLFromExtensionId(extension_id),
        *url_string);
    if (!url.is_valid()) {
      return false;
    }
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
    component_ime.manifest =
        ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
            extension.manifest_resource_id);
    if (component_ime.manifest.empty()) {
      LOG(ERROR) << "Couldn't get manifest from resource_id("
                 << extension.manifest_resource_id << ")";
      continue;
    }

    std::optional<base::Value::Dict> maybe_manifest =
        ParseManifest(component_ime.manifest);
    if (!maybe_manifest.has_value()) {
      LOG(ERROR) << "Failed to load invalid manifest: "
                 << component_ime.manifest;
      continue;
    }
    const base::Value::Dict& manifest = maybe_manifest.value();

    if (!ReadExtensionInfo(manifest, extension.id, &component_ime)) {
      LOG(ERROR) << "manifest doesn't have needed information for IME.";
      continue;
    }

    component_ime.id = extension.id;

    if (!component_ime.path.IsAbsolute()) {
      base::FilePath resources_path;
      if (!base::PathService::Get(chrome::DIR_RESOURCES, &resources_path)) {
        NOTREACHED_IN_MIGRATION();
      }
      component_ime.path = resources_path.Append(component_ime.path);
    }

    const base::Value::List* component_list =
        manifest.FindList(extensions::manifest_keys::kInputComponents);
    if (!component_list) {
      LOG(ERROR) << "No input_components is found in manifest.";
      continue;
    }

    for (const base::Value& value : *component_list) {
      if (!value.is_dict()) {
        continue;
      }

      const base::Value::Dict& dictionary = value.GetDict();
      ComponentExtensionEngine engine;
      ReadEngineComponent(component_ime, dictionary, &engine);

      const char* kHindiInscriptEngineId = "vkd_hi_inscript";
      if (engine.engine_id == kHindiInscriptEngineId &&
          !base::FeatureList::IsEnabled(features::kHindiInscriptLayout)) {
        bool policy_value = false;
        CrosSettings::Get()->GetBoolean(kDeviceHindiInscriptLayoutEnabled,
                                        &policy_value);
        if (!policy_value) {
          continue;
        }
      }

      component_ime.engines.push_back(engine);
    }
    out_imes->push_back(component_ime);
  }
}

}  // namespace input_method
}  // namespace ash
