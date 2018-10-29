// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_manager/open_with_browser.h"

#include <stddef.h>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "base/task/post_task.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/drive/drive_integration_service.h"
#include "chrome/browser/chromeos/file_manager/filesystem_api_util.h"
#include "chrome/browser/chromeos/fileapi/external_file_url_util.h"
#include "chrome/browser/plugins/plugin_prefs.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/scoped_tabbed_browser_displayer.h"
#include "chrome/common/chrome_content_client.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chromeos/components/drivefs/mojom/drivefs.mojom.h"
#include "components/drive/drive_api_util.h"
#include "components/drive/file_system_core_util.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/plugin_service.h"
#include "content/public/common/pepper_plugin_info.h"
#include "net/base/filename_util.h"

using content::BrowserThread;
using content::PluginService;

namespace file_manager {
namespace util {
namespace {

const base::FilePath::CharType kPdfExtension[] = FILE_PATH_LITERAL(".pdf");
const base::FilePath::CharType kSwfExtension[] = FILE_PATH_LITERAL(".swf");

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
};

// Returns true if |file_path| is viewable in the browser (ex. HTML file).
bool IsViewableInBrowser(const base::FilePath& file_path) {
  for (size_t i = 0; i < arraysize(kFileExtensionsViewableInBrowser); i++) {
    if (file_path.MatchesExtension(kFileExtensionsViewableInBrowser[i]))
      return true;
  }
  return false;
}

bool IsPepperPluginEnabled(Profile* profile,
                           const base::FilePath& plugin_path) {
  DCHECK(profile);

  const content::PepperPluginInfo* pepper_info =
      PluginService::GetInstance()->GetRegisteredPpapiPluginInfo(plugin_path);
  if (!pepper_info)
    return false;

  scoped_refptr<PluginPrefs> plugin_prefs = PluginPrefs::GetForProfile(profile);
  if (!plugin_prefs.get())
    return false;

  return plugin_prefs->IsPluginEnabled(pepper_info->ToWebPluginInfo());
}

bool IsPdfPluginEnabled(Profile* profile) {
  DCHECK(profile);

  static const base::FilePath plugin_path(ChromeContentClient::kPDFPluginPath);
  return IsPepperPluginEnabled(profile, plugin_path);
}

bool IsFlashPluginEnabled(Profile* profile) {
  DCHECK(profile);

  base::FilePath plugin_path(
      base::CommandLine::ForCurrentProcess()->GetSwitchValueNative(
          switches::kPpapiFlashPath));
  if (plugin_path.empty())
    base::PathService::Get(chrome::FILE_PEPPER_FLASH_PLUGIN, &plugin_path);
  return IsPepperPluginEnabled(profile, plugin_path);
}

void OpenNewTab(Profile* profile, const GURL& url) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Check the validity of the pointer so that the closure from
  // base::Bind(&OpenNewTab, profile) can be passed between threads.
  if (!g_browser_process->profile_manager()->IsValidProfile(profile))
    return;

  chrome::ScopedTabbedBrowserDisplayer displayer(profile);
  chrome::AddSelectedTabWithURL(displayer.browser(), url,
      ui::PAGE_TRANSITION_LINK);

  // Since the ScopedTabbedBrowserDisplayer does not guarantee that the
  // browser will be shown on the active desktop, we ensure the visibility.
  multi_user_util::MoveWindowToCurrentDesktop(
      displayer.browser()->window()->GetNativeWindow());
}

// Reads the alternate URL from a GDoc file. When it fails, returns a file URL
// for |file_path| as fallback.
// Note that an alternate url is a URL to open a hosted document.
GURL ReadUrlFromGDocAsync(const base::FilePath& file_path) {
  GURL url = drive::util::ReadUrlFromGDocFile(file_path);
  if (url.is_empty())
    url = net::FilePathToFileURL(file_path);
  return url;
}

// Parse a local file to extract the Docs url and open this url.
void OpenGDocUrlFromFile(const base::FilePath& file_path, Profile* profile) {
  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&ReadUrlFromGDocAsync, file_path),
      base::BindOnce(&OpenNewTab, profile));
}

// Open a hosted GDoc, from a path hosted in DriveFS.
void OpenHostedDriveFsFile(const base::FilePath& file_path,
                           Profile* profile,
                           drive::FileError error,
                           drivefs::mojom::FileMetadataPtr metadata) {
  if (error != drive::FILE_ERROR_OK)
    return;
  if (metadata->type != drivefs::mojom::FileMetadata::Type::kHosted) {
    OpenGDocUrlFromFile(file_path, profile);
    return;
  }
  GURL hosted_url(metadata->alternate_url);
  if (!hosted_url.is_valid())
    return;

  OpenNewTab(profile, hosted_url);
}

}  // namespace

bool OpenFileWithBrowser(Profile* profile,
                         const storage::FileSystemURL& file_system_url) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(profile);

  const base::FilePath file_path = file_system_url.path();

  // For things supported natively by the browser, we should open it
  // in a tab.
  if (IsViewableInBrowser(file_path) ||
      ShouldBeOpenedWithPlugin(profile, file_path.Extension())) {
    // Use external file URL if it is provided for the file system.
    GURL page_url = chromeos::FileSystemURLToExternalFileURL(file_system_url);
    if (page_url.is_empty())
      page_url = net::FilePathToFileURL(file_path);

    OpenNewTab(profile, page_url);
    return true;
  }

  if (drive::util::HasHostedDocumentExtension(file_path)) {
    if (file_manager::util::IsUnderNonNativeLocalPath(profile, file_path)) {
      // The file is on a non-native volume. Use external file URL. If the file
      // is on the drive volume, ExternalFileURLRequestJob redirects the URL to
      // drive's web interface. Otherwise (e.g. MTP, FSP), the file is just
      // downloaded in a browser tab.
      const GURL url =
          chromeos::FileSystemURLToExternalFileURL(file_system_url);
      DCHECK(!url.is_empty());
      OpenNewTab(profile, url);
    } else {
      drive::DriveIntegrationService* integration_service =
          drive::DriveIntegrationServiceFactory::FindForProfile(profile);
      base::FilePath path;
      if (integration_service && integration_service->IsMounted() &&
          integration_service->GetDriveFsInterface() &&
          integration_service->GetRelativeDrivePath(file_path, &path)) {
        integration_service->GetDriveFsInterface()->GetMetadata(
            path, base::BindOnce(&OpenHostedDriveFsFile, file_path, profile));
        return true;
      }
      OpenGDocUrlFromFile(file_path, profile);
    }
    return true;
  }

  // Failed to open the file of unknown type.
  LOG(WARNING) << "Unknown file type: " << file_path.value();
  return false;
}

// If a bundled plugin is enabled, we should open pdf/swf files in a tab.
bool ShouldBeOpenedWithPlugin(
    Profile* profile,
    const base::FilePath::StringType& file_extension) {
  DCHECK(profile);

  const base::FilePath file_path =
      base::FilePath::FromUTF8Unsafe("dummy").AddExtension(file_extension);
  if (file_path.MatchesExtension(kPdfExtension))
    return IsPdfPluginEnabled(profile);
  if (file_path.MatchesExtension(kSwfExtension))
    return IsFlashPluginEnabled(profile);
  return false;
}

}  // namespace util
}  // namespace file_manager
