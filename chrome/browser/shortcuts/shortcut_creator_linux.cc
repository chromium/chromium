// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/shortcuts/shortcut_creator_linux.h"

#include <stdlib.h>

#include <optional>

#include "base/base_paths.h"
#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/safe_base_name.h"
#include "base/hash/md5.h"
#include "base/location.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_refptr.h"
#include "base/path_service.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/shell_integration_linux.h"
#include "chrome/browser/shortcuts/linux_xdg_wrapper.h"
#include "chrome/browser/shortcuts/linux_xdg_wrapper_impl.h"
#include "chrome/browser/shortcuts/shortcut_creator.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_family.h"
#include "url/gurl.h"

namespace shortcuts {
namespace {

base::SafeBaseName GenerateIconFilename(const GURL& url) {
  std::string url_hash = base::MD5String(url.spec());
  std::optional<base::SafeBaseName> base_name =
      base::SafeBaseName::Create(base::StrCat({"shortcut-", url_hash, ".png"}));
  CHECK(base_name);
  return base_name.value();
}

ShortcutCreatorResult CreateExecutableFile(const base::FilePath& file_path,
                                           const std::string& contents) {
  base::File file(file_path, base::File::FLAG_CREATE | base::File::FLAG_WRITE);
  if (!file.IsValid()) {
    return ShortcutCreatorResult::kError;
  }
  if (!file.Write(0, base::as_byte_span(contents)).has_value()) {
    // Attempt to delete the file to clean up
    file.Close();
    base::DeleteFile(file_path);
    return ShortcutCreatorResult::kError;
  }
  file.Close();

  // User:  RWX
  // Group: R_X
  // Other: R_X
  bool success = base::SetPosixFilePermissions(
      file_path, base::FILE_PERMISSION_USER_MASK |
                     base::FILE_PERMISSION_READ_BY_GROUP |
                     base::FILE_PERMISSION_EXECUTE_BY_GROUP |
                     base::FILE_PERMISSION_READ_BY_OTHERS |
                     base::FILE_PERMISSION_EXECUTE_BY_OTHERS);
  // Failing to set permissions is acceptable.
  return success ? ShortcutCreatorResult::kSuccess
                 : ShortcutCreatorResult::kSuccessWithErrors;
}

}  // namespace

ShortcutCreatorOutput CreateShortcutOnLinuxDesktop(
    const std::string& shortcut_name,
    const GURL& shortcut_url,
    const gfx::Image& icon,
    const base::FilePath& profile_path,
    LinuxXdgWrapper& xdg_wrapper) {
  // First, create necessary directories and do .desktop file location
  // resolution before writing any data, removing some cleanup.
  base::FilePath icon_directory = profile_path.Append(kWebShortcutsIconDirName);
  if (!base::CreateDirectory(icon_directory)) {
    return {.result = ShortcutCreatorResult::kError};
  }
  EmitIconStorageCountMetric(icon_directory);

  std::optional<base::SafeBaseName> desktop_file_basename =
      shell_integration_linux::GetUniqueWebShortcutFilename(shortcut_name);

  if (!desktop_file_basename) {
    return {.result = ShortcutCreatorResult::kError};
  }

  base::FilePath desktop_path;
  if (!base::PathService::Get(base::DIR_USER_DESKTOP, &desktop_path)) {
    return {.result = ShortcutCreatorResult::kError};
  }

  base::FilePath shortcut_desktop_location =
      desktop_path.Append(desktop_file_basename->path());

  // Second, write the icon to disk.
  base::SafeBaseName icon_base_name = GenerateIconFilename(shortcut_url);
  base::FilePath icon_path = icon_directory.Append(icon_base_name);
  scoped_refptr<base::RefCountedMemory> png_data = icon.As1xPNGBytes();

  bool non_fatal_failure = false;
  if (!base::WriteFile(icon_path, *png_data)) {
    non_fatal_failure = true;
  }

  // Third, create the .desktop file.
  std::string desktop_file_contents =
      shell_integration_linux::GetDesktopFileContentsForUrlShortcut(
          shortcut_name, shortcut_url, icon_path, profile_path);
  ShortcutCreatorResult shortcut_file_creation_result =
      CreateExecutableFile(shortcut_desktop_location, desktop_file_contents);
  switch (shortcut_file_creation_result) {
    case ShortcutCreatorResult::kError:
      // Attempt to clean up the icon.
      base::DeleteFile(icon_path);
      return {.result = ShortcutCreatorResult::kError};
    case ShortcutCreatorResult::kSuccessWithErrors:
      non_fatal_failure = true;
      break;
    case ShortcutCreatorResult::kSuccess:
      break;
  }

  // Fourth, install the .desktop file into the desktop menu. If this fails that
  // is OK, as linux distros often have the capability of double-clicking the
  // .desktop file to launch them.
  int error_code = xdg_wrapper.XdgDesktopMenuInstall(shortcut_desktop_location);
  if (error_code != EXIT_SUCCESS) {
    non_fatal_failure = true;
  }

  auto success_result = non_fatal_failure
                            ? ShortcutCreatorResult::kSuccessWithErrors
                            : ShortcutCreatorResult::kSuccess;
  return {.shortcut_path = shortcut_desktop_location, .result = success_result};
}

namespace {
LinuxXdgWrapper* g_xdg_wrapper_override = nullptr;
}

void SetDefaultXdgWrapperForTesting(LinuxXdgWrapper* xdg_wrapper) {
  CHECK_NE(xdg_wrapper != nullptr, g_xdg_wrapper_override != nullptr);
  g_xdg_wrapper_override = xdg_wrapper;
}

void CreateShortcutOnUserDesktop(ShortcutMetadata shortcut_metadata,
                                 ShortcutCreatorCallback complete) {
  CHECK(shortcut_metadata.IsValid());
  const gfx::Image* image =
      shortcut_metadata.shortcut_images.GetBest(gfx::Size(128, 128));
  CHECK(image);
  CHECK_EQ(image->Size().width(), 128);
  LinuxXdgWrapperImpl wrapper_impl;
  ShortcutCreatorOutput result = CreateShortcutOnLinuxDesktop(
      base::UTF16ToUTF8(shortcut_metadata.shortcut_title),
      shortcut_metadata.shortcut_url, *image, shortcut_metadata.profile_path,
      g_xdg_wrapper_override ? *g_xdg_wrapper_override : wrapper_impl);
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(complete), result.shortcut_path, result.result));
}

scoped_refptr<base::SequencedTaskRunner> GetShortcutsTaskRunner() {
  return base::ThreadPool::CreateSequencedTaskRunner(
      {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::BLOCK_SHUTDOWN});
}

}  // namespace shortcuts
