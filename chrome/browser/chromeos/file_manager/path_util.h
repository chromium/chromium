// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_FILE_MANAGER_PATH_UTIL_H_
#define CHROME_BROWSER_CHROMEOS_FILE_MANAGER_PATH_UTIL_H_

#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "storage/browser/file_system/file_system_url.h"

class GURL;
class Profile;

namespace file_manager {
namespace util {

// Absolute base path for removable media on Chrome OS. Exposed here so it can
// be used by tests.
extern const base::FilePath::CharType kRemovableMediaPath[];

// Absolute path for the folder containing Android files.
extern const base::FilePath::CharType kAndroidFilesPath[];

// Gets the absolute path for the 'Downloads' folder for the |profile|.
base::FilePath GetDownloadsFolderForProfile(Profile* profile);

// Gets the absolute path for the 'MyFiles' folder for the |profile|.
base::FilePath GetMyFilesFolderForProfile(Profile* profile);

// Converts |old_path| to |new_path| and returns true, if the old path points
// to an old location of user folders (in "Downloads" or "Google Drive").
// The |profile| argument is used for determining the location of the
// "Downloads" folder.
//
// As of now (M40), the conversion is used only during initialization of
// download_prefs, where profile unaware initialization precedes profile
// aware stage. Below are the list of relocations we have made in the past.
// *Updated in M73 to handle /home/chronos/user to
// /home/chronos/u-{hash}/MyFiles/Downloads
//
// M27: crbug.com/229304, for supporting {offline, recent, shared} folders
//   in Drive. Migration code for this is removed in M34.
// M34-35: crbug.com/313539, 356322, for supporting multi profiles.
//   Migration code is removed in M40.
bool MigratePathFromOldFormat(Profile* profile,
                              const base::FilePath& old_base,
                              const base::FilePath& old_path,
                              base::FilePath* new_path);

// Converts |old_path| in <cryptohome>/Downloads[/*] to |new_path| in
// <cryptohome/MyFiles/Downloads[*].  Returns true if path is changed else
// returns false if |old_path| was not inside Downloads, and |new_path| is
// undefined.
//
// Introduced in M73.  This code updates values stored in prefs.
// TODO(crbug.com/911946) Remove this when no users are running M72 or earlier.
bool MigrateFromDownloadsToMyFiles(Profile* profile,
                                   const base::FilePath& old_path,
                                   base::FilePath* new_path);

// Convers |old_path| in /special/drive-<hash> to |new_path| in
// /media/fuse/drivefs-<id>. Returns true if path is changed else
// returns false if |old_path| was not inside Drive, and |new_path| is
// undefined.
bool MigrateToDriveFs(Profile* profile,
                      const base::FilePath& old_path,
                      base::FilePath* new_path);

// The canonical mount point name for "Downloads" folder.
std::string GetDownloadsMountPointName(Profile* profile);

// The canonical mount point name for ARC "Play files" folder.
const std::string GetAndroidFilesMountPointName();

// The canonical mount point name for crostini "Linux files" folder.
std::string GetCrostiniMountPointName(Profile* profile);

// The actual directory the crostini "Linux files" folder is mounted.
base::FilePath GetCrostiniMountDirectory(Profile* profile);

// The sshfs mount options for crostini "Linux files" mount.
std::vector<std::string> GetCrostiniMountOptions(
    const std::string& hostname,
    const std::string& host_private_key,
    const std::string& container_public_key);

// Convert a cracked url to a path inside the Crostini VM.
bool ConvertFileSystemURLToPathInsideCrostini(
    Profile* profile,
    const storage::FileSystemURL& file_system_url,
    base::FilePath* inside);

// DEPRECATED. Use |ConvertToContentUrls| instead.
// While this function can convert paths under Downloads, /media/removable
// and /special/drive, this CANNOT convert paths under ARC media directories
// (/special/arc-documents-provider).
// TODO(crbug.com/811679): Migrate all callers and remove this.
bool ConvertPathToArcUrl(const base::FilePath& path, GURL* arc_url_out);

using ConvertToContentUrlsCallback =
    base::OnceCallback<void(const std::vector<GURL>& content_urls)>;

// Asynchronously converts Chrome OS file system URLs to content:// URLs.
// Always returns a vector of the same size as |file_system_urls|.
// Empty GURLs are filled in the vector if conversion fails.
void ConvertToContentUrls(
    const std::vector<storage::FileSystemURL>& file_system_urls,
    ConvertToContentUrlsCallback callback);

// Convert path into a string suitable for display in settings.
// Replacements:
// * /home/chronos/user/Downloads                => Downloads
// * /home/chronos/u-<hash>/Downloads            => Downloads
// * /media/fuse/drivefs-<hash>/root             => Google Drive
// * /media/fuse/drivefs-<hash>/team_drives      => Team Drives
// * /media/fuse/drivefs-<hash>/Computers        => Computers
// * /run/arc/sdcard/write/emulated/0            => Play files
// * /media/fuse/crostini_<hash>_termina_penguin => Linux files
// * '/' with ' \u203a ' (angled quote sign) for display purposes.
std::string GetPathDisplayTextForSettings(Profile* profile,
                                          const std::string& path);

// Extracts |mount_name|, |file_system_name|, and |full_path| from given
// |absolute_path|.
bool ExtractMountNameFileSystemNameFullPath(const base::FilePath& absolute_path,
                                            std::string* mount_name,
                                            std::string* file_system_name,
                                            std::string* full_path);
}  // namespace util
}  // namespace file_manager

#endif  // CHROME_BROWSER_CHROMEOS_FILE_MANAGER_PATH_UTIL_H_
