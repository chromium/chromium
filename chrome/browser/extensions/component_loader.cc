// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/component_loader.h"

#include <string>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/json/json_string_value_serializer.h"
#include "base/metrics/histogram_macros.h"
#include "base/path_service.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/component_extensions_whitelist/whitelist.h"
#include "chrome/browser/extensions/data_deleter.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/pdf/pdf_extension_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/crx_file/id_util.h"
#include "components/nacl/common/buildflags.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/plugin_service.h"
#include "content/public/common/content_switches.h"
#include "extensions/browser/extension_file_task_runner.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_l10n_util.h"
#include "extensions/common/file_util.h"
#include "extensions/common/manifest_constants.h"
#include "ppapi/buildflags/buildflags.h"
#include "printing/buildflags/buildflags.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

#if defined(OS_CHROMEOS)
#include "chromeos/chromeos_switches.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/storage_partition.h"
#include "extensions/browser/extensions_browser_client.h"
#include "storage/browser/fileapi/file_system_context.h"
#include "ui/chromeos/devicetype_utils.h"
#include "ui/file_manager/grit/file_manager_resources.h"
#include "ui/keyboard/grit/keyboard_resources.h"
#endif

#if defined(GOOGLE_CHROME_BUILD)
#include "chrome/browser/defaults.h"
#endif

using content::BrowserThread;

namespace extensions {

namespace {

static bool enable_background_extensions_during_testing = false;

std::string GenerateId(const base::DictionaryValue* manifest,
                       const base::FilePath& path) {
  std::string raw_key;
  std::string id_input;
  CHECK(manifest->GetString(manifest_keys::kPublicKey, &raw_key));
  CHECK(Extension::ParsePEMKeyBytes(raw_key, &id_input));
  std::string id = crx_file::id_util::GenerateId(id_input);
  return id;
}

#if defined(OS_CHROMEOS)
std::unique_ptr<base::DictionaryValue> LoadManifestOnFileThread(
    const base::FilePath& root_directory,
    const base::FilePath::CharType* manifest_filename,
    bool localize_manifest) {
  DCHECK(GetExtensionFileTaskRunner()->RunsTasksInCurrentSequence());
  std::string error;
  std::unique_ptr<base::DictionaryValue> manifest(
      file_util::LoadManifest(root_directory, manifest_filename, &error));
  if (!manifest) {
    LOG(ERROR) << "Can't load "
               << root_directory.Append(manifest_filename).AsUTF8Unsafe()
               << ": " << error;
    return nullptr;
  }

  if (localize_manifest) {
    bool localized = extension_l10n_util::LocalizeExtension(
        root_directory, manifest.get(), &error);
    CHECK(localized) << error;
  }

  return manifest;
}

bool IsNormalSession() {
  return !base::CommandLine::ForCurrentProcess()->HasSwitch(
             chromeos::switches::kGuestSession) &&
         user_manager::UserManager::IsInitialized() &&
         user_manager::UserManager::Get()->IsUserLoggedIn();
}
#endif  // defined(OS_CHROMEOS)

}  // namespace

ComponentLoader::ComponentExtensionInfo::ComponentExtensionInfo(
    std::unique_ptr<base::DictionaryValue> manifest_param,
    const base::FilePath& directory)
    : manifest(std::move(manifest_param)), root_directory(directory) {
  if (!root_directory.IsAbsolute()) {
    CHECK(base::PathService::Get(chrome::DIR_RESOURCES, &root_directory));
    root_directory = root_directory.Append(directory);
  }
  extension_id = GenerateId(manifest.get(), root_directory);
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

ComponentLoader::ComponentExtensionInfo::~ComponentExtensionInfo() {}

ComponentLoader::ComponentLoader(ExtensionServiceInterface* extension_service,
                                 PrefService* profile_prefs,
                                 PrefService* local_state,
                                 Profile* profile)
    : profile_prefs_(profile_prefs),
      local_state_(local_state),
      profile_(profile),
      extension_service_(extension_service),
      ignore_whitelist_for_testing_(false),
      weak_factory_(this) {}

ComponentLoader::~ComponentLoader() {
}

void ComponentLoader::LoadAll() {
  TRACE_EVENT0("browser,startup", "ComponentLoader::LoadAll");
  SCOPED_UMA_HISTOGRAM_TIMER("Extensions.LoadAllComponentTime");

  for (const auto& component_extension : component_extensions_)
    Load(component_extension);
}

std::unique_ptr<base::DictionaryValue> ComponentLoader::ParseManifest(
    base::StringPiece manifest_contents) const {
  JSONStringValueDeserializer deserializer(manifest_contents);
  std::unique_ptr<base::Value> manifest = deserializer.Deserialize(NULL, NULL);

  if (!manifest.get() || !manifest->is_dict()) {
    LOG(ERROR) << "Failed to parse extension manifest.";
    return std::unique_ptr<base::DictionaryValue>();
  }
  return base::DictionaryValue::From(std::move(manifest));
}

std::string ComponentLoader::Add(int manifest_resource_id,
                                 const base::FilePath& root_directory) {
  if (!ignore_whitelist_for_testing_ &&
      !IsComponentExtensionWhitelisted(manifest_resource_id))
    return std::string();

  base::StringPiece manifest_contents =
      ui::ResourceBundle::GetSharedInstance().GetRawDataResource(
          manifest_resource_id);
  return Add(manifest_contents, root_directory, true);
}

std::string ComponentLoader::Add(const base::StringPiece& manifest_contents,
                                 const base::FilePath& root_directory) {
  return Add(manifest_contents, root_directory, false);
}

std::string ComponentLoader::Add(const base::StringPiece& manifest_contents,
                                 const base::FilePath& root_directory,
                                 bool skip_whitelist) {
  // The Value is kept for the lifetime of the ComponentLoader. This is
  // required in case LoadAll() is called again.
  std::unique_ptr<base::DictionaryValue> manifest =
      ParseManifest(manifest_contents);
  if (manifest)
    return Add(std::move(manifest), root_directory, skip_whitelist);
  return std::string();
}

std::string ComponentLoader::Add(
    std::unique_ptr<base::DictionaryValue> parsed_manifest,
    const base::FilePath& root_directory,
    bool skip_whitelist) {
  ComponentExtensionInfo info(std::move(parsed_manifest), root_directory);
  if (!ignore_whitelist_for_testing_ &&
      !skip_whitelist &&
      !IsComponentExtensionWhitelisted(info.extension_id))
    return std::string();

  component_extensions_.push_back(std::move(info));
  ComponentExtensionInfo& added_info = component_extensions_.back();
  if (extension_service_->is_ready())
    Load(added_info);
  return added_info.extension_id;
}

std::string ComponentLoader::AddOrReplace(const base::FilePath& path) {
  base::FilePath absolute_path = base::MakeAbsoluteFilePath(path);
  std::string error;
  std::unique_ptr<base::DictionaryValue> manifest(
      file_util::LoadManifest(absolute_path, &error));
  if (!manifest) {
    LOG(ERROR) << "Could not load extension from '" <<
                  absolute_path.value() << "'. " << error;
    return std::string();
  }
  Remove(GenerateId(manifest.get(), absolute_path));

  // We don't check component extensions loaded by path because this is only
  // used by developers for testing.
  return Add(std::move(manifest), absolute_path, true);
}

void ComponentLoader::Reload(const std::string& extension_id) {
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
  extension_service_->AddComponentExtension(extension.get());
}

void ComponentLoader::Remove(const base::FilePath& root_directory) {
  // Find the ComponentExtensionInfo for the extension.
  for (const auto& component_extension : component_extensions_) {
    if (component_extension.root_directory == root_directory) {
      Remove(GenerateId(component_extension.manifest.get(), root_directory));
      break;
    }
  }
}

void ComponentLoader::Remove(const std::string& id) {
  for (auto it = component_extensions_.begin();
       it != component_extensions_.end(); ++it) {
    if (it->extension_id == id) {
      UnloadComponent(&(*it));
      component_extensions_.erase(it);
      break;
    }
  }
}

bool ComponentLoader::Exists(const std::string& id) const {
  for (const auto& component_extension : component_extensions_) {
    if (component_extension.extension_id == id)
      return true;
  }
  return false;
}

void ComponentLoader::AddFileManagerExtension() {
#if defined(OS_CHROMEOS)
  AddWithNameAndDescription(
      IDR_FILEMANAGER_MANIFEST,
      base::FilePath(FILE_PATH_LITERAL("file_manager")),
      l10n_util::GetStringUTF8(IDS_FILEMANAGER_APP_NAME),
      l10n_util::GetStringUTF8(IDS_FILEMANAGER_APP_DESCRIPTION));
#endif  // defined(OS_CHROMEOS)
}

void ComponentLoader::AddVideoPlayerExtension() {
#if defined(OS_CHROMEOS)
  Add(IDR_VIDEO_PLAYER_MANIFEST,
      base::FilePath(FILE_PATH_LITERAL("video_player")));
#endif  // defined(OS_CHROMEOS)
}

void ComponentLoader::AddAudioPlayerExtension() {
#if defined(OS_CHROMEOS)
  Add(IDR_AUDIO_PLAYER_MANIFEST,
      base::FilePath(FILE_PATH_LITERAL("audio_player")));
#endif  // defined(OS_CHROMEOS)
}

void ComponentLoader::AddGalleryExtension() {
#if defined(OS_CHROMEOS)
  Add(IDR_GALLERY_MANIFEST, base::FilePath(FILE_PATH_LITERAL("gallery")));
#endif
}

void ComponentLoader::AddZipArchiverExtension() {
#if defined(OS_CHROMEOS) && BUILDFLAG(ENABLE_NACL)
  base::FilePath resources_path;
  if (base::PathService::Get(chrome::DIR_RESOURCES, &resources_path)) {
    AddWithNameAndDescriptionFromDir(
        resources_path.Append(extension_misc::kZipArchiverExtensionPath),
        extension_misc::kZipArchiverExtensionId,
        l10n_util::GetStringUTF8(IDS_ZIP_ARCHIVER_NAME),
        l10n_util::GetStringUTF8(IDS_ZIP_ARCHIVER_DESCRIPTION));
  }
#endif  // defined(OS_CHROMEOS) && BUILDFLAG(ENABLE_NACL)
}

void ComponentLoader::AddHangoutServicesExtension() {
#if BUILDFLAG(ENABLE_HANGOUT_SERVICES_EXTENSION)
  Add(IDR_HANGOUT_SERVICES_MANIFEST,
      base::FilePath(FILE_PATH_LITERAL("hangout_services")));
#endif
}

void ComponentLoader::AddImageLoaderExtension() {
#if defined(OS_CHROMEOS)
  Add(IDR_IMAGE_LOADER_MANIFEST,
      base::FilePath(FILE_PATH_LITERAL("image_loader")));
#endif  // defined(OS_CHROMEOS)
}

void ComponentLoader::AddNetworkSpeechSynthesisExtension() {
  Add(IDR_NETWORK_SPEECH_SYNTHESIS_MANIFEST,
      base::FilePath(FILE_PATH_LITERAL("network_speech_synthesis")));
}

#if defined(OS_CHROMEOS)
void ComponentLoader::AddChromeOsSpeechSynthesisExtensions() {
  AddComponentFromDir(
      base::FilePath(extension_misc::kGoogleSpeechSynthesisExtensionPath),
      extension_misc::kGoogleSpeechSynthesisExtensionId,
      base::BindRepeating(&ComponentLoader::EnableFileSystemInGuestMode,
                          weak_factory_.GetWeakPtr(),
                          extension_misc::kGoogleSpeechSynthesisExtensionId));

  if (chromeos::switches::AreExperimentalAccessibilityFeaturesEnabled()) {
    AddComponentFromDir(
        base::FilePath(extension_misc::kEspeakSpeechSynthesisExtensionPath),
        extension_misc::kEspeakSpeechSynthesisExtensionId,
        base::RepeatingClosure());
  }
}
#endif

void ComponentLoader::AddWithNameAndDescription(
    int manifest_resource_id,
    const base::FilePath& root_directory,
    const std::string& name_string,
    const std::string& description_string) {
  if (!ignore_whitelist_for_testing_ &&
      !IsComponentExtensionWhitelisted(manifest_resource_id))
    return;

  base::StringPiece manifest_contents =
      ui::ResourceBundle::GetSharedInstance().GetRawDataResource(
          manifest_resource_id);

  // The Value is kept for the lifetime of the ComponentLoader. This is
  // required in case LoadAll() is called again.
  std::unique_ptr<base::DictionaryValue> manifest =
      ParseManifest(manifest_contents);

  if (manifest) {
    manifest->SetString(manifest_keys::kName, name_string);
    manifest->SetString(manifest_keys::kDescription, description_string);
    Add(std::move(manifest), root_directory, true);
  }
}

void ComponentLoader::AddChromeApp() {
#if BUILDFLAG(ENABLE_APP_LIST)
  AddWithNameAndDescription(
      IDR_CHROME_APP_MANIFEST, base::FilePath(FILE_PATH_LITERAL("chrome_app")),
      l10n_util::GetStringUTF8(IDS_SHORT_PRODUCT_NAME),
      l10n_util::GetStringUTF8(IDS_CHROME_SHORTCUT_DESCRIPTION));
#endif
}

void ComponentLoader::AddKeyboardApp() {
#if defined(OS_CHROMEOS)
  Add(IDR_KEYBOARD_MANIFEST, base::FilePath(FILE_PATH_LITERAL("keyboard")));
#endif
}

void ComponentLoader::AddWebStoreApp() {
#if defined(OS_CHROMEOS)
  if (!IsNormalSession())
    return;
#endif

  AddWithNameAndDescription(
      IDR_WEBSTORE_MANIFEST, base::FilePath(FILE_PATH_LITERAL("web_store")),
      l10n_util::GetStringUTF8(IDS_WEBSTORE_NAME_STORE),
      l10n_util::GetStringUTF8(IDS_WEBSTORE_APP_DESCRIPTION));
}

scoped_refptr<const Extension> ComponentLoader::CreateExtension(
    const ComponentExtensionInfo& info, std::string* utf8_error) {
  // TODO(abarth): We should REQUIRE_MODERN_MANIFEST_VERSION once we've updated
  //               our component extensions to the new manifest version.
  int flags = Extension::REQUIRE_KEY;
  return Extension::Create(
      info.root_directory,
      Manifest::COMPONENT,
      *info.manifest,
      flags,
      utf8_error);
}

// static
void ComponentLoader::EnableBackgroundExtensionsForTesting() {
  enable_background_extensions_during_testing = true;
}

void ComponentLoader::AddDefaultComponentExtensions(
    bool skip_session_components) {
  // Do not add component extensions that have background pages here -- add them
  // to AddDefaultComponentExtensionsWithBackgroundPages.
#if defined(OS_CHROMEOS)
  Add(IDR_MOBILE_MANIFEST,
      base::FilePath(FILE_PATH_LITERAL("/usr/share/chromeos-assets/mobile")));

#if defined(GOOGLE_CHROME_BUILD)
  if (browser_defaults::enable_help_app) {
    Add(IDR_HELP_MANIFEST, base::FilePath(FILE_PATH_LITERAL(
                               "/usr/share/chromeos-assets/helpapp")));
  }
#endif  // defined(GOOGLE_CHROME_BUILD)

  // Skip all other extensions that require user session presence.
  if (!skip_session_components) {
    Add(IDR_CROSH_BUILTIN_MANIFEST, base::FilePath(FILE_PATH_LITERAL(
        "/usr/share/chromeos-assets/crosh_builtin")));
  }
#else  // defined(OS_CHROMEOS)
  DCHECK(!skip_session_components);
#if BUILDFLAG(ENABLE_PRINTING)
  // Cloud Print component app. Not required on Chrome OS.
  Add(IDR_CLOUDPRINT_MANIFEST,
      base::FilePath(FILE_PATH_LITERAL("cloud_print")));
#endif  // BUILDFLAG(ENABLE_PRINTING)
#endif  // defined(OS_CHROMEOS)

  if (!skip_session_components) {
    AddWebStoreApp();
    AddChromeApp();
  }

  AddKeyboardApp();

  AddDefaultComponentExtensionsWithBackgroundPages(skip_session_components);

#if BUILDFLAG(ENABLE_PLUGINS)
  Add(pdf_extension_util::GetManifest(),
      base::FilePath(FILE_PATH_LITERAL("pdf")));
#endif
}

void ComponentLoader::AddDefaultComponentExtensionsForKioskMode(
    bool skip_session_components) {
  // Do not add component extensions that have background pages here -- add them
  // to AddDefaultComponentExtensionsWithBackgroundPagesForKioskMode.

  // No component extension for kiosk app launch splash screen.
  if (skip_session_components)
    return;

  // Component extensions needed for kiosk apps.
  AddFileManagerExtension();

  // Add virtual keyboard.
  AddKeyboardApp();

  AddDefaultComponentExtensionsWithBackgroundPagesForKioskMode();

#if BUILDFLAG(ENABLE_PLUGINS)
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
  if (!enable_background_extensions_during_testing &&
      (command_line->HasSwitch(::switches::kTestType) ||
       command_line->HasSwitch(
           ::switches::kDisableComponentExtensionsWithBackgroundPages))) {
    return;
  }

#if defined(OS_CHROMEOS) && defined(GOOGLE_CHROME_BUILD)
  // Since this is a v2 app it has a background page.
  AddWithNameAndDescription(
      IDR_GENIUS_APP_MANIFEST,
      base::FilePath(
          FILE_PATH_LITERAL("/usr/share/chromeos-assets/genius_app")),
      l10n_util::GetStringUTF8(IDS_GENIUS_APP_NAME),
      l10n_util::GetStringFUTF8(IDS_GENIUS_APP_DESCRIPTION,
                                ui::GetChromeOSDeviceName()));
#endif

  if (!skip_session_components) {
    AddVideoPlayerExtension();
    AddAudioPlayerExtension();
    AddFileManagerExtension();
    AddGalleryExtension();
    AddZipArchiverExtension();

    AddHangoutServicesExtension();
    AddImageLoaderExtension();

    bool install_feedback = enable_background_extensions_during_testing;
#if defined(GOOGLE_CHROME_BUILD)
    install_feedback = true;
#endif  // defined(GOOGLE_CHROME_BUILD)
    if (install_feedback)
      Add(IDR_FEEDBACK_MANIFEST, base::FilePath(FILE_PATH_LITERAL("feedback")));
  }

#if defined(OS_CHROMEOS)
  if (!skip_session_components) {
#if defined(GOOGLE_CHROME_BUILD)
    if (!command_line->HasSwitch(
            chromeos::switches::kDisableOfficeEditingComponentApp)) {
      std::string id = Add(IDR_QUICKOFFICE_MANIFEST, base::FilePath(
          FILE_PATH_LITERAL("/usr/share/chromeos-assets/quickoffice")));
      EnableFileSystemInGuestMode(id);
    }
#endif  // defined(GOOGLE_CHROME_BUILD)

    Add(IDR_ECHO_MANIFEST,
        base::FilePath(FILE_PATH_LITERAL("/usr/share/chromeos-assets/echo")));

    if (!command_line->HasSwitch(chromeos::switches::kGuestSession)) {
      Add(IDR_WALLPAPERMANAGER_MANIFEST,
          base::FilePath(FILE_PATH_LITERAL("chromeos/wallpaper_manager")));
    }

    Add(IDR_FIRST_RUN_DIALOG_MANIFEST,
        base::FilePath(FILE_PATH_LITERAL("chromeos/first_run/app")));

    Add(IDR_CONNECTIVITY_DIAGNOSTICS_MANIFEST,
        base::FilePath(extension_misc::kConnectivityDiagnosticsPath));
    Add(IDR_CONNECTIVITY_DIAGNOSTICS_LAUNCHER_MANIFEST,
        base::FilePath(extension_misc::kConnectivityDiagnosticsLauncherPath));

    Add(IDR_ARC_SUPPORT_MANIFEST,
        base::FilePath(FILE_PATH_LITERAL("chromeos/arc_support")));
  }
#endif  // defined(OS_CHROMEOS)

#if defined(GOOGLE_CHROME_BUILD)
#if !defined(OS_CHROMEOS)  // http://crbug.com/314799
  AddNetworkSpeechSynthesisExtension();
#endif

#endif  // defined(GOOGLE_CHROME_BUILD)

  Add(IDR_CRYPTOTOKEN_MANIFEST,
      base::FilePath(FILE_PATH_LITERAL("cryptotoken")));
}

void ComponentLoader::
    AddDefaultComponentExtensionsWithBackgroundPagesForKioskMode() {
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();

  // Component extensions with background pages are not enabled during tests
  // because they generate a lot of background behavior that can interfere.
  if (!enable_background_extensions_during_testing &&
      (command_line->HasSwitch(::switches::kTestType) ||
       command_line->HasSwitch(
           ::switches::kDisableComponentExtensionsWithBackgroundPages))) {
    return;
  }

  AddHangoutServicesExtension();
}

void ComponentLoader::UnloadComponent(ComponentExtensionInfo* component) {
  if (extension_service_->is_ready()) {
    extension_service_->
        RemoveComponentExtension(component->extension_id);
  }
}

void ComponentLoader::EnableFileSystemInGuestMode(const std::string& id) {
#if defined(OS_CHROMEOS)
  if (!IsNormalSession()) {
    // TODO(dpolukhin): Hack to enable HTML5 temporary file system for
    // the extension. Some component extensions don't work without temporary
    // file system access. Make sure temporary file system is enabled in the off
    // the record browser context (as that is the one used in guest session).
    content::BrowserContext* off_the_record_context =
        ExtensionsBrowserClient::Get()->GetOffTheRecordContext(profile_);
    GURL site = content::SiteInstance::GetSiteForURL(
        off_the_record_context, Extension::GetBaseURLFromExtensionId(id));
    storage::FileSystemContext* file_system_context =
        content::BrowserContext::GetStoragePartitionForSite(
            off_the_record_context, site)->GetFileSystemContext();
    file_system_context->EnableTemporaryFileSystemInIncognito();
  }
#endif
}

#if defined(OS_CHROMEOS)
void ComponentLoader::AddComponentFromDir(
    const base::FilePath& root_directory,
    const char* extension_id,
    const base::Closure& done_cb) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  const base::FilePath::CharType* manifest_filename =
      IsNormalSession() ? extensions::kManifestFilename
                        : extension_misc::kGuestManifestFilename;

  base::PostTaskAndReplyWithResult(
      GetExtensionFileTaskRunner().get(), FROM_HERE,
      base::Bind(&LoadManifestOnFileThread, root_directory, manifest_filename,
                 true),
      base::Bind(&ComponentLoader::FinishAddComponentFromDir,
                 weak_factory_.GetWeakPtr(), root_directory, extension_id,
                 base::nullopt, base::nullopt, done_cb));
}

void ComponentLoader::AddWithNameAndDescriptionFromDir(
    const base::FilePath& root_directory,
    const char* extension_id,
    const std::string& name_string,
    const std::string& description_string) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  base::PostTaskAndReplyWithResult(
      GetExtensionFileTaskRunner().get(), FROM_HERE,
      base::Bind(&LoadManifestOnFileThread, root_directory,
                 extensions::kManifestFilename, false),
      base::Bind(&ComponentLoader::FinishAddComponentFromDir,
                 weak_factory_.GetWeakPtr(), root_directory, extension_id,
                 name_string, description_string, base::Closure()));
}

void ComponentLoader::FinishAddComponentFromDir(
    const base::FilePath& root_directory,
    const char* extension_id,
    const base::Optional<std::string>& name_string,
    const base::Optional<std::string>& description_string,
    const base::Closure& done_cb,
    std::unique_ptr<base::DictionaryValue> manifest) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!manifest)
    return;  // Error already logged.

  if (name_string)
    manifest->SetString(manifest_keys::kName, name_string.value());

  if (description_string) {
    manifest->SetString(manifest_keys::kDescription,
                        description_string.value());
  }

  std::string actual_extension_id =
      Add(std::move(manifest), root_directory, false);
  CHECK_EQ(extension_id, actual_extension_id);
  if (!done_cb.is_null())
    done_cb.Run();
}
#endif

}  // namespace extensions
