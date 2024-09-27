// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/component_loader.h"

#include <optional>
#include <string>
#include <string_view>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_string_value_serializer.h"
#include "base/metrics/histogram_macros.h"
#include "base/path_service.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "base/values.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/component_extensions_allowlist/allowlist.h"
#include "chrome/browser/extensions/data_deleter.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/launch_util.h"
#include "chrome/browser/extensions/profile_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/generated_resources.h"
#include "components/crx_file/id_util.h"
#include "components/nacl/common/buildflags.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_switches.h"
#include "extensions/browser/extension_file_task_runner.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/pref_names.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/extension_l10n_util.h"
#include "extensions/common/file_util.h"
#include "extensions/common/manifest_constants.h"
#include "pdf/buildflags.h"
#include "printing/buildflags/buildflags.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/ash_switches.h"
#include "ash/keyboard/ui/grit/keyboard_resources.h"
#include "base/system/sys_info.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/storage_partition.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/common/switches.h"
#include "storage/browser/file_system/file_system_context.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/file_manager/grit/file_manager_resources.h"
#endif

#if BUILDFLAG(IS_CHROMEOS)
#include "chromeos/constants/chromeos_features.h"
#endif

#if BUILDFLAG(ENABLE_PDF)
#include "chrome/browser/pdf/pdf_extension_util.h"
#endif

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
#include "chrome/browser/defaults.h"
#endif

using content::BrowserThread;

namespace extensions {

namespace {

#if BUILDFLAG(ENABLE_HANGOUT_SERVICES_EXTENSION)
BASE_FEATURE(kHangoutsExtensionV3,
             "HangoutsExtensionV3",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(ENABLE_HANGOUT_SERVICES_EXTENSION)

bool g_enable_background_extensions_during_testing = false;

#if BUILDFLAG(IS_CHROMEOS_ASH) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
// Whether HelpApp is enabled.
bool g_enable_help_app = true;
#endif

ExtensionId GenerateId(const base::Value::Dict& manifest,
                       const base::FilePath& path) {
  std::string id_input;
  const std::string* raw_key = manifest.FindString(manifest_keys::kPublicKey);
  CHECK(raw_key != nullptr);
  CHECK(Extension::ParsePEMKeyBytes(*raw_key, &id_input));
  ExtensionId id = crx_file::id_util::GenerateId(id_input);
  return id;
}

#if BUILDFLAG(IS_CHROMEOS)
std::optional<base::Value::Dict> LoadManifestOnFileThread(
    const base::FilePath& root_directory,
    const base::FilePath::CharType* manifest_filename,
    bool localize_manifest) {
  DCHECK(GetExtensionFileTaskRunner()->RunsTasksInCurrentSequence());
  std::string error;
  std::optional<base::Value::Dict> manifest(
      file_util::LoadManifest(root_directory, manifest_filename, &error));
  if (!manifest) {
    LOG(ERROR) << "Can't load "
               << root_directory.Append(manifest_filename).AsUTF8Unsafe()
               << ": " << error;
    return std::nullopt;
  }

  if (localize_manifest) {
    // This is only called for Chrome OS component extensions which are loaded
    // from a read-only rootfs partition, so it is safe to set
    // |gzip_permission| to kAllowForTrustedSource.
    bool localized = extension_l10n_util::LocalizeExtension(
        root_directory, &manifest.value(),
        extension_l10n_util::GzippedMessagesPermission::kAllowForTrustedSource,
        &error);
    CHECK(localized) << error;
  }

  return manifest;
}
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_CHROMEOS_ASH)
bool IsNormalSession() {
  return !base::CommandLine::ForCurrentProcess()->HasSwitch(
             ash::switches::kGuestSession) &&
         user_manager::UserManager::IsInitialized() &&
         user_manager::UserManager::Get()->IsUserLoggedIn();
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace

ComponentLoader::ComponentExtensionInfo::ComponentExtensionInfo(
    base::Value::Dict manifest_param,
    const base::FilePath& directory)
    : manifest(std::move(manifest_param)), root_directory(directory) {
  if (!root_directory.IsAbsolute()) {
    CHECK(base::PathService::Get(chrome::DIR_RESOURCES, &root_directory));
    root_directory = root_directory.Append(directory);
  }
  extension_id = GenerateId(manifest, root_directory);
}

ComponentLoader::ComponentExtensionInfo::ComponentExtensionInfo(
    ComponentExtensionInfo&& other)
    : manifest(std::move(other.manifest)),
      root_directory(std::move(other.root_directory)),
      extension_id(std::move(other.extension_id)) {}

ComponentLoader::ComponentExtensionInfo&
ComponentLoader::ComponentExtensionInfo::operator=(
    ComponentExtensionInfo&& other) {
  manifest = std::move(other.manifest);
  root_directory = std::move(other.root_directory);
  extension_id = std::move(other.extension_id);
  return *this;
}

ComponentLoader::ComponentExtensionInfo::~ComponentExtensionInfo() = default;

ComponentLoader::ComponentLoader(ExtensionSystem* extension_system,
                                 Profile* profile)
    : profile_(profile),
      extension_system_(extension_system),
      ignore_allowlist_for_testing_(false) {}

ComponentLoader::~ComponentLoader() = default;

void ComponentLoader::LoadAll() {
  TRACE_EVENT0("browser,startup", "ComponentLoader::LoadAll");
  bool is_user_profile =
      profile_util::ProfileCanUseNonComponentExtensions(profile_);
  const base::TimeTicks load_start_time = base::TimeTicks::Now();

  for (const auto& component_extension : component_extensions_) {
    Load(component_extension);
  }

  const base::TimeDelta load_all_component_time =
      base::TimeTicks::Now() - load_start_time;
  UMA_HISTOGRAM_TIMES("Extensions.LoadAllComponentTime",
                      load_all_component_time);
  if (is_user_profile) {
    UMA_HISTOGRAM_TIMES("Extensions.LoadAllComponentTime.User",
                        load_all_component_time);
  } else {
    UMA_HISTOGRAM_TIMES("Extensions.LoadAllComponentTime.NonUser",
                        load_all_component_time);
  }
}

std::optional<base::Value::Dict> ComponentLoader::ParseManifest(
    std::string_view manifest_contents) const {
  JSONStringValueDeserializer deserializer(manifest_contents);
  std::unique_ptr<base::Value> manifest =
      deserializer.Deserialize(nullptr, nullptr);

  if (!manifest.get() || !manifest->is_dict()) {
    LOG(ERROR) << "Failed to parse extension manifest.";
    return std::nullopt;
  }

  return std::move(*manifest).TakeDict();
}

ExtensionId ComponentLoader::Add(int manifest_resource_id,
                                 const base::FilePath& root_directory) {
  if (!ignore_allowlist_for_testing_ &&
      !IsComponentExtensionAllowlisted(manifest_resource_id)) {
    return std::string();
  }

  std::string manifest_contents =
      ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
          manifest_resource_id);
  return Add(manifest_contents, root_directory, true);
}

ExtensionId ComponentLoader::Add(base::Value::Dict manifest,
                                 const base::FilePath& root_directory) {
  return Add(std::move(manifest), root_directory, false);
}

ExtensionId ComponentLoader::Add(std::string_view manifest_contents,
                                 const base::FilePath& root_directory) {
  return Add(manifest_contents, root_directory, false);
}

ExtensionId ComponentLoader::Add(std::string_view manifest_contents,
                                 const base::FilePath& root_directory,
                                 bool skip_allowlist) {
  // The Value is kept for the lifetime of the ComponentLoader. This is
  // required in case LoadAll() is called again.
  std::optional<base::Value::Dict> manifest = ParseManifest(manifest_contents);
  if (manifest) {
    return Add(std::move(*manifest), root_directory, skip_allowlist);
  }
  return std::string();
}

ExtensionId ComponentLoader::Add(base::Value::Dict parsed_manifest,
                                 const base::FilePath& root_directory,
                                 bool skip_allowlist) {
  ComponentExtensionInfo info(std::move(parsed_manifest), root_directory);
  if (!ignore_allowlist_for_testing_ && !skip_allowlist &&
      !IsComponentExtensionAllowlisted(info.extension_id)) {
    return std::string();
  }

  component_extensions_.push_back(std::move(info));
  ComponentExtensionInfo& added_info = component_extensions_.back();
  if (extension_system_->is_ready()) {
    Load(added_info);
  }
  return added_info.extension_id;
}

ExtensionId ComponentLoader::AddOrReplace(const base::FilePath& path) {
  base::FilePath absolute_path = base::MakeAbsoluteFilePath(path);
  std::string error;
  std::optional<base::Value::Dict> manifest(
      file_util::LoadManifest(absolute_path, &error));
  if (!manifest) {
    LOG(ERROR) << "Could not load extension from '" << absolute_path.value()
               << "'. " << error;
    return std::string();
  }
  Remove(GenerateId(*manifest, absolute_path));

  // We don't check component extensions loaded by path because this is only
  // used by developers for testing.
  return Add(std::move(*manifest), absolute_path, true);
}

void ComponentLoader::Reload(const ExtensionId& extension_id) {
  for (const auto& component_extension : component_extensions_) {
    if (component_extension.extension_id == extension_id) {
      Load(component_extension);
      break;
    }
  }
}

void ComponentLoader::Load(const ComponentExtensionInfo& info) {
  std::string error;
  scoped_refptr<const Extension> extension(CreateExtension(info, &error));
  if (!extension.get()) {
    LOG(ERROR) << error;
    return;
  }

  CHECK_EQ(info.extension_id, extension->id()) << extension->name();
  extension_system_->extension_service()->AddComponentExtension(
      extension.get());
}

void ComponentLoader::Remove(const base::FilePath& root_directory) {
  // Find the ComponentExtensionInfo for the extension.
  for (const auto& component_extension : component_extensions_) {
    if (component_extension.root_directory == root_directory) {
      Remove(GenerateId(component_extension.manifest, root_directory));
      break;
    }
  }
}

void ComponentLoader::Remove(const ExtensionId& id) {
  for (auto it = component_extensions_.begin();
       it != component_extensions_.end(); ++it) {
    if (it->extension_id == id) {
      UnloadComponent(&(*it));
      component_extensions_.erase(it);
      break;
    }
  }
}

bool ComponentLoader::Exists(const ExtensionId& id) const {
  for (const auto& component_extension : component_extensions_) {
    if (component_extension.extension_id == id) {
      return true;
    }
  }
  return false;
}

std::vector<ExtensionId> ComponentLoader::GetRegisteredComponentExtensionsIds()
    const {
  std::vector<ExtensionId> result;
  for (const auto& el : component_extensions_) {
    result.push_back(el.extension_id);
  }
  return result;
}

#if BUILDFLAG(ENABLE_HANGOUT_SERVICES_EXTENSION)
void ComponentLoader::AddHangoutServicesExtension() {
  // Finch controlled migration to a v3 Manifest - see crbug.com/326877912.
  if (base::FeatureList::IsEnabled(kHangoutsExtensionV3)) {
    Add(IDR_HANGOUT_SERVICES_MANIFEST_V3,
        base::FilePath(FILE_PATH_LITERAL("hangout_services")));
  } else {
    Add(IDR_HANGOUT_SERVICES_MANIFEST_V2,
        base::FilePath(FILE_PATH_LITERAL("hangout_services")));
  }
}
#endif  // BUILDFLAG(ENABLE_HANGOUT_SERVICES_EXTENSION)

void ComponentLoader::AddNetworkSpeechSynthesisExtension() {
  Add(IDR_NETWORK_SPEECH_SYNTHESIS_MANIFEST,
      base::FilePath(FILE_PATH_LITERAL("network_speech_synthesis")));
}

void ComponentLoader::AddWithNameAndDescription(
    int manifest_resource_id,
    const base::FilePath& root_directory,
    const std::string& name_string,
    const std::string& description_string) {
  if (!ignore_allowlist_for_testing_ &&
      !IsComponentExtensionAllowlisted(manifest_resource_id)) {
    return;
  }

  std::string manifest_contents =
      ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
          manifest_resource_id);

  // The Value is kept for the lifetime of the ComponentLoader. This is
  // required in case LoadAll() is called again.
  std::optional<base::Value::Dict> manifest = ParseManifest(manifest_contents);

  if (manifest) {
    manifest->Set(manifest_keys::kName, name_string);
    manifest->Set(manifest_keys::kDescription, description_string);
    Add(std::move(*manifest), root_directory, true);
  }
}

void ComponentLoader::AddWebStoreApp() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (!IsNormalSession()) {
    return;
  }
#endif

  AddWithNameAndDescription(
      IDR_WEBSTORE_MANIFEST, base::FilePath(FILE_PATH_LITERAL("web_store")),
      l10n_util::GetStringUTF8(IDS_WEBSTORE_NAME_STORE),
      l10n_util::GetStringUTF8(IDS_WEBSTORE_APP_DESCRIPTION));
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
void ComponentLoader::AddChromeApp() {
  AddWithNameAndDescription(
      IDR_CHROME_APP_MANIFEST, base::FilePath(FILE_PATH_LITERAL("chrome_app")),
      crosapi::browser_util::IsAshWebBrowserEnabled()
          ? l10n_util::GetStringUTF8(IDS_SHORT_PRODUCT_NAME)
          : "Ash Chrome",  // Because this is debug only, we do not need i18n.
      l10n_util::GetStringUTF8(IDS_CHROME_SHORTCUT_DESCRIPTION));
}

void ComponentLoader::AddImageLoaderExtension() {
  Add(IDR_IMAGE_LOADER_MANIFEST,
      base::FilePath(FILE_PATH_LITERAL("image_loader")));
}

void ComponentLoader::AddGuestModeTestExtension(const base::FilePath& path) {
  base::SysInfo::CrashIfChromeOSNonTestImage();
  AddComponentFromDirWithManifestFilename(
      path, extension_misc::kGuestModeTestExtensionId,
      extensions::kManifestFilename, extensions::kManifestFilename,
      base::RepeatingClosure());
}

void ComponentLoader::AddKeyboardApp() {
  Add(IDR_KEYBOARD_MANIFEST, base::FilePath(FILE_PATH_LITERAL("keyboard")));
}

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

scoped_refptr<const Extension> ComponentLoader::CreateExtension(
    const ComponentExtensionInfo& info,
    std::string* utf8_error) {
  // TODO(abarth): We should REQUIRE_MODERN_MANIFEST_VERSION once we've updated
  //               our component extensions to the new manifest version.
  int flags = Extension::REQUIRE_KEY;
  return Extension::Create(info.root_directory,
                           mojom::ManifestLocation::kComponent, info.manifest,
                           flags, utf8_error);
}

// static
void ComponentLoader::EnableBackgroundExtensionsForTesting() {
  g_enable_background_extensions_during_testing = true;
}

#if BUILDFLAG(IS_CHROMEOS_ASH) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
// static
void ComponentLoader::DisableHelpAppForTesting() {
  g_enable_help_app = false;
}
#endif

void ComponentLoader::AddDefaultComponentExtensions(
    bool skip_session_components) {
  // Do not add component extensions that have background pages here -- add them
  // to AddDefaultComponentExtensionsWithBackgroundPages.
#if BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  if (g_enable_help_app) {
    Add(IDR_HELP_MANIFEST, base::FilePath(FILE_PATH_LITERAL(
                               "/usr/share/chromeos-assets/helpapp")));
  }
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

  AddKeyboardApp();
#else   // BUILDFLAG(IS_CHROMEOS_ASH)
  DCHECK(!skip_session_components);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  if (!skip_session_components) {
    AddWebStoreApp();
#if BUILDFLAG(IS_CHROMEOS_ASH)
    if (crosapi::browser_util::IsAshWebBrowserEnabled() ||
        ash::switches::IsAshDebugBrowserEnabled()) {
      AddChromeApp();
    }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
#if BUILDFLAG(ENABLE_PDF)
    Add(pdf_extension_util::GetManifest(),
        base::FilePath(FILE_PATH_LITERAL("pdf")));
#endif  // BUILDFLAG(ENABLE_PDF)
  }

  AddDefaultComponentExtensionsWithBackgroundPages(skip_session_components);
}

void ComponentLoader::AddDefaultComponentExtensionsForKioskMode(
    bool skip_session_components) {
  // Do not add component extensions that have background pages here -- add them
  // to AddDefaultComponentExtensionsWithBackgroundPagesForKioskMode.

  // No component extension for kiosk app launch splash screen.
  if (skip_session_components) {
    return;
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Add virtual keyboard.
  AddKeyboardApp();
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  AddDefaultComponentExtensionsWithBackgroundPagesForKioskMode();

#if BUILDFLAG(ENABLE_PDF)
  Add(pdf_extension_util::GetManifest(),
      base::FilePath(FILE_PATH_LITERAL("pdf")));
#endif
}

void ComponentLoader::AddDefaultComponentExtensionsWithBackgroundPages(
    bool skip_session_components) {
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();

  // Component extensions with background pages are not enabled during tests
  // because they generate a lot of background behavior that can interfere.
  const bool should_disable_background_extensions =
      !g_enable_background_extensions_during_testing &&
      (command_line->HasSwitch(::switches::kTestType) ||
       command_line->HasSwitch(
           ::switches::kDisableComponentExtensionsWithBackgroundPages));

#if BUILDFLAG(ENABLE_HANGOUT_SERVICES_EXTENSION)
  const bool enable_hangout_services_extension_for_testing =
      command_line->HasSwitch(::switches::kTestType) &&
      command_line->HasSwitch(
          ::switches::kEnableHangoutServicesExtensionForTesting);
  if (!skip_session_components &&
      (!should_disable_background_extensions ||
       enable_hangout_services_extension_for_testing)) {
    AddHangoutServicesExtension();
  }
#endif  // BUILDFLAG(ENABLE_HANGOUT_SERVICES_EXTENSION)

  if (should_disable_background_extensions) {
    return;
  }

  if (!skip_session_components) {
#if BUILDFLAG(IS_CHROMEOS)
    Add(IDR_ECHO_MANIFEST,
        base::FilePath(FILE_PATH_LITERAL("/usr/share/chromeos-assets/echo")));
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
    if (!base::FeatureList::IsEnabled(
            chromeos::features::kDisableOfficeEditingComponentApp)) {
      AddComponentFromDirWithManifestFilename(
          base::FilePath("/usr/share/chromeos-assets/quickoffice"),
          extension_misc::kQuickOfficeComponentExtensionId,
          extensions::kManifestFilename, extensions::kManifestFilename,
          base::DoNothing());
    }
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_CHROMEOS_ASH)
    if (command_line->HasSwitch(switches::kLoadGuestModeTestExtension)) {
      base::FilePath path = base::FilePath(command_line->GetSwitchValueASCII(
          switches::kLoadGuestModeTestExtension));
      AddGuestModeTestExtension(path);
    }
    AddImageLoaderExtension();

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
    // TODO(https://crbug.com/1005083): Force the off the record profile to be
    // created to allow the virtual keyboard to work in guest mode.
    if (!IsNormalSession()) {
      ExtensionsBrowserClient::Get()->GetOffTheRecordContext(profile_);
    }
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

    Add(IDR_ARC_SUPPORT_MANIFEST,
        base::FilePath(FILE_PATH_LITERAL("chromeos/arc_support")));
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  }

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
#if !BUILDFLAG(IS_CHROMEOS_ASH)  // http://crbug.com/314799
  AddNetworkSpeechSynthesisExtension();
#endif

#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
}

void ComponentLoader::
    AddDefaultComponentExtensionsWithBackgroundPagesForKioskMode() {
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();

  // Component extensions with background pages are not enabled during tests
  // because they generate a lot of background behavior that can interfere.
  if (!g_enable_background_extensions_during_testing &&
      (command_line->HasSwitch(::switches::kTestType) ||
       command_line->HasSwitch(
           ::switches::kDisableComponentExtensionsWithBackgroundPages))) {
    return;
  }

#if BUILDFLAG(ENABLE_HANGOUT_SERVICES_EXTENSION)
  AddHangoutServicesExtension();
#endif  // BUILDFLAG(ENABLE_HANGOUT_SERVICES_EXTENSION)
}

void ComponentLoader::UnloadComponent(ComponentExtensionInfo* component) {
  if (extension_system_->is_ready()) {
    extension_system_->extension_service()->RemoveComponentExtension(
        component->extension_id);
  }
}

#if BUILDFLAG(IS_CHROMEOS)
void ComponentLoader::AddComponentFromDirWithManifestFilename(
    const base::FilePath& root_directory,
    const ExtensionId& extension_id,
    const base::FilePath::CharType* manifest_file_name,
    const base::FilePath::CharType* guest_manifest_file_name,
    base::OnceClosure done_cb) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
#if BUILDFLAG(IS_CHROMEOS_ASH)
  const base::FilePath::CharType* manifest_filename =
      IsNormalSession() ? manifest_file_name : guest_manifest_file_name;
#else
  const base::FilePath::CharType* manifest_filename = manifest_file_name;
#endif
  GetExtensionFileTaskRunner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&LoadManifestOnFileThread, root_directory,
                     manifest_filename, true),
      base::BindOnce(&ComponentLoader::FinishAddComponentFromDir,
                     weak_factory_.GetWeakPtr(), root_directory, extension_id,
                     std::nullopt, std::nullopt, std::move(done_cb)));
}

void ComponentLoader::FinishAddComponentFromDir(
    const base::FilePath& root_directory,
    const ExtensionId& extension_id,
    const std::optional<std::string>& name_string,
    const std::optional<std::string>& description_string,
    base::OnceClosure done_cb,
    std::optional<base::Value::Dict> manifest) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!manifest) {
    return;  // Error already logged.
  }

  if (name_string) {
    manifest->Set(manifest_keys::kName, name_string.value());
  }

  if (description_string) {
    manifest->Set(manifest_keys::kDescription, description_string.value());
  }

  std::string actual_extension_id =
      Add(std::move(*manifest), root_directory, false);
  CHECK_EQ(extension_id, actual_extension_id);
  if (done_cb) {
    std::move(done_cb).Run();
  }
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_ASH)
void ComponentLoader::AddComponentFromDir(const base::FilePath& root_directory,
                                          const ExtensionId& extension_id,
                                          base::OnceClosure done_cb) {
  AddComponentFromDirWithManifestFilename(
      root_directory, extension_id, extensions::kManifestFilename,
      extension_misc::kGuestManifestFilename, std::move(done_cb));
}

void ComponentLoader::AddWithNameAndDescriptionFromDir(
    const base::FilePath& root_directory,
    const ExtensionId& extension_id,
    const std::string& name_string,
    const std::string& description_string) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  GetExtensionFileTaskRunner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&LoadManifestOnFileThread, root_directory,
                     extensions::kManifestFilename, false),
      base::BindOnce(&ComponentLoader::FinishAddComponentFromDir,
                     weak_factory_.GetWeakPtr(), root_directory, extension_id,
                     name_string, description_string, base::OnceClosure()));
}

void ComponentLoader::AddChromeOsSpeechSynthesisExtensions() {
  if (!Exists(extension_misc::kGoogleSpeechSynthesisExtensionId)) {
    AddComponentFromDir(
        base::FilePath(extension_misc::kGoogleSpeechSynthesisExtensionPath),
        extension_misc::kGoogleSpeechSynthesisExtensionId,
        base::BindRepeating(
            &ComponentLoader::FinishLoadSpeechSynthesisExtension,
            weak_factory_.GetWeakPtr(),
            extension_misc::kGoogleSpeechSynthesisExtensionId));
  }

  if (!Exists(extension_misc::kEspeakSpeechSynthesisExtensionId)) {
    AddComponentFromDir(
        base::FilePath(extension_misc::kEspeakSpeechSynthesisExtensionPath),
        extension_misc::kEspeakSpeechSynthesisExtensionId,
        base::BindRepeating(
            &ComponentLoader::FinishLoadSpeechSynthesisExtension,
            weak_factory_.GetWeakPtr(),
            extension_misc::kEspeakSpeechSynthesisExtensionId));
  }
}

void ComponentLoader::FinishLoadSpeechSynthesisExtension(
    const ExtensionId& extension_id) {
  // TODO(crbug.com/41449926): mitigation for extension not awake after
  // load.
  extensions::ProcessManager::Get(profile_)->WakeEventPage(extension_id,
                                                           base::DoNothing());
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace extensions
