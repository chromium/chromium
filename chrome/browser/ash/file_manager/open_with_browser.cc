// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ash/file_manager/open_with_browser.h"

#include <stddef.h>

#include "ash/constants/web_app_id_constants.h"
#include "ash/public/cpp/new_window_delegate.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/path_service.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/launch_result_type.h"
#include "chrome/browser/ash/drive/drive_integration_service.h"
#include "chrome/browser/ash/file_manager/file_tasks.h"
#include "chrome/browser/ash/file_manager/filesystem_api_util.h"
#include "chrome/browser/ash/file_manager/office_file_tasks.h"
#include "chrome/browser/ash/fileapi/external_file_url_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/webui/ash/cloud_upload/cloud_upload_util.h"
#include "chrome/browser/ui/webui/ash/cloud_upload/hats_office_trigger.h"
#include "chrome/common/chrome_content_client.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chromeos/ash/components/drivefs/drivefs_util.h"
#include "chromeos/ash/components/drivefs/mojom/drivefs.mojom.h"
#include "components/drive/drive_api_util.h"
#include "components/drive/file_system_core_util.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/filename_util.h"
#include "pdf/buildflags.h"
#include "storage/browser/file_system/file_system_url.h"

using content::BrowserThread;

namespace file_manager::util {
namespace {

// List of file extensions viewable in the browser.
constexpr const base::FilePath::CharType* kFileExtensionsViewableInBrowser[] = {
    FILE_PATH_LITERAL(".bmp"),   FILE_PATH_LITERAL(".ico"),
    FILE_PATH_LITERAL(".jpg"),   FILE_PATH_LITERAL(".jpeg"),
    FILE_PATH_LITERAL(".png"),   FILE_PATH_LITERAL(".webp"),
    FILE_PATH_LITERAL(".gif"),   FILE_PATH_LITERAL(".txt"),
    FILE_PATH_LITERAL(".html"),  FILE_PATH_LITERAL(".htm"),
    FILE_PATH_LITERAL(".mhtml"), FILE_PATH_LITERAL(".mht"),
    FILE_PATH_LITERAL(".xhtml"), FILE_PATH_LITERAL(".xht"),
    FILE_PATH_LITERAL(".shtml"), FILE_PATH_LITERAL(".svg"),
#if BUILDFLAG(ENABLE_PDF)
    FILE_PATH_LITERAL(".pdf"),
#endif  // BUILDFLAG(ENABLE_PDF)
};

// Returns true if |file_path| is viewable in the browser (ex. HTML file).
bool IsViewableInBrowser(const base::FilePath& file_path) {
  for (size_t i = 0; i < std::size(kFileExtensionsViewableInBrowser); i++) {
    if (file_path.MatchesExtension(kFileExtensionsViewableInBrowser[i])) {
      return true;
    }
  }
  return false;
}

bool OpenNewTab(const GURL& url) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!ash::NewWindowDelegate::GetPrimary()) {
    return false;
  }
  ash::NewWindowDelegate::GetPrimary()->OpenUrl(
      url, ash::NewWindowDelegate::OpenUrlFrom::kUserInteraction,
      ash::NewWindowDelegate::Disposition::kNewForegroundTab);
  return true;
}

std::optional<std::string> GetAppIdFromFilePath(
    const GURL& url,
    const base::FilePath& file_path) {
  if (url.SchemeIsFile()) {
    return std::nullopt;
  }
  const std::string& file_extension = file_path.FinalExtension();
  if (file_extension == ".gdoc" ||
      file_tasks::WordGroupExtensions().contains(file_extension)) {
    return web_app::kGoogleDocsAppId;
  } else if (file_extension == ".gsheet" ||
             file_tasks::ExcelGroupExtensions().contains(file_extension)) {
    return web_app::kGoogleSheetsAppId;
  } else if (file_extension == ".gslides" ||
             file_tasks::PowerPointGroupExtensions().contains(file_extension)) {
    return web_app::kGoogleSlidesAppId;
  }
  return std::nullopt;
}

// Reads the alternate URL from a GDoc file. When it fails, returns a file URL
// for |file_path| as fallback.
// Note that an alternate url is a URL to open a hosted document.
GURL ReadUrlFromGDocAsync(const base::FilePath& file_path) {
  GURL url = drive::util::ReadUrlFromGDocFile(file_path);
  if (url.is_empty()) {
    url = net::FilePathToFileURL(file_path);
  }
  return url;
}

// Parse a local file to extract the Docs url and open this url.
void OpenGDocUrlFromFile(Profile* profile,
                         const base::FilePath& file_path,
                         LaunchAppCallback callback) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&ReadUrlFromGDocAsync, file_path),
      base::BindOnce(base::IgnoreResult(&OpenHostedFileInNewTabOrApp), profile,
                     file_path, std::move(callback)));
}

// Open a hosted GDoc, from a path hosted in DriveFS.
void OpenHostedDriveFsFile(Profile* profile,
                           const base::FilePath& file_path,
                           LaunchAppCallback callback,
                           drive::FileError error,
                           drivefs::mojom::FileMetadataPtr metadata) {
  if (error != drive::FILE_ERROR_OK) {
    return;
  }
  if (drivefs::IsLocal(metadata->type)) {
    OpenGDocUrlFromFile(profile, file_path, std::move(callback));
    return;
  }
  GURL hosted_url(metadata->alternate_url);
  if (!hosted_url.is_valid()) {
    return;
  }

  OpenHostedFileInNewTabOrApp(profile, file_path, std::move(callback),
                              hosted_url);
}

// Open an encrypted file by redirecting the user to Google Drive.
void OpenEncryptedDriveFsFile(const base::FilePath& file_path,
                              drive::FileError error,
                              drivefs::mojom::FileMetadataPtr metadata) {
  if (error != drive::FILE_ERROR_OK) {
    return;
  }
  GURL hosted_url(metadata->alternate_url);
  if (!hosted_url.is_valid()) {
    return;
  }

  OpenNewTab(hosted_url);
}

}  // namespace

bool OpenHostedFileInNewTabOrApp(Profile* profile,
                                 const base::FilePath& file_path,
                                 LaunchAppCallback callback,
                                 const GURL& hosted_url) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  std::optional<std::string> app_id =
      GetAppIdFromFilePath(hosted_url, file_path);
  if (!app_id.has_value()) {
    std::move(callback).Run(std::nullopt);
    return OpenNewTab(hosted_url);
  } else if (base::FeatureList::IsEnabled(
                 ::features::kHappinessTrackingOffice) &&
             file_tasks::IsOfficeFile(file_path)) {
    ash::cloud_upload::HatsOfficeTrigger::Get().ShowSurveyAfterAppInactive(
        app_id.value(), ash::cloud_upload::HatsOfficeLaunchingApp::kDrive);
  }
  apps::AppServiceProxy* app_service =
      apps::AppServiceProxyFactory::GetForProfile(profile);
  DCHECK(app_service);
  const apps::AppRegistryCache& cache = app_service->AppRegistryCache();
  bool is_app_available = false;
  cache.ForOneApp(
      app_id.value(), [&is_app_available](const apps::AppUpdate& update) {
        is_app_available = update.Readiness() == apps::Readiness::kReady;
      });

  if (!is_app_available) {
    std::move(callback).Run(std::nullopt);
    return OpenNewTab(hosted_url);
  }

  auto chained_callback =
      base::BindOnce([](apps::LaunchResult&& result) {
        LOG_IF(ERROR, result.state != apps::LaunchResult::State::kSuccess)
            << "Failed to launch hosted file via app despite "
               "it being ready";
        return result.state;
      }).Then(std::move(callback));

  app_service->LaunchAppWithUrl(app_id.value(), ui::EF_NONE, hosted_url,
                                apps::LaunchSource::kFromFileManager, nullptr,
                                std::move(chained_callback));
  return true;
}

bool OpenFileWithAppOrBrowser(Profile* profile,
                              const storage::FileSystemURL& file_system_url,
                              const std::string& action_id,
                              LaunchAppCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(profile);

  const base::FilePath file_path = file_system_url.path();

  if (action_id == "open-encrypted") {
    drive::DriveIntegrationService* integration_service =
        drive::DriveIntegrationServiceFactory::FindForProfile(profile);
    base::FilePath path;
    if (integration_service && integration_service->IsMounted() &&
        integration_service->GetDriveFsInterface() &&
        integration_service->GetRelativeDrivePath(file_path, &path)) {
      integration_service->GetDriveFsInterface()->GetMetadata(
          path, base::BindOnce(&OpenEncryptedDriveFsFile, file_path));
      return true;
    }
    LOG(WARNING) << "Failed to open file: " << file_path.value()
                 << ": no connection to integration service";
    return false;
  }

  // For things supported natively by the browser, we should open it in a tab.
  if (IsViewableInBrowser(file_path) || action_id == "view-pdf" ||
      (action_id == "view-in-browser" && file_path.Extension() == "")) {
    // Use external file URL if it is provided for the file system.
    GURL page_url = ash::FileSystemURLToExternalFileURL(file_system_url);
    if (page_url.is_empty()) {
      page_url = net::FilePathToFileURL(file_path);
    }

    OpenNewTab(page_url);
    return true;
  }

  if (drive::util::HasHostedDocumentExtension(file_path)) {
    if (file_manager::util::IsUnderNonNativeLocalPath(profile, file_path)) {
      // The file is on a non-native volume. Use external file URL. If the file
      // is on the drive volume, ExternalFileURLRequestJob redirects the URL to
      // drive's web interface. Otherwise (e.g. MTP, FSP), the file is just
      // downloaded in a browser tab.
      const GURL url = ash::FileSystemURLToExternalFileURL(file_system_url);
      DCHECK(!url.is_empty());
      OpenNewTab(url);
    } else {
      drive::DriveIntegrationService* integration_service =
          drive::DriveIntegrationServiceFactory::FindForProfile(profile);
      base::FilePath path;
      if (integration_service && integration_service->IsMounted() &&
          integration_service->GetDriveFsInterface() &&
          integration_service->GetRelativeDrivePath(file_path, &path)) {
        integration_service->GetDriveFsInterface()->GetMetadata(
            path, base::BindOnce(&OpenHostedDriveFsFile, profile, file_path,
                                 std::move(callback)));
        return true;
      }
      OpenGDocUrlFromFile(profile, file_path, std::move(callback));
    }
    return true;
  }

  // Failed to open the file of unknown type.
  LOG(WARNING) << "Unknown file type: " << file_path.value();
  return false;
}

}  // namespace file_manager::util
