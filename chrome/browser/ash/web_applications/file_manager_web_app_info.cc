// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/web_applications/file_manager_web_app_info.h"

#include <string>

#include "ash/constants/ash_features.h"
#include "ash/style/ash_color_provider.h"
#include "ash/webui/file_manager/resources/grit/file_manager_swa_resources.h"
#include "ash/webui/file_manager/url_constants.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/file_manager/file_tasks.h"
#include "chrome/browser/ash/web_applications/system_web_app_install_utils.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/grit/generated_resources.h"
#include "extensions/common/constants.h"
#include "third_party/blink/public/mojom/manifest/display_mode.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/styles/cros_styles.h"
#include "ui/file_manager/grit/file_manager_resources.h"

using ash::file_manager::kChromeUIFileManagerURL;

namespace {

// Appends a file handler to `info`.
// The handler action has the format: chrome://file-manager/?${ACTION_NAME}
// This means: For files with the given `file_extensions` or `mime_type` the
// Files SWA is a candidate app to open/handle such files.
void AppendFileHandler(WebAppInstallInfo& info,
                       const std::string& action_name,
                       const base::flat_set<std::string>& file_extensions,
                       const std::string& mime_type = "") {
  apps::FileHandler handler;

  GURL action = GURL(kChromeUIFileManagerURL);
  GURL::Replacements replacements;
  replacements.SetQueryStr(action_name);
  handler.action = action.ReplaceComponents(replacements);

  handler.accept.emplace_back();
  handler.accept.back().file_extensions = file_extensions;
  if (!mime_type.empty())
    handler.accept.back().mime_type = mime_type;

  info.file_handlers.push_back(std::move(handler));
}

}  // namespace

std::unique_ptr<WebAppInstallInfo> CreateWebAppInfoForFileManager() {
  auto info = std::make_unique<WebAppInstallInfo>();
  info->start_url = GURL(kChromeUIFileManagerURL);
  info->scope = GURL(kChromeUIFileManagerURL);
  info->title = l10n_util::GetStringUTF16(IDS_FILEMANAGER_APP_NAME);
  web_app::CreateIconInfoForSystemWebApp(
      info->start_url,
      {
          {"icon16.png", 16, IDR_FILE_MANAGER_ICON_16},
          {"icon32.png", 32, IDR_FILE_MANAGER_ICON_32},
          {"icon48.png", 48, IDR_FILE_MANAGER_ICON_48},
          {"icon64.png", 64, IDR_FILE_MANAGER_ICON_64},
          {"icon128.png", 128, IDR_FILE_MANAGER_ICON_128},
          {"icon192.png", 192, IDR_FILE_MANAGER_ICON_192},
          {"icon256.png", 256, IDR_FILE_MANAGER_ICON_256},
      },
      *info);

  auto* color_provider = ash::AshColorProvider::Get();
  info->theme_color =
      color_provider->GetBackgroundColorInMode(/*use_dark_color=*/false);
  info->dark_mode_theme_color =
      color_provider->GetBackgroundColorInMode(/*use_dark_color=*/true);
  info->background_color = info->theme_color;
  info->dark_mode_background_color = info->dark_mode_theme_color;
  info->display_mode = blink::mojom::DisplayMode::kStandalone;
  info->user_display_mode = blink::mojom::DisplayMode::kStandalone;

  // Add File Handlers. NOTE: Order of handlers matters.
  // Archives:
  AppendFileHandler(*info, "mount-archive",
                    {"7z", "bz2", "crx", "gz", "iso", "rar", "tar", "tbz",
                     "tbz2", "tgz", "zip"});

  // Drive & Google Docs:
  AppendFileHandler(*info, "open-hosted-generic",
                    {"gdraw", "gtable", "gform", "gmaps", "gsite", "glink"});
  AppendFileHandler(*info, "open-hosted-gdoc", {"gdoc"});
  AppendFileHandler(*info, "open-hosted-gsheet", {"gsheet"});
  AppendFileHandler(*info, "open-hosted-gslides", {"gslides"});

  // Drive & Office Docs:
  AppendFileHandler(*info,
                    ::file_manager::file_tasks::kActionIdWebDriveOfficeWord,
                    {"doc", "docx"});
  AppendFileHandler(*info,
                    ::file_manager::file_tasks::kActionIdWebDriveOfficeExcel,
                    {"xls", "xlsx"});
  AppendFileHandler(
      *info, ::file_manager::file_tasks::kActionIdWebDriveOfficePowerPoint,
      {"ppt", "pptx"});

  // View in the browser (with mime-type):
  AppendFileHandler(*info, "view-pdf", {"pdf"}, "application/pdf");
  AppendFileHandler(
      *info, "view-in-browser",
      {"htm", "html", "mht", "mhtml", "shtml", "xht", "xhtml", "svg", "txt"},
      "text/plain");

  // Crostini:
  AppendFileHandler(*info, "install-linux-package", {"deb"});
  AppendFileHandler(*info, "import-crostini-image", {"tini"});

  // For File Picker and Save As dialogs:
  AppendFileHandler(*info, "select", {"*"});
  AppendFileHandler(*info, "open", {"*"});
  return info;
}

FileManagerSystemAppDelegate::FileManagerSystemAppDelegate(Profile* profile)
    : web_app::SystemWebAppDelegate(
          web_app::SystemAppType::FILE_MANAGER,
          "File Manager",
          GURL(kChromeUIFileManagerURL),
          profile,
          web_app::OriginTrialsMap(
              {{web_app::GetOrigin(kChromeUIFileManagerURL),
                {"FileHandling"}}})) {}

std::unique_ptr<WebAppInstallInfo> FileManagerSystemAppDelegate::GetWebAppInfo()
    const {
  return CreateWebAppInfoForFileManager();
}

bool FileManagerSystemAppDelegate::ShouldCaptureNavigations() const {
  return true;
}

bool FileManagerSystemAppDelegate::ShouldReuseExistingWindow() const {
  return false;
}

bool FileManagerSystemAppDelegate::IsAppEnabled() const {
  return ash::features::IsFileManagerSwaEnabled();
}

bool FileManagerSystemAppDelegate::ShouldShowNewWindowMenuOption() const {
  return true;
}

std::vector<web_app::AppId>
FileManagerSystemAppDelegate::GetAppIdsToUninstallAndReplace() const {
  if (ash::features::IsFileManagerSwaEnabled()) {
    return {extension_misc::kFilesManagerAppId};
  }
  return {};
}
