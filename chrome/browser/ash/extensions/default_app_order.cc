// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/extensions/default_app_order.h"

#include <array>
#include <utility>

#include "ash/components/arc/app/arc_app_constants.h"
#include "ash/constants/ash_paths.h"
#include "ash/constants/web_app_id_constants.h"
#include "ash/public/cpp/app_list/internal_app_id_constants.h"
#include "ash/webui/mall/app_id.h"
#include "ash/webui/projector_app/public/cpp/projector_app_constants.h"
#include "ash/webui/system_apps/public/system_web_app_type.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_file_value_serializer.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "base/task/thread_pool.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/apps/app_service/policy_util.h"
#include "chrome/browser/ash/guest_os/guest_os_terminal.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chromeos/ash/components/file_manager/app_id.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/app_constants/constants.h"
#include "components/services/app_service/public/cpp/package_id.h"
#include "extensions/common/constants.h"

using apps::PackageId;
using apps::PackageType;

namespace chromeos {
namespace default_app_order {
namespace {

// The single ExternalLoader instance.
ExternalLoader* loader_instance = nullptr;

// Names used in JSON file.
const char kOemAppsFolderAttr[] = "oem_apps_folder";
const char kLocalizedContentAttr[] = "localized_content";
const char kDefaultAttr[] = "default";
const char kNameAttr[] = "name";
const char kImportDefaultOrderAttr[] = "import_default_order";

// Reads external ordinal json file and returns the parsed value. Returns NULL
// if the file does not exist or could not be parsed properly.
std::unique_ptr<base::Value::List> ReadExternalOrdinalFile(
    const base::FilePath& path) {
  if (!base::PathExists(path))
    return nullptr;

  JSONFileValueDeserializer deserializer(path);
  std::string error_msg;
  std::unique_ptr<base::Value> value =
      deserializer.Deserialize(nullptr, &error_msg);
  if (!value) {
    LOG(WARNING) << "Unable to deserialize default app ordinals json data:"
                 << error_msg << ", file=" << path.value();
    return nullptr;
  }

  if (!value->is_list())
    LOG(WARNING) << "Expect a JSON list in file " << path.value();

  return std::make_unique<base::Value::List>(std::move(*value).TakeList());
}

std::string GetLocaleSpecificStringImpl(const base::Value::Dict& root,
                                        const std::string& locale,
                                        const std::string& dictionary_name,
                                        const std::string& entry_name) {
  const base::Value::Dict* dict_content = root.FindDict(dictionary_name);
  if (!dict_content)
    return std::string();

  const base::Value::Dict* locale_dict = dict_content->FindDict(locale);
  if (locale_dict) {
    const std::string* result = locale_dict->FindString(entry_name);
    if (result)
      return *result;
  }

  const base::Value::Dict* default_dict = dict_content->FindDict(kDefaultAttr);
  if (default_dict) {
    const std::string* result = default_dict->FindString(entry_name);
    if (result)
      return *result;
  }

  return std::string();
}

// Gets built-in default app order.
void GetDefault(std::vector<std::string>* app_ids) {
  // Canonical ordering specified in: go/default-apps
  // clang-format off
  app_ids->insert(app_ids->end(), {
    app_constants::kChromeAppId,
    arc::kPlayStoreAppId,

    extension_misc::kFilesManagerAppId,
    file_manager::kFileManagerSwaAppId
  });

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  if (chromeos::features::IsContainerAppPreinstallEnabled()) {
      app_ids->push_back(web_app::kContainerAppId);
  }
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

  app_ids->insert(app_ids->end(), {
    arc::kGmailAppId,
    extension_misc::kGmailAppId,
    web_app::kGmailAppId,

    web_app::kGoogleMeetAppId,

    web_app::kGoogleChatAppId,

    extension_misc::kGoogleDocsAppId,
    web_app::kGoogleDocsAppId,

    extension_misc::kGoogleSlidesAppId,
    web_app::kGoogleSlidesAppId,

    extension_misc::kGoogleSheetsAppId,
    web_app::kGoogleSheetsAppId,

    extension_misc::kGoogleDriveAppId,
    web_app::kGoogleDriveAppId,

    extension_misc::kGoogleKeepAppId,
    web_app::kGoogleKeepAppId,

    arc::kGoogleCalendarAppId,
    extension_misc::kCalendarAppId,
    web_app::kGoogleCalendarAppId,

    web_app::kMessagesAppId,

    arc::kYoutubeAppId,
    extension_misc::kYoutubeAppId,
    web_app::kYoutubeAppId,

    arc::kYoutubeMusicAppId,
    web_app::kYoutubeMusicAppId,
    arc::kYoutubeMusicWebApkAppId,

    arc::kPlayMoviesAppId,
    extension_misc::kGooglePlayMoviesAppId,
    arc::kGoogleTVAppId,

    arc::kPlayMusicAppId,
    extension_misc::kGooglePlayMusicAppId,

    arc::kPlayBooksAppId,
    extension_misc::kGooglePlayBooksAppId,
    web_app::kPlayBooksAppId,

    web_app::kCameraAppId,
    web_app::kRecorderAppId,

    arc::kGooglePhotosAppId,
    extension_misc::kGooglePhotosAppId,

    arc::kGoogleMapsAppId,
    web_app::kGoogleMapsAppId,

    ash::kInternalAppIdSettings,
    web_app::kSettingsAppId,
    web_app::kOsSettingsAppId,

    web_app::kHelpAppId,

    web_app::kMallAppId,
    ash::kMallSystemAppId,

    web_app::kCalculatorAppId,
    extension_misc::kCalculatorAppId,

    web_app::kMediaAppId,
    web_app::kCursiveAppId,
    web_app::kCanvasAppId,

    ash::kChromeUIUntrustedProjectorSwaAppId,
    web_app::kAdobeExpressAppId,
    extension_misc::kTextEditorAppId,
    web_app::kPrintManagementAppId,
    web_app::kScanningAppId,
    web_app::kShortcutCustomizationAppId,
    guest_os::kTerminalSystemAppId,

    web_app::kYoutubeTVAppId,
    web_app::kGoogleNewsAppId,
    extensions::kWebStoreAppId,
    web_app::kGraduationAppId,

    arc::kLightRoomAppId,
    arc::kInfinitePainterAppId,
    web_app::kShowtimeAppId,
    extension_misc::kGooglePlusAppId,
  });
  // clang-format on

  if (chromeos::features::IsCloudGamingDeviceEnabled()) {
    app_ids->push_back(web_app::kNvidiaGeForceNowAppId);
  }
}

PackageId SystemPackageId(ash::SystemWebAppType type) {
  return PackageId(PackageType::kSystem,
                   *apps_util::GetPolicyIdForSystemWebAppType(type));
}

}  // namespace

size_t DefaultAppCount() {
  std::vector<std::string> apps;

  GetDefault(&apps);

  return apps.size();
}

ExternalLoader::ExternalLoader(bool async)
    : loaded_(base::WaitableEvent::ResetPolicy::MANUAL,
              base::WaitableEvent::InitialState::NOT_SIGNALED) {
  DCHECK(!loader_instance);
  loader_instance = this;

  if (async) {
    base::ThreadPool::PostTask(
        FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
        base::BindOnce(&ExternalLoader::Load, base::Unretained(this)));
  } else {
    Load();
  }
}

ExternalLoader::~ExternalLoader() {
  DCHECK(loaded_.IsSignaled());
  DCHECK_EQ(loader_instance, this);
  loader_instance = nullptr;
}

const std::vector<std::string>& ExternalLoader::GetAppIds() {
  if (!loaded_.IsSignaled())
    LOG(ERROR) << "GetAppIds() called before loaded.";
  return app_ids_;
}

const std::string& ExternalLoader::GetOemAppsFolderName() {
  if (!loaded_.IsSignaled())
    LOG(ERROR) << "GetOemAppsFolderName() called before loaded.";
  return oem_apps_folder_name_;
}

void ExternalLoader::Load() {
  base::FilePath ordinals_file;
  CHECK(base::PathService::Get(ash::FILE_DEFAULT_APP_ORDER, &ordinals_file));

  std::unique_ptr<base::Value::List> ordinals_value =
      ReadExternalOrdinalFile(ordinals_file);
  if (ordinals_value) {
    std::string locale = g_browser_process->GetApplicationLocale();
    for (const base::Value& i : *ordinals_value) {
      if (i.is_string()) {
        std::string app_id = i.GetString();
        app_ids_.push_back(app_id);
      } else if (i.is_dict()) {
        const base::Value::Dict& dict = i.GetDict();
        if (dict.FindBool(kOemAppsFolderAttr).value_or(false)) {
          oem_apps_folder_name_ = GetLocaleSpecificStringImpl(
              dict, locale, kLocalizedContentAttr, kNameAttr);
        } else if (dict.FindBool(kImportDefaultOrderAttr).value_or(false)) {
          GetDefault(&app_ids_);
        } else {
          LOG(ERROR) << "Invalid syntax in default_app_order.json";
        }
      } else {
        LOG(ERROR) << "Invalid entry in default_app_order.json";
      }
    }
  } else {
    GetDefault(&app_ids_);
  }

  loaded_.Signal();
}

void Get(std::vector<std::string>* app_ids) {
  // |loader_instance| could be NULL for test.
  if (!loader_instance) {
    GetDefault(app_ids);
    return;
  }

  *app_ids = loader_instance->GetAppIds();
}

base::span<const apps::LauncherItem> GetAppPreloadServiceDefaults() {
  static const base::NoDestructor<std::array<apps::LauncherItem, 20>>
      kPackageIds({
          PackageId(PackageType::kChromeApp, app_constants::kChromeAppId),
          PackageId(PackageType::kSystem, app_constants::kLacrosChrome),
          PackageId(PackageType::kChromeApp, arc::kPlayStoreAppId),
          SystemPackageId(ash::SystemWebAppType::FILE_MANAGER),
          PackageId(PackageType::kWeb, web_app::kGmailManifestId),
          PackageId(PackageType::kWeb, web_app::kGoogleDocsManifestId),
          PackageId(PackageType::kWeb, web_app::kGoogleSlidesManifestId),
          PackageId(PackageType::kWeb, web_app::kGoogleSheetsManifestId),
          PackageId(PackageType::kWeb, web_app::kGoogleDriveManifestId),
          PackageId(PackageType::kWeb, web_app::kYoutubeManifestId),
          SystemPackageId(ash::SystemWebAppType::CAMERA),
          SystemPackageId(ash::SystemWebAppType::SETTINGS),
          SystemPackageId(ash::SystemWebAppType::HELP),
          SystemPackageId(ash::SystemWebAppType::MALL),
          SystemPackageId(ash::SystemWebAppType::MEDIA),
          SystemPackageId(ash::SystemWebAppType::PROJECTOR),
          SystemPackageId(ash::SystemWebAppType::PRINT_MANAGEMENT),
          SystemPackageId(ash::SystemWebAppType::SCANNING),
          SystemPackageId(ash::SystemWebAppType::SHORTCUT_CUSTOMIZATION),
          SystemPackageId(ash::SystemWebAppType::TERMINAL),
      });

  return *kPackageIds;
}

std::string GetOemAppsFolderName() {
  // |loader_instance| could be NULL for test.
  if (!loader_instance)
    return std::string();
  else
    return loader_instance->GetOemAppsFolderName();
}

}  // namespace default_app_order
}  // namespace chromeos
