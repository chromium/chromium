// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/extensions/default_app_order.h"

#include <array>
#include <utility>

#include "ash/constants/ash_paths.h"
#include "ash/constants/web_app_id_constants.h"
#include "ash/public/cpp/app_list/internal_app_id_constants.h"
#include "ash/webui/mall/app_id.h"
#include "base/containers/span.h"
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
#include "chrome/browser/web_applications/policy/app_service_web_app_policy.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chromeos/ash/components/file_manager/app_id.h"
#include "chromeos/ash/components/system_web_apps/system_web_app_type.h"
#include "chromeos/ash/experiences/arc/app/arc_app_constants.h"
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
std::unique_ptr<base::ListValue> ReadExternalOrdinalFile(
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

  return std::make_unique<base::ListValue>(std::move(*value).TakeList());
}

std::string GetLocaleSpecificStringImpl(const base::DictValue& root,
                                        const std::string& locale,
                                        const std::string& dictionary_name,
                                        const std::string& entry_name) {
  const base::DictValue* dict_content = root.FindDict(dictionary_name);
  if (!dict_content)
    return std::string();

  const base::DictValue* locale_dict = dict_content->FindDict(locale);
  if (locale_dict) {
    const std::string* result = locale_dict->FindString(entry_name);
    if (result)
      return *result;
  }

  const base::DictValue* default_dict = dict_content->FindDict(kDefaultAttr);
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
  if (chromeos::features::IsGeminiAppPreinstallEnabled()) {
    app_ids->push_back(ash::kGeminiAppId);
  }
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

  app_ids->insert(app_ids->end(), {
    arc::kGmailAppId,
    extension_misc::kGmailAppId,
    ash::kGmailAppId,

    ash::kGoogleMeetAppId,

    ash::kGoogleChatAppId,
    ash::kOldGoogleChatAppId,

    extension_misc::kGoogleDocsAppId,
    ash::kGoogleDocsAppId,

    extension_misc::kGoogleSlidesAppId,
    ash::kGoogleSlidesAppId,

    extension_misc::kGoogleSheetsAppId,
    ash::kGoogleSheetsAppId,

    extension_misc::kGoogleDriveAppId,
    ash::kGoogleDriveAppId,

    extension_misc::kGoogleKeepAppId,
    ash::kGoogleKeepAppId,

    arc::kGoogleCalendarAppId,
    extension_misc::kCalendarAppId,
    ash::kGoogleCalendarAppId,

    ash::kMessagesAppId,

    ash::kNotebookLmAppId,

    ash::kVidsAppId,

    arc::kYoutubeAppId,
    extension_misc::kYoutubeAppId,
    ash::kYoutubeAppId,

    arc::kYoutubeMusicAppId,
    ash::kYoutubeMusicAppId,
    arc::kYoutubeMusicWebApkAppId,

    arc::kPlayMoviesAppId,
    extension_misc::kGooglePlayMoviesAppId,
    arc::kGoogleTVAppId,

    arc::kPlayMusicAppId,
    extension_misc::kGooglePlayMusicAppId,

    arc::kPlayBooksAppId,
    extension_misc::kGooglePlayBooksAppId,
    ash::kPlayBooksAppId,

    ash::kCameraAppId,
    ash::kRecorderAppId,

    arc::kGooglePhotosAppId,
    extension_misc::kGooglePhotosAppId,

    arc::kGoogleMapsAppId,
    ash::kGoogleMapsAppId,

    ash::kInternalAppIdSettings,
    ash::kSettingsAppId,
    ash::kOsSettingsAppId,

    ash::kHelpAppId,

    ash::kMallSystemAppId,

    ash::kCalculatorAppId,
    extension_misc::kCalculatorAppId,

    ash::kMediaAppId,
    ash::kCursiveAppId,
    ash::kCanvasAppId,

    ash::kChromeUIUntrustedProjectorSwaAppId,
    ash::kAdobeExpressAppId,
    extension_misc::kTextEditorAppId,
    ash::kPrintManagementAppId,
    ash::kScanningAppId,
    ash::kShortcutCustomizationAppId,
    guest_os::kTerminalSystemAppId,

    ash::kYoutubeTVAppId,
    ash::kGoogleNewsAppId,
    extensions::kWebStoreAppId,
    ash::kGraduationAppId,

    arc::kLightRoomAppId,
    arc::kInfinitePainterAppId,
    ash::kShowtimeAppId,
    extension_misc::kGooglePlusAppId,
  });
  // clang-format on

  if (chromeos::features::IsCloudGamingDeviceEnabled()) {
    app_ids->push_back(ash::kNvidiaGeForceNowAppId);
  }
}

PackageId SystemPackageId(ash::SystemWebAppType type) {
  return PackageId(PackageType::kSystem,
                   *web_app::GetPolicyIdForSystemWebAppType(type));
}

}  // namespace

size_t DefaultAppCount() {
  std::vector<std::string> apps;

  GetDefault(&apps);

  return apps.size();
}

// static
ExternalLoader::ParsedAppOrder ExternalLoader::ReadAndParseAppOrder(
    base::FilePath path,
    std::string locale) {
  std::unique_ptr<base::ListValue> ordinals_value =
      ReadExternalOrdinalFile(path);
  if (!ordinals_value) {
    std::vector<std::string> app_ids;
    GetDefault(&app_ids);
    return {app_ids, ""};
  }

  std::vector<std::string> app_ids;
  std::string oem_apps_folder_name;
  for (const base::Value& entry : *ordinals_value) {
    if (entry.is_string()) {
      app_ids.push_back(entry.GetString());
      continue;
    }
    if (entry.is_dict()) {
      const base::DictValue& dict = entry.GetDict();
      if (dict.FindBool(kOemAppsFolderAttr).value_or(false)) {
        oem_apps_folder_name = GetLocaleSpecificStringImpl(
            dict, locale, kLocalizedContentAttr, kNameAttr);
      } else if (dict.FindBool(kImportDefaultOrderAttr).value_or(false)) {
        GetDefault(&app_ids);
      } else {
        LOG(ERROR) << "Invalid syntax in default_app_order.json";
      }
      continue;
    }
    LOG(ERROR) << "Invalid entry in default_app_order.json";
  }
  return {app_ids, oem_apps_folder_name};
}

ExternalLoader::ExternalLoader(std::string locale, bool async) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!loader_instance, base::NotFatalUntil::M160);
  loader_instance = this;

  base::FilePath ordinals_file;
  CHECK(base::PathService::Get(ash::FILE_DEFAULT_APP_ORDER, &ordinals_file));

  if (async) {
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
        base::BindOnce(&ExternalLoader::ReadAndParseAppOrder,
                       std::move(ordinals_file), std::move(locale)),
        base::BindOnce(&ExternalLoader::OnLoadFinished,
                       weak_ptr_factory_.GetWeakPtr()));
  } else {
    OnLoadFinished(
        ReadAndParseAppOrder(std::move(ordinals_file), std::move(locale)));
  }
}

ExternalLoader::~ExternalLoader() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_EQ(loader_instance, this, base::NotFatalUntil::M160);
  loader_instance = nullptr;
}

const std::vector<std::string>& ExternalLoader::GetAppIds() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!is_loaded_) {
    LOG(ERROR) << "GetAppIds() called before loaded.";
  }
  return app_order_.app_ids;
}

const std::string& ExternalLoader::GetOemAppsFolderName() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!is_loaded_) {
    LOG(ERROR) << "GetOemAppsFolderName() called before loaded.";
  }
  return app_order_.oem_apps_folder_name;
}

void ExternalLoader::OnLoadFinished(ParsedAppOrder parsed_order) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  app_order_ = std::move(parsed_order);
  is_loaded_ = true;
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
  static const base::NoDestructor<std::array<apps::LauncherItem, 19>>
      kPackageIds({
          PackageId(PackageType::kChromeApp, app_constants::kChromeAppId),
          PackageId(PackageType::kChromeApp, arc::kPlayStoreAppId),
          SystemPackageId(ash::SystemWebAppType::FILE_MANAGER),
          PackageId(PackageType::kWeb, ash::kGmailManifestId),
          PackageId(PackageType::kWeb, ash::kGoogleDocsManifestId),
          PackageId(PackageType::kWeb, ash::kGoogleSlidesManifestId),
          PackageId(PackageType::kWeb, ash::kGoogleSheetsManifestId),
          PackageId(PackageType::kWeb, ash::kGoogleDriveManifestId),
          PackageId(PackageType::kWeb, ash::kYoutubeManifestId),
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
