// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/mac/code_sign_clone_manager.h"

#import <Foundation/Foundation.h>
#include <paths.h>
#include <sys/clonefile.h>
#include <sys/fcntl.h>
#include <sys/stat.h>
#include <sys/syslimits.h>
#include <unistd.h>

#include <iomanip>
#include <string>
#include <vector>

#include "base/apple/foundation_util.h"
#include "base/containers/fixed_flat_set.h"
#include "base/feature_list.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/mac/mac_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/path_service.h"
#include "base/posix/eintr_wrapper.h"
#include "base/process/launch.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/common/content_paths.h"
#include "content/public/common/content_switches.h"

//
// Sourced from Libc-1592.100.35 (macOS 14.5)
// https://github.com/apple-oss-distributions/Libc/blob/Libc-1592.100.35/gen/confstr.c#L78
//
// `DIRHELPER_USER_LOCAL_TRANSLOCATION` is not public and its name has been made
// up here. Support for the value `DIRHELPER_USER_LOCAL_TRANSLOCATION`
// represents was added in macOS 11.
//
typedef enum {
  DIRHELPER_USER_LOCAL = 0,            // "0/"
  DIRHELPER_USER_LOCAL_TEMP,           // "T/"
  DIRHELPER_USER_LOCAL_CACHE,          // "C/"
  DIRHELPER_USER_LOCAL_TRANSLOCATION,  // "X/"
  DIRHELPER_USER_LOCAL_LAST = DIRHELPER_USER_LOCAL_TRANSLOCATION
} dirhelper_which_t;

//
// Sourced from Libsystem-1345.120.2 (macOS 14.5)
// https://github.com/apple-oss-distributions/Libsystem/blob/Libsystem-1345.120.2/init.c#L125
//
// Tested on macOS 10.15+. If an unsupported `which` is provided, NULL is
// returned.
// If successful, the requested directory path will be returned. The directory
// will be created if it does not exist.
//
// When `DIRHELPER_USER_LOCAL_TRANSLOCATION` is provided and the calling process
// is sandboxed, `_dirhelper` will return NULL and the directory will not be
// created.
//
extern "C" char* _dirhelper(dirhelper_which_t which,
                            char* buffer,
                            size_t buffer_length);

namespace {

constexpr char kContentsMacOS[] = "Contents/MacOS";
constexpr char kCodeSignClone[] = "code_sign_clone";
constexpr int kMkdtempFormatXCount = 6;

NSString* g_temp_dir_for_testing = nil;
NSString* g_dirhelper_path_for_testing = nil;

// Removes the quarantine attribute, if any. Removal is best effort.
void RemoveQuarantineAttribute(const base::FilePath& path) {
  if (!base::mac::RemoveQuarantineAttribute(path)) {
    DLOG(ERROR) << "error removing quarantine attribute "
                << std::quoted(path.value());
  }
}

bool ValidateTempDir(const base::FilePath& path) {
  if (!base::MakeAbsoluteFilePath(path).value().starts_with(
          "/private/var/folders/")) {
    DLOG(ERROR) << "failed to validate temporary dir "
                << std::quoted(path.value());
    return false;
  }
  return true;
}

//
// Get a temporary directory that is cleaned on machine boot but not
// periodically. `DIRHELPER_USER_LOCAL_TRANSLOCATION` and `Cleanup At Startup`
// are the only found directories that have this behavior. Use
// `DIRHELPER_USER_LOCAL_TRANSLOCATION` when running on macOS 11+, and `Cleanup
// At Startup` otherwise.
//
// Returns true if a suitable temporary directory path is found. Returns false
// otherwise.
//
// Here are some notes about the various temporary directory options.
//
// `/tmp` (`/private/tmp`) is cleaned on machine boot. Additionally, files that
//   have a birth and access time older than three days are deleted. This is
//   handled by `/usr/libexec/tmp_cleaner`, which is run by the
//   `com.apple.tmp_cleaner` launch daemon.
//
// `/var/tmp` (`/private/var/tmp`) is not cleaned on machine boot or
//   periodically.
//
// `DIRHELPER_USER_LOCAL` (`/var/folders/.../0`) is not cleaned on machine boot
//   or periodically.
//
// `DIRHELPER_USER_LOCAL_TEMP` (`/var/folders/.../T`) is cleaned on machine
//   boot. Additionally, files that have a birth and access time older than
//   three days are deleted. This is handled by `/usr/libexec/dirhelper`, which
//   is run by the `com.apple.bsd.dirhelper` launch daemon. Recent versions of
//   `dirhelper` are not open source but here is the last open version (10.9.2,
//   2014-02-25) for reference if this assumption needs to be revisited.
//   https://github.com/apple-oss-distributions/system_cmds/blob/system_cmds-597.90.1/dirhelper.tproj/dirhelper.c
//
// `DIRHELPER_USER_LOCAL_CACHE` (`/var/folders/.../C`) is not cleaned on machine
//   boot or periodically.
//
// `DIRHELPER_USER_LOCAL_TRANSLOCATION` (`/var/folders/.../X`) is cleaned on
//   machine boot and not otherwise cleaned periodically, but is only available
//   on macOS 11 and later through a private interface.
//
// `Cleanup At Startup` (`/var/folders/.../Cleanup At Startup`) is cleaned on
//   machine boot and not otherwise cleaned periodically, but its path is not
//   available through any known interface.
//
// Note: APFS `access_time` is not updated when the file is read, unless its
// value is prior to the timestamp stored in the `mod_time` field. This is
// applicable to the periodic cleaning that happens in `/tmp` and
// `DIRHELPER_USER_LOCAL_TEMP`. The optional feature flag
// `APFS_FEATURE_STRICTATIME` (the `strictatime` mount option, see `man 8
// mount`) can be set to update `access_time` each time the file is read,
// however the flag is not enabled by default.
// https://developer.apple.com/support/downloads/Apple-File-System-Reference.pdf#page=67
//
// TODO(https://crbug.com/345241051): Add metric for premature clone clean up.
bool GetCleanupOnBootTempDir(base::FilePath* path) {
  if (g_temp_dir_for_testing) {
    *path = base::apple::NSStringToFilePath(g_temp_dir_for_testing);
    return true;
  }

  // Remove the @available check and the else block when
  // `MAC_OS_X_VERSION_MIN_REQUIRED` is macOS 11+.
  NSString* temp_dir;
  if (@available(macOS 11.0, *)) {
    char buffer[PATH_MAX];
    if (!g_dirhelper_path_for_testing &&
        !_dirhelper(DIRHELPER_USER_LOCAL_TRANSLOCATION, buffer, PATH_MAX)) {
      DLOG(ERROR) << "_dirhelper error";
      return false;
    }

    // /var/folders/.../X/
    temp_dir = g_dirhelper_path_for_testing ?: @(buffer);

    // `_dirhelper` with `DIRHELPER_USER_LOCAL_TRANSLOCATION` shouldn't return
    // any user controlled paths, but validate just to be sure.
    if (!ValidateTempDir(base::apple::NSStringToFilePath(temp_dir))) {
      return false;
    }
  } else {
    // /var/folders/.../T/
    temp_dir = NSTemporaryDirectory();

    if (![temp_dir.lastPathComponent isEqualToString:@"T"]) {
      DLOG(ERROR) << " NSTemporaryDirectory last component is not \"T\" "
                  << std::quoted(temp_dir.fileSystemRepresentation);
      return false;
    }

    // /var/folders/.../
    temp_dir = [temp_dir stringByDeletingLastPathComponent];

    // Internal `NSTemporaryDirectory` failures could result in env `TMPDIR`
    // being returned. Ensure we get an expected path.
    // Validate before trying to create "Cleanup At Startup".
    if (!ValidateTempDir(base::apple::NSStringToFilePath(temp_dir))) {
      return false;
    }

    // /var/folders/.../Cleanup At Startup/
    temp_dir = [temp_dir stringByAppendingPathComponent:@"Cleanup At Startup"];

    // Create the "Cleanup At Startup" directory if needed. When created by
    // macOS the mode is 0755, do the same here. If we are the ones who created
    // the directory, `chmod` 0755 to overcome a restrictive umask.
    if (mkdir(temp_dir.fileSystemRepresentation, 0755) == 0) {
      if (chmod(temp_dir.fileSystemRepresentation, 0755) != 0) {
        DPLOG(ERROR) << "chmod "
                     << std::quoted(temp_dir.fileSystemRepresentation);
        return false;
      }
    } else if (errno != EEXIST) {
      DPLOG(ERROR) << "mkdir "
                   << std::quoted(temp_dir.fileSystemRepresentation);
      return false;
    }
  }

  base::FilePath temporary_directory_path =
      base::apple::NSStringToFilePath(temp_dir);

  // `DIRHELPER_USER_LOCAL_TRANSLOCATION` created by `_dirhelper` or `"Cleanup
  // At Startup"` by `mkdir`, from the browser process, will be stamped with a
  // quarantine attribute. Attempt to remove it.
  RemoveQuarantineAttribute(temporary_directory_path);

  *path = temporary_directory_path;
  return true;
}

// Returns the "type" argument identifying a code sign clone cleanup process
// ("--type=code-sign-clone-cleanup").
std::string CodeSignCloneCleanupTypeArg() {
  return base::StringPrintf("--%s=%s", switches::kProcessType,
                            switches::kCodeSignCloneCleanupProcess);
}

// Returns the argument for the unique suffix of the temporary directory. The
// full path will be reconstructed and validated by the helper process.
// ("--unique-temp-dir-suffix=tKdILk").
std::string UniqueTempDirSuffixArg(const std::string& unique_temp_dir_suffix) {
  return base::StringPrintf("--%s=%s", switches::kUniqueTempDirSuffix,
                            unique_temp_dir_suffix.c_str());
}

// Example:
//   /private/var/folders/.../X/org.chromium.Chromium.code_sign_clone
bool GetCloneTempDir(base::FilePath* path) {
  base::FilePath temp_dir;
  if (!GetCleanupOnBootTempDir(&temp_dir)) {
    return false;
  }
  base::StringPiece prefix = base::apple::BaseBundleID();
  *path = base::MakeAbsoluteFilePath(temp_dir).Append(
      base::StrCat({prefix, ".", kCodeSignClone}));
  return true;
}

// Example:
//   /private/var/folders/.../X/org.chromium.Chromium.code_sign_clone/code_sign_clone.tKdILk
bool CreateUniqueCloneTempDir(base::FilePath* path) {
  base::FilePath clone_temp_dir;
  if (!GetCloneTempDir(&clone_temp_dir)) {
    return false;
  }

  // 0700 was chosen intentionally to avoid giving away filesystem access more
  // broadly to something that might be private and protected.
  if (mkdir(clone_temp_dir.value().c_str(), 0700) != 0 && errno != EEXIST) {
    DPLOG(ERROR) << "mkdir " << std::quoted(clone_temp_dir.value());
    return false;
  }
  RemoveQuarantineAttribute(clone_temp_dir);

  // Example:
  //   code_sign_clone.XXXXXX
  std::string format = base::StrCat(
      {kCodeSignClone, ".", std::string(kMkdtempFormatXCount, 'X')});

  base::FilePath unique_format = clone_temp_dir.Append(format);
  char* buffer = const_cast<char*>(unique_format.value().c_str());
  if (!mkdtemp(buffer)) {
    DPLOG(ERROR) << "mkdtemp " << std::quoted(buffer);
    return false;
  }
  base::FilePath unique_path = base::FilePath(buffer);
  RemoveQuarantineAttribute(unique_path);

  *path = unique_path;
  return true;
}

// Example suffix:
//   tKdILk
// Example return value:
//   /private/var/folders/.../X/org.chromium.Chromium.code_sign_clone/code_sign_clone.tKdILk
// May return an empty path if the constructed path does not resolve.
base::FilePath GetAbsoluteUniqueCloneTempDirForSuffix(
    const std::string& suffix) {
  base::FilePath clone_temp_dir;
  if (!GetCloneTempDir(&clone_temp_dir)) {
    return base::FilePath();
  }
  return base::MakeAbsoluteFilePath(
      clone_temp_dir.Append(base::StrCat({kCodeSignClone, ".", suffix})));
}

void RecordHardLinkError(int error) {
  base::UmaHistogramSparse("Mac.AppHardLinkError", error);
}

// Unlink the destination main executable and replace it with a hard link to
// source main executable.
bool HardLinkMainExecutable(const base::FilePath& source_path,
                            const base::FilePath& destination_path,
                            const base::FilePath& main_executable_name) {
  base::FilePath destination_main_executable_path =
      destination_path.Append(kContentsMacOS).Append(main_executable_name);
  if (unlink(destination_main_executable_path.value().c_str()) != 0 &&
      errno != ENOENT) {
    DPLOG(ERROR) << "unlink "
                 << std::quoted(destination_main_executable_path.value());
    return false;
  }
  base::FilePath source_main_executable_path =
      source_path.Append(kContentsMacOS).Append(main_executable_name);
  if (link(source_main_executable_path.value().c_str(),
           destination_main_executable_path.value().c_str()) != 0) {
    RecordHardLinkError(errno);
    DPLOG(ERROR) << "link " << std::quoted(source_main_executable_path.value())
                 << ", "
                 << std::quoted(destination_main_executable_path.value());
    return false;
  }
  RecordHardLinkError(0);
  return true;
}

void RecordClonefileError(int error) {
  base::UmaHistogramSparse("Mac.AppClonefileError", error);
}

// Copy-on-write clones `source_path` to `destination_path`. The `source_path`
// main executable is then hard linked within the the corresponding
// `destination_path` directory.
bool CloneApp(const base::FilePath& source_path,
              const base::FilePath& destination_path,
              const base::FilePath& main_executable_name) {
  // Clone the entire app.
  // `CLONEFILE(2)` strongly discourages using `clonefile` to clone directories.
  // It suggests using `copyfile` instead. When cloning M125 Chrome on an M1 Max
  // Mac `copyfile` is much slower than `clonefile` (~70ms vs. ~10ms).
  // We are ignoring the warning because of the speed gains with `clonefile`.
  // A feedback has been opened with Apple asking about this warning.
  // FB13814551: clonefile directories
  if (clonefile(source_path.value().c_str(), destination_path.value().c_str(),
                0) != 0) {
    RecordClonefileError(errno);
    DPLOG(ERROR) << "clonefile " << std::quoted(source_path.value()) << ", "
                 << std::quoted(destination_path.value());
    return false;
  }
  RecordClonefileError(0);

  // The top top level directory created by `clonefile` has the quarantine
  // attribute set. The rest of the directory tree does not have the attribute
  // set.
  RemoveQuarantineAttribute(destination_path);

  // Hard link the main executable.
  if (!HardLinkMainExecutable(source_path, destination_path,
                              main_executable_name)) {
    return false;
  }
  return true;
}

// Launch the code-sign-clone-cleanup helper process passing the unique suffix
// of the temporary directory as an argument. The full path will be
// reconstructed and validated in the helper process.
void DeleteUniqueTempDirRecursivelyFromHelperProcess(
    const std::string& unique_suffix) {
  base::FilePath child_path;
  if (!base::PathService::Get(content::CHILD_PROCESS_EXE, &child_path)) {
    DLOG(ERROR) << "No CHILD_PROCESS_EXE";
    return;
  }

  std::vector<std::string> code_sign_clone_cleanup_args{
      child_path.value(),
      CodeSignCloneCleanupTypeArg(),
      UniqueTempDirSuffixArg(unique_suffix),
  };

  // The child helper process should outlive its parent.
  base::LaunchOptions options;
  options.new_process_group = true;

  // Null out stdout and stderr to prevent unexpected output after the browser
  // has exited if launching from a terminal.
  // `base::LaunchProcess` maps stdin to /dev/null.
  base::ScopedFD null_fd(HANDLE_EINTR(open(_PATH_DEVNULL, O_WRONLY)));
  if (!null_fd.is_valid()) {
    DPLOG(ERROR) << "open " << std::quoted(_PATH_DEVNULL);
    return;
  }
  options.fds_to_remap.emplace_back(null_fd.get(), STDOUT_FILENO);
  options.fds_to_remap.emplace_back(null_fd.get(), STDERR_FILENO);

  if (!base::LaunchProcess(code_sign_clone_cleanup_args, options).IsValid()) {
    DLOG(ERROR) << "base::LaunchProcess failed";
    return;
  }
}

// Example of an expected unique_temp_dir_suffix: tKdILk
bool ValidateUniqueDirSuffix(const std::string& unique_temp_dir_suffix) {
  //
  // mkdtemp(XXXXXX) possible values.
  //
  // Quote from:
  // https://pubs.opengroup.org/onlinepubs/9699919799/functions/mkdtemp.html
  //
  // "The mkdtemp() function shall modify the contents of template by replacing
  // six or more 'X' characters at the end of the pathname with the same number
  // of characters from the portable filename character set."
  //
  // The portable filename character set:
  // https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/V1_chap03.html#tag_03_282
  //
  static constexpr auto kFormat = base::MakeFixedFlatSet<const char>({
      'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm',
      'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z',
      'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M',
      'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
      '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '.', '_', '-',
  });
  if (unique_temp_dir_suffix.length() != kMkdtempFormatXCount) {
    return false;
  }
  for (const char& c : unique_temp_dir_suffix) {
    if (!kFormat.contains(c)) {
      return false;
    }
  }
  return true;
}

// Example of an expected unique_temp_dir_path:
//   /private/var/folders/.../X/org.chromium.Chromium.code_sign_clone/code_sign_clone.tKdILk
// Make sure the path is within GetCloneTempDir() and has a valid prefix.
bool ValidateUniqueTempDirPath(const base::FilePath& unique_temp_dir_path) {
  base::FilePath clone_temp_dir;
  if (!GetCloneTempDir(&clone_temp_dir)) {
    return false;
  }
  base::FilePath prefix =
      clone_temp_dir.Append(base::StrCat({kCodeSignClone, "."}));
  return unique_temp_dir_path.value().starts_with(prefix.value());
}

}  // namespace

namespace code_sign_clone_manager {

BASE_FEATURE(kMacAppCodeSignClone,
             "MacAppCodeSignClone",
             base::FEATURE_DISABLED_BY_DEFAULT);

CodeSignCloneManager::CodeSignCloneManager(
    const base::FilePath& src_path,
    const base::FilePath& main_executable_name,
    CloneCallback callback) {
  if (!base::FeatureList::IsEnabled(kMacAppCodeSignClone)) {
    return;
  }

  // Post a background task to perform the clone. If the task has not yet
  // started and Chrome is shutdown, the `SKIP_ON_SHUTDOWN` behavior will drop
  // the task. This is okay. If Chrome is shutting down, there is no need for a
  // code-sign-clone. If the task does not run, `needs_cleanup_` will be `false`
  // which will stop `~CodeSignCloneManager` from unnecessarily launching the
  // cleanup helper. If the task does run, it is guaranteed to complete before
  // `ThreadPoolInstance::Shutdown` returns, which is run before
  // `~CodeSignCloneManager`. It is safe to read `needs_cleanup_` from
  // `~CodeSignCloneManager` without any explicit synchronization.
  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&CodeSignCloneManager::Clone, weak_factory_.GetWeakPtr(),
                     src_path, main_executable_name, std::move(callback)));
}

CodeSignCloneManager::~CodeSignCloneManager() {
  if (!needs_cleanup_) {
    return;
  }

  // Unlinking M125 takes ~20ms on an M1 Max Mac. When this destructor is
  // called, Chrome is in the process of shutting down and new background tasks
  // can not be posted. Instead of blocking, perform the unlinking from a child
  // helper process.
  DeleteUniqueTempDirRecursivelyFromHelperProcess(unique_temp_dir_suffix_);
}

void CodeSignCloneManager::Clone(const base::FilePath& src_path,
                                 const base::FilePath& main_executable_name,
                                 CloneCallback callback) {
  base::TimeTicks start_time = base::TimeTicks::Now();

  // Intentionally avoiding `base::ScopedTempDir()`. The temp dir is
  // expected to exist beyond the lifetime of this process. The temp dir
  // will be deleted by the clone-cleanup helper process after the browser
  // exits.
  //
  // Also intentionally avoiding `base::GetTempDir()` which uses the
  // `MAC_CHROMIUM_TMPDIR` environment variable (if set). Since
  // `unique_temp_dir_path` will be reconstructed in the clone-cleanup helper
  // process, external control over the `MAC_CHROMIUM_TMPDIR` and the
  // `unique_temp_dir_suffix_` argument could result in misuse. We want
  // to prevent the helper process from being able to delete arbitrary
  // files.
  //
  // Example `unique_temp_dir_path`:
  //   /private/var/folders/.../X/org.chromium.Chromium.code_sign_clone/code_sign_clone.tKdILk
  //
  // The clone will be created inside of this directory.
  base::FilePath unique_temp_dir_path;
  if (!CreateUniqueCloneTempDir(&unique_temp_dir_path)) {
    std::move(callback).Run(base::FilePath());
    return;
  }

  // .tKdILk
  unique_temp_dir_suffix_ = unique_temp_dir_path.FinalExtension();

  // Trim the leading "."
  unique_temp_dir_suffix_.erase(unique_temp_dir_suffix_.begin());

  // `unique_temp_dir_suffix_` will be validated later during cleanup from the
  // helper process. To avoid leaking clones if validation fails, make sure it
  // passes validation here before continuing.
  if (!ValidateUniqueDirSuffix(unique_temp_dir_suffix_)) {
    DLOG(ERROR) << "ValidateUniqueDirSuffix() failed";
    return;
  }

  // Ignore any errors from creating the clone. There are many scenarios where
  // these operations could fail (different filesystems for the source and
  // destination, no clone filesystem support, read only disk, full disk,
  // etc.). If there is a failure, clean up any artifacts and allow Chrome to
  // keep running. Instances of Chrome in this situation will be susceptible to
  // code signature validation errors when an update is staged on disk. This is
  // being tracked via the Mac.AppUpgradeCodeSignatureValidationStatus metric.
  base::FilePath clone_app_path =
      unique_temp_dir_path.Append(src_path.BaseName());
  if (!CloneApp(src_path, clone_app_path, main_executable_name)) {
    base::DeletePathRecursively(unique_temp_dir_path);
    std::move(callback).Run(base::FilePath());
    return;
  }

  base::TimeDelta delta = base::TimeTicks::Now() - start_time;
  base::UmaHistogramTimes("Mac.AppCodeSignCloneCreationTime", delta);

  // Let `~CodeSignCloneManager` know it needs to clean up. `Clone` is run from
  // a posted task which is guaranteed to finish once it has started. It will
  // block shutdown until complete. `ThreadPoolInstance::Shutdown` is run before
  // `~CodeSignCloneManager`. `Clone` and ` ~CodeSignCloneManager` will never
  // overlap, it is safe to set `needs_cleanup_` from this task.
  needs_cleanup_ = true;

  std::move(callback).Run(clone_app_path);

  // TODO(https://crbug.com/343784575): Search for inactive clones and clean
  // them up and/or create a metric.
}

void CodeSignCloneManager::SetTemporaryDirectoryPathForTesting(
    const base::FilePath& path) {
  g_temp_dir_for_testing = base::apple::FilePathToNSString(path);
}

void CodeSignCloneManager::ClearTemporaryDirectoryPathForTesting() {
  g_temp_dir_for_testing = nil;
}

void CodeSignCloneManager::SetDirhelperPathForTesting(
    const base::FilePath& path) {
  g_dirhelper_path_for_testing = base::apple::FilePathToNSString(path);
}

void CodeSignCloneManager::ClearDirhelperPathForTesting() {
  g_dirhelper_path_for_testing = nil;
}

base::FilePath CodeSignCloneManager::GetCloneTemporaryDirectoryForTesting() {
  base::FilePath clone_temp_dir;
  GetCloneTempDir(&clone_temp_dir);
  return clone_temp_dir;
}

namespace internal {

// Main entry point for `--type=clone-cleanup` helper process.
// switches::kUniqueTempDirSuffix is expected; the full path will be
// reconstructed and validated. If switches::kWaitForParentExit exists the
// unique temporary directory will be deleted once the parent process exits.
int ChromeCodeSignCloneCleanupMain(
    content::MainFunctionParams main_parameters) {
  // Make sure the unique suffix is the correct format.
  std::string unique_temp_dir_suffix =
      main_parameters.command_line->GetSwitchValueASCII(
          switches::kUniqueTempDirSuffix);
  if (!ValidateUniqueDirSuffix(unique_temp_dir_suffix)) {
    DLOG(ERROR) << "ValidateUniqueDirSuffix() failed";
    return 1;
  }

  // Make sure the resolved path points to the expected location.
  base::FilePath unique_temp_dir_path =
      GetAbsoluteUniqueCloneTempDirForSuffix(unique_temp_dir_suffix);
  if (!ValidateUniqueTempDirPath(unique_temp_dir_path)) {
    DLOG(ERROR) << "ValidateUniqueTempDirPath() failed";
    return 1;
  }

  // The "--type=clone-cleanup" helper process is launched during from the
  // browser process during shutdown. Wait until the parent browser process dies
  // to ensure the clone is not being used.
  // There is no rush to clean up the temporary clone. Prefer polling the ppid
  // over more responsive but complex options.
  if (!main_parameters.command_line->HasSwitch(
          "no-wait-for-parent-exit-for-testing")) {
    while (getppid() != 1) {
      sleep(1);
    }
  }

  base::DeletePathRecursively(unique_temp_dir_path);
  return 0;
}

}  // namespace internal

}  // namespace code_sign_clone_manager
