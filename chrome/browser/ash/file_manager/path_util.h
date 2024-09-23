// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILE_MANAGER_PATH_UTIL_H_
#define CHROME_BROWSER_ASH_FILE_MANAGER_PATH_UTIL_H_

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "chrome/browser/ash/guest_os/guest_id.h"
#include "storage/browser/file_system/file_system_url.h"

class GURL;
class Profile;

namespace base {
class Pickle;
}  // namespace base

namespace ui {
class DataTransferEndpoint;
struct FileInfo;
}  // namespace ui

namespace file_manager::util {

// Absolute path for FuseBox media mount point (sans a trailing slash).
extern const base::FilePath::CharType kFuseBoxMediaPath[];

// Absolute path for FuseBox media mount point (with a trailing slash).
extern const base::FilePath::CharType kFuseBoxMediaSlashPath[];

// Absolute base path for removable media on Chrome OS. Exposed here so it can
// be used by tests.
extern const base::FilePath::CharType kRemovableMediaPath[];

// Absolute path for the folder containing Android files.
extern const base::FilePath::CharType kAndroidFilesPath[];

// Absolute path for the folder containing font files.
extern const base::FilePath::CharType kSystemFontsPath[];

// Absolute path for the folder containing archive mounts.
extern const base::FilePath::CharType kArchiveMountPath[];

// FuseBox as a named constant string: "fusebox".
extern const char kFuseBox[];

// The storage::FileSystemURL mount name prefix for FuseBox mounts.
extern const char kFuseBoxMountNamePrefix[];

// The "foo." in "/media/fuse/fusebox/foo.bar/x/y.z" FuseBox filenames, per
// volume type (Android Documents Provider, File System Provider, Media
// Transfer Protocol, etc). The "foo.bar" component as a whole is also known as
// the FuseBox subdir.
//
// They end in a "." to clearly separate the "foo" and the "bar". This is not a
// "-" or a "_", to avoid any ambiguity when "bar" is the base64url encoding of
// something. This is not a ":", since /bin/bash tab completion would otherwise
// backslash-escape the colon (which works but it's avoidable complexity) and
// e.g. $PATH-like environment variables are colon separated.
extern const char kFuseBoxSubdirPrefixADP[];
extern const char kFuseBoxSubdirPrefixFSP[];
extern const char kFuseBoxSubdirPrefixLOC[];
extern const char kFuseBoxSubdirPrefixMTP[];
extern const char kFuseBoxSubdirPrefixTMP[];

// Name of the mount point used to store temporary files for sharing.
extern const char kShareCacheMountPointName[];

// Returns the valid FilesApp origin. It may be either the System Web App
// chrome:// URL or the legacy Chrome App chrome-extension:// URL.
const url::Origin& GetFilesAppOrigin();

// Gets the absolute path for the 'Downloads' folder for the |profile|.
base::FilePath GetDownloadsFolderForProfile(Profile* profile);

// Gets the absolute path for the 'MyFiles' folder for the |profile|.
base::FilePath GetMyFilesFolderForProfile(Profile* profile);

// Gets the absolute path for the user's Android Play files (Movies, Pictures,
// etc..., Android apps excluded). The default path may be overridden by tests.
base::FilePath GetAndroidFilesPath();

// Gets the absolute path for the user's Share Cache directory, which is used
// to store temporary files being shared from one app to another.
base::FilePath GetShareCacheFilePath(Profile* profile);

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
// TODO(crbug.com/41430020) Remove this when no users are running M72 or
// earlier.
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
std::string GetAndroidFilesMountPointName();

// The canonical mount point name for crostini "Linux files" folder.
std::string GetCrostiniMountPointName(Profile* profile);

// The canonical mount point name for the Guest OS `id`.
std::string GetGuestOsMountPointName(Profile* profile,
                                     const guest_os::GuestId& id);

// The actual directory the crostini "Linux files" folder is mounted.
base::FilePath GetCrostiniMountDirectory(Profile* profile);

// The actual directory the Guest OS with `mountPointName` is mounted in.
base::FilePath GetGuestOsMountDirectory(std::string mountPointName);

// Convert a cracked |file_system_url| to a path inside a VM mounted at
// |vm_mount| (e.g. /mnt/chromeos). If |map_crostini_home| is set, paths under
// GetCrostiniMountDirectory() are translated to be under the user's home
// directory (e.g. /home/user) otherwise these paths map to
// |vm_mount|/LinuxFiles. This function is the reverse of
// ConvertPathInsideVMToFileSystemURL(). Returns true iff path can be converted.
bool ConvertFileSystemURLToPathInsideVM(
    Profile* profile,
    const storage::FileSystemURL& file_system_url,
    const base::FilePath& vm_mount,
    bool map_crostini_home,
    base::FilePath* inside);

// Convert a cracked url to a path inside the Crostini VM.
bool ConvertFileSystemURLToPathInsideCrostini(
    Profile* profile,
    const storage::FileSystemURL& file_system_url,
    base::FilePath* inside);

// Convert a Fusebox Moniker path to a path inside VM mounted at `vm_mount`.
// `inside` is modified only when the return value is true (success).
bool ConvertFuseboxMonikerPathToPathInsideVM(const base::FilePath& path,
                                             const base::FilePath& vm_mount,
                                             base::FilePath* inside);

// Convert a path inside a VM mounted at |vm_mount| (e.g. /mnt/chromeos) to a
// FileSystemURL. If |map_crostini_home| is set, paths
// under the user's home directory (e.g. /home/user) are translated to be under
// GetCrostiniMountDirectory(). This function is the reverse of
// ConvertFileSystemURLToPathInsideVM(). Returns true iff path can be converted.
bool ConvertPathInsideVMToFileSystemURL(
    Profile* profile,
    const base::FilePath& inside,
    const base::FilePath& vm_mount,
    bool map_crostini_home,
    storage::FileSystemURL* file_system_url);

// DEPRECATED. Use |ConvertToContentUrls| instead.
// While this function can convert paths under Downloads, /media/removable
// and /special/drive, this CANNOT convert paths under ARC media directories
// (/special/arc-documents-provider).
// TODO(crbug.com/811679): Migrate all callers and remove this.
// |*requires_sharing_out| will be set to true if |path| needs to be made
// available to ARCVM by sharing via Seneschal.
// Precondition: arc_url_out != nullptr
// Precondition: requires_sharing_out != nullptr
bool ConvertPathToArcUrl(const base::FilePath& path,
                         GURL* arc_url_out,
                         bool* requires_sharing_out);

// |paths_to_share| is a list of paths to be made available to ARCVM by sharing
// them via Seneschal.
using ConvertToContentUrlsCallback =
    base::OnceCallback<void(const std::vector<GURL>& content_urls,
                            const std::vector<base::FilePath>& paths_to_share)>;

// Converts the given FileSystemURL to a file path which can be passed to
// ConvertPathToArcUrl().
base::FilePath ConvertFileSystemURLToPathForSharingWithArc(
    const storage::FileSystemURL& file_system_url);

// Asynchronously converts Chrome OS file system URLs to content:// URLs.
// Always returns a vector of the same size as |file_system_urls|.
// Empty GURLs are filled in the vector if conversion fails.
void ConvertToContentUrls(
    Profile* profile,
    const std::vector<storage::FileSystemURL>& file_system_urls,
    ConvertToContentUrlsCallback callback);

// Replace `prefix` with `replacement` at the beginning of `*s`.
bool ReplacePrefix(std::string* s,
                   std::string_view prefix,
                   std::string_view replacement);

// Convert path into a string suitable for display in settings.
// Replacement examples:
// * /home/chronos/user/MyFiles                  => My files
// * /home/chronos/u-<hash>/MyFiles              => My files
// * /media/fuse/drivefs-<hash>/root             => Google Drive › My Drive
// * /media/fuse/drivefs-<hash>/team_drives      => Google Drive › Team Drives
// * /media/fuse/drivefs-<hash>/Computers        => Google Drive › Computers
// * /run/arc/sdcard/write/emulated/0            => Play files
// * /media/fuse/crostini_<hash>_termina_penguin => Linux files
// * /media/archive/<id>                         => <id>
// * /media/removable/<id>                       => <id>
// * '/' with ' › ' (angled quote sign) for display purposes.
std::string GetPathDisplayTextForSettings(Profile* profile,
                                          std::string_view path);

// Extracts |mount_name|, |file_system_name|, and |full_path| from given
// |absolute_path|.
bool ExtractMountNameFileSystemNameFullPath(const base::FilePath& absolute_path,
                                            std::string* mount_name,
                                            std::string* file_system_name,
                                            std::string* full_path);

// Extracts the file/directory name from the URL and unescape to convert %20 to
// space.
std::string GetDisplayableFileName(GURL file_url);
std::string GetDisplayableFileName(storage::FileSystemURL file_url);
std::u16string GetDisplayableFileName16(GURL file_url);
std::u16string GetDisplayableFileName16(storage::FileSystemURL file_url);

// Turns an absolute path into one suitable for display. Returns nullopt if the
// given path is invalid or not on a mounted volume.
std::optional<base::FilePath> GetDisplayablePath(Profile* profile,
                                                 base::FilePath path);
std::optional<base::FilePath> GetDisplayablePath(
    Profile* profile,
    storage::FileSystemURL file_url);

// Reads pickle for FilesApp fs/sources with newline-separated filesystem
// URLs. Validates that |source| is FilesApp.
std::vector<ui::FileInfo> ParseFileSystemSources(
    const ui::DataTransferEndpoint* source,
    const base::Pickle& pickle);

}  // namespace file_manager::util

#endif  // CHROME_BROWSER_ASH_FILE_MANAGER_PATH_UTIL_H_
