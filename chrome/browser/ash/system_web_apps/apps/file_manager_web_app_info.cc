// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_web_apps/apps/file_manager_web_app_info.h"

#include <string>

#include "ash/webui/file_manager/resources/grit/file_manager_swa_resources.h"
#include "ash/webui/file_manager/url_constants.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/file_manager/office_file_tasks.h"
#include "chrome/browser/ash/system_web_apps/apps/system_web_app_install_utils.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
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

// Devices we use for testing (eve) have default width 1200px. We will set min
// width to 596px to still allow snapping on those devices in tablet mode.
const int kFileManagerMinimumWidth = 596;
const int kFileManagerMinimumHeight = 240;

// Appends a file handler to `info`.
// The handler action has the format: chrome://file-manager/?${ACTION_NAME}
// This means: For files with the given `file_extensions` or `mime_type` the
// Files SWA is a candidate app to open/handle such files.
void AppendFileHandler(web_app::WebAppInstallInfo& info,
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
  if (!mime_type.empty()) {
    handler.accept.back().mime_type = mime_type;
  }

  info.file_handlers.push_back(std::move(handler));
}

}  // namespace

std::unique_ptr<web_app::WebAppInstallInfo> CreateWebAppInfoForFileManager() {
  GURL start_url(kChromeUIFileManagerURL);
  auto info =
      web_app::CreateSystemWebAppInstallInfoWithStartUrlAsIdentity(start_url);
  info->scope = GURL(kChromeUIFileManagerURL);
  info->title = l10n_util::GetStringUTF16(IDS_FILEMANAGER_APP_NAME);
  web_app::CreateIconInfoForSystemWebApp(
      info->start_url(),
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

  info->theme_color = cros_styles::ResolveColor(
      cros_styles::ColorName::kBgColor, /*is_dark_mode=*/false);
  info->dark_mode_theme_color = cros_styles::ResolveColor(
      cros_styles::ColorName::kBgColor, /*is_dark_mode=*/true);
  info->background_color = info->theme_color;
  info->dark_mode_background_color = info->dark_mode_theme_color;
  info->display_mode = blink::mojom::DisplayMode::kStandalone;
  info->user_display_mode = web_app::mojom::UserDisplayMode::kStandalone;

  // Add File Handlers. NOTE: Order of handlers matters.
  // Archives:
  AppendFileHandler(*info, "mount-archive",
                    {
                        "7z",     //
                        "bz",     //
                        "bz2",    //
                        "crx",    //
                        "gz",     //
                        "iso",    //
                        "lz",     //
                        "lzma",   //
                        "rar",    //
                        "tar",    //
                        "taz",    //
                        "tb2",    //
                        "tbz",    //
                        "tbz2",   //
                        "tgz",    //
                        "tlz",    //
                        "tlzma",  //
                        "txz",    //
                        "tz",     //
                        "tz2",    //
                        "tzst",   //
                        "xz",     //
                        "z",      //
                        "zip",    //
                        "zst",    //
                    });

  // Drive & Google Docs:
  AppendFileHandler(
      *info, "open-hosted-generic",
      {"gdraw", "gtable", "gform", "gmaps", "gsite", "glink", "gmaillayout"});
  AppendFileHandler(*info, "open-hosted-gdoc", {"gdoc"});
  AppendFileHandler(*info, "open-hosted-gsheet", {"gsheet"});
  AppendFileHandler(*info, "open-hosted-gslides", {"gslides"});

  // View in the browser (with mime-type):
  AppendFileHandler(*info, "view-pdf", {"pdf"}, "application/pdf");
  AppendFileHandler(
      *info, "view-in-browser",
      {"htm", "html", "mht", "mhtml", "shtml", "xht", "xhtml", "svg", "txt"},
      "text/plain");

  // Crostini:
  AppendFileHandler(*info, "install-linux-package", {"deb"});
  AppendFileHandler(*info, "import-crostini-image", {"tini"});

  return info;
}

FileManagerSystemAppDelegate::FileManagerSystemAppDelegate(Profile* profile)
    : ash::SystemWebAppDelegate(ash::SystemWebAppType::FILE_MANAGER,
                                "File Manager",
                                GURL(kChromeUIFileManagerURL),
                                profile) {}

std::unique_ptr<web_app::WebAppInstallInfo>
FileManagerSystemAppDelegate::GetWebAppInfo() const {
  return CreateWebAppInfoForFileManager();
}

bool FileManagerSystemAppDelegate::ShouldCaptureNavigations() const {
  return true;
}

Browser* FileManagerSystemAppDelegate::GetWindowForLaunch(
    Profile* profile,
    const GURL& url) const {
  return nullptr;
}

bool FileManagerSystemAppDelegate::IsAppEnabled() const {
  return true;
}

bool FileManagerSystemAppDelegate::ShouldShowNewWindowMenuOption() const {
  return true;
}

std::vector<std::string>
FileManagerSystemAppDelegate::GetAppIdsToUninstallAndReplace() const {
  return {extension_misc::kFilesManagerAppId};
}

gfx::Size FileManagerSystemAppDelegate::GetMinimumWindowSize() const {
  return {kFileManagerMinimumWidth, kFileManagerMinimumHeight};
}
