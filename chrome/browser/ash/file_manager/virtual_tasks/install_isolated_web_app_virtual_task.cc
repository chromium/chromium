// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_manager/virtual_tasks/install_isolated_web_app_virtual_task.h"

#include <string>
#include <vector>

#include "ash/webui/file_manager/url_constants.h"
#include "base/strings/strcat.h"
#include "chrome/browser/ash/file_manager/file_tasks.h"
#include "chrome/browser/ash/file_manager/virtual_tasks/id_constants.h"
#include "chrome/browser/ash/fileapi/file_system_backend.h"
#include "chrome/browser/ash/fusebox/fusebox_server.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_features.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_installation_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "components/services/app_service/public/cpp/intent_util.h"
#include "content/public/browser/isolated_web_apps_policy.h"
#include "storage/browser/file_system/file_system_url.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/strings/grit/ui_chromeos_strings.h"
#include "url/gurl.h"

namespace file_manager::file_tasks {

InstallIsolatedWebAppVirtualTask::InstallIsolatedWebAppVirtualTask() {
  matcher_file_extensions_ = {"swbn"};
}

bool InstallIsolatedWebAppVirtualTask::IsEnabled(Profile* profile) const {
  return web_app::IsIwaUnmanagedInstallEnabled(profile);
}

std::string InstallIsolatedWebAppVirtualTask::id() const {
  return base::StrCat({ash::file_manager::kChromeUIFileManagerURL, "?",
                       kActionIdInstallIsolatedWebApp});
}

std::string InstallIsolatedWebAppVirtualTask::title() const {
  return l10n_util::GetStringUTF8(
      IDS_FILE_BROWSER_TASK_INSTALL_ISOLATED_WEB_APP);
}

GURL InstallIsolatedWebAppVirtualTask::icon_url() const {
  // This gets overridden in file_tasks.ts.
  // TODO(crbug.com/40280769): Specify the icon here instead of overriding it.
  return GURL();
}

bool InstallIsolatedWebAppVirtualTask::IsDlpBlocked(
    const std::vector<std::string>& dlp_source_urls) const {
  return false;
}

bool InstallIsolatedWebAppVirtualTask::Execute(
    Profile* profile,
    const TaskDescriptor& task,
    const std::vector<storage::FileSystemURL>& file_urls) const {
  if (file_urls.empty()) {
    return false;
  }

  auto* web_app_provider = web_app::WebAppProvider::GetForWebApps(profile);
  if (!web_app_provider) {
    return false;
  }
  for (const FileSystemURL& file_url : file_urls) {
    base::FilePath path = file_url.path();
    // VirtualTask::Match should return false if multiple files with different
    // extensions were selected. `AsUTF8Unsafe()` is safe on ChromeOS.
    DCHECK(apps_util::ExtensionMatched(path.BaseName().AsUTF8Unsafe(),
                                       matcher_file_extensions_[0]));
    web_app_provider->ui_manager().LaunchOrFocusIsolatedWebAppInstaller(
        file_url.path());
  }
  return true;
}

}  // namespace file_manager::file_tasks
