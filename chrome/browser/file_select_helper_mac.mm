// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/file_select_helper.h"

#include <Cocoa/Cocoa.h>
#include <sys/stat.h>

#include <string_view>

#include "base/apple/foundation_util.h"
#include "base/files/file.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/zlib/google/zip.h"
#include "ui/shell_dialogs/selected_file_info.h"

namespace {

std::string_view AsStringPiece(NSString* str) {
  const char* data = [str fileSystemRepresentation];
  return data ? std::string_view(data) : std::string_view();
}

// Given the |path| of a package, returns the destination that the package
// should be zipped to. Returns an empty path on any errors.
base::FilePath ZipDestination(const base::FilePath& path) {
  base::FilePath dest;

  if (!base::GetTempDir(&dest)) {
    // Couldn't get the temporary directory.
    return base::FilePath();
  }

  // TMPDIR/<bundleID>/zip_cache/<guid>

  NSString* bundleID = [[NSBundle mainBundle] bundleIdentifier];
  dest = dest.Append(AsStringPiece(bundleID));

  dest = dest.Append("zip_cache");

  NSString* guid = [[NSProcessInfo processInfo] globallyUniqueString];
  dest = dest.Append(AsStringPiece(guid));

  return dest;
}

// Returns the path of the package and its components relative to the package's
// parent directory.
std::vector<base::FilePath> RelativePathsForPackage(
    const base::FilePath& package) {
  // Reject packages where the root itself is a symbolic link.
  // When a user selects a file or package through the standard file picker
  // (NSOpenPanel), AppKit's `resolvesAliases` property is enabled by default,
  // resolving both Finder aliases and symlinks to their target directory before
  // reaching this code. Therefore, legitimate user selections through the
  // dialog already have their targets resolved. A raw symlink only arrives here
  // if the selection bypassed NSOpenPanel (such as via drag-and-drop onto an
  // <input type="file">). Following a root symlink in that case is dangerous
  // (e.g. a dropped "Fake.app -> /Users/victim" would cause the entire target
  // directory to be considered part of the package and zipped).
  if (base::IsLink(package)) {
    return {};
  }

  base::FilePath real_package = base::MakeAbsoluteFilePath(package);
  // MakeAbsoluteFilePath() uses realpath(3) and returns an empty path if the
  // package path does not exist on disk, permission is denied, or resolution
  // fails.
  if (real_package.empty()) {
    return {};
  }

  // Get the base directory.
  base::FilePath base_dir = package.DirName();

  // Add the package as the first relative path.
  std::vector<base::FilePath> relative_paths;
  relative_paths.push_back(package.BaseName());

  // Add the components of the package as relative paths.
  base::FileEnumerator file_enumerator(
      package, true /* recursive */,
      base::FileEnumerator::FILES | base::FileEnumerator::DIRECTORIES |
          base::FileEnumerator::SHOW_SYM_LINKS);
  for (base::FilePath path = file_enumerator.Next(); !path.empty();
       path = file_enumerator.Next()) {
    // Only include symlinks whose targets reside inside the package tree.
    // External symlinks (or broken symlinks) are skipped to prevent file
    // leakage outside the package. Internal symlinks are preserved by having
    // their target contents copied into the zip archive, preserving working
    // bundles.
    if (S_ISLNK(file_enumerator.GetInfo().stat().st_mode)) {
      base::FilePath target = base::MakeAbsoluteFilePath(path);
      if (target.empty() || !real_package.IsParent(target)) {
        continue;
      }
    }
    base::FilePath relative_path;
    bool success = base_dir.AppendRelativePath(path, &relative_path);
    if (success) {
      relative_paths.push_back(relative_path);
    }
  }

  return relative_paths;
}

}  // namespace

base::FilePath FileSelectHelper::ZipPackage(const base::FilePath& path) {
  base::FilePath dest(ZipDestination(path));
  if (dest.empty())
    return dest;

  if (!base::CreateDirectory(dest.DirName()))
    return base::FilePath();

  base::File file(dest, base::File::FLAG_CREATE | base::File::FLAG_WRITE);
  if (!file.IsValid())
    return base::FilePath();

  std::vector<base::FilePath> files_to_zip(RelativePathsForPackage(path));
  if (files_to_zip.empty()) {
    return base::FilePath();
  }

  base::FilePath base_dir = path.DirName();
  bool success = zip::ZipFiles(base_dir, files_to_zip, file.GetPlatformFile());

  int result = -1;
  if (success)
    result = fchmod(file.GetPlatformFile(), S_IRUSR);

  return result >= 0 ? dest : base::FilePath();
}

void FileSelectHelper::ProcessSelectedFilesMac(
    const std::vector<ui::SelectedFileInfo>& files) {
  // Make a mutable copy of the input files.
  std::vector<ui::SelectedFileInfo> files_out(files);
  std::vector<base::FilePath> temporary_files;

  for (auto& file_info : files_out) {
    NSString* filename = base::apple::FilePathToNSString(file_info.local_path);
    BOOL isPackage =
        [[NSWorkspace sharedWorkspace] isFilePackageAtPath:filename];
    if (isPackage && base::DirectoryExists(file_info.local_path)) {
      base::FilePath result = ZipPackage(file_info.local_path);

      if (!result.empty()) {
        temporary_files.push_back(result);
        file_info.local_path = result;
        file_info.file_path = result;
        file_info.display_name.append(".zip");
      }
    }
  }

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&FileSelectHelper::ProcessSelectedFilesMacOnUIThread, this,
                     files_out, temporary_files));
}

void FileSelectHelper::ProcessSelectedFilesMacOnUIThread(
    const std::vector<ui::SelectedFileInfo>& files,
    const std::vector<base::FilePath>& temporary_files) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!temporary_files.empty()) {
    temporary_files_.insert(
        temporary_files_.end(), temporary_files.begin(), temporary_files.end());

    // Typically, |temporary_files| are deleted after |web_contents_| is
    // destroyed. If |web_contents_| is already NULL, then the temporary files
    // need to be deleted now.
    if (!web_contents_) {
      DeleteTemporaryFiles();
      RunFileChooserEnd();
      return;
    }
  }

  ConvertToFileChooserFileInfoList(files);
}
