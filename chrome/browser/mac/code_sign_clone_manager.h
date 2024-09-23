// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MAC_CODE_SIGN_CLONE_MANAGER_H_
#define CHROME_BROWSER_MAC_CODE_SIGN_CLONE_MANAGER_H_

#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "content/public/common/main_function_params.h"

namespace code_sign_clone_manager {

BASE_DECLARE_FEATURE(kMacAppCodeSignClone);

//
// Manages a temporary copy-on-write clone of an app bundle. The temporary clone
// crucially has its main executable replaced with a hard link to the source's
// main executable. This is intended for use by the browser app to keep in-use
// files available on the filesystem after a staged update. This is in service
// of keeping the app's code signature statically valid and in agreement with
// its dynamic code signature after a staged update. See
// https://crbug.com/338582873 for more details.
//
// During initialization, the app bundle in `src_path` will be APFS
// copy-on-write cloned to a temporary directory. The main executable in the
// clone is then replaced with a hard link to the `src_path` main executable.
// The hard linked main executable is crucial in passing dynamic code signature
// validation after an update is staged. For dynamic validation, we need to keep
// a reachable path to the main executable alive on the filesystem, which is why
// it is hard linked. To support support static code signature validation, the
// hard linked main executable needs to be surrounded by a bundle structure that
// validates. The rest of the cloned files make up this bundle structure and
// support static code signature validation.
//
// Creation of this clone is best effort, there are no guarantees of success.
// Some failure cases include running the source app bundle from:
//   * not-APFS (because filesystem clones are only supported on APFS)
//   * a read-only filesystem (because this needs to write to the same
//   filesystem that the app is running from)
//   * a non-root filesystem (because the clone is built in
//   `/private/var/folders/...`, which is on the root filesystem)
//
// Upon destruction, the temporary directory hosting the clone will be deleted.
//
// Clone creation and deletion will take place in the background and will not
// block. Background clone creation will take place in the calling process on a
// background thread to prevent blocking startup. Care has been taken to
// optimize clone creation to avoid resource contention on startup. Background
// clone deletion will take place from a helper process. This is to prevent
// blocking browser shutdown, with the added benefit of keeping in-use files
// available on the filesystem until the very end of the browser process.
//
// Cloning and hard linking the main executable was chosen as it is the least
// expensive out of the explored options (tested with Google Chrome M125 on an
// M1 Max Mac).
//
// `clonefile` + `link` the main executable: ~10ms
// Hard linking the whole app tree: best approach ~60ms
//   * `-[NSFileManager linkItemAtURL:toURL:error:]`: ~120ms
//   * `base::FileEnumerator`: ~80ms
//   * `readdir`, `getattrlistbulk`, `fts` only `stat`ing dirs: ~60ms
//
// In the common case, both options (`clonefile` vs. hard link-based) take up no
// extra disk space, aside from directory entries, while `clonefile` involves
// the least amount of touching of the disk. The only disadvantage of
// `clonefile` is that it’s currently APFS-only. The purely hard link-based
// approaches would be more broadly applicable (on HFS+ for example). This is
// not a major concern given how ubiquitous APFS on macOS has become.
//
// Each instance of Chrome will create a new temporary clone of itself at
// startup even if one already exists. In this way each instance of Chrome
// maintains a reference to its specific in-use files, keeping them accessible
// on the filesystem until exit. Once the last instance of Chrome that maintains
// a reference to the in-use files exits and its cleanup helper runs, the files
// will become inaccessible on the filesystem and their disk blocks will be
// freed.
//
// Example path to the cloned app bundle:
//   /private/var/folders/c4/ygf_t4gn0tx0k1y1hm32hh6w00b_4p/X/org.chromium.Chromium.code_sign_clone/code_sign_clone.tKdILk/Chromium.app
//
// Each clone contains an instance-specific snapshot of an on-disk
// representation of Chrome. The bundles are verifiable by both dynamic and
// static code signature checks.
//
class CodeSignCloneManager {
 public:
  using CloneCallback = base::OnceCallback<void(base::FilePath)>;

  //
  // `src_path` is the path to the app bundle to be cloned.
  //
  // `main_executable_name` is the name of the main executable to be hard
  // linked. The name must be an entry in the "Contents/MacOS" directory within
  // the `src_path`.
  //
  // `callback` is called after the clone has been created or if an error
  // occurs.
  //
  CodeSignCloneManager(const base::FilePath& src_path,
                       const base::FilePath& main_executable_name,
                       CloneCallback callback = base::DoNothing());
  CodeSignCloneManager(const CodeSignCloneManager&) = delete;
  CodeSignCloneManager& operator=(const CodeSignCloneManager&) = delete;
  ~CodeSignCloneManager();

  static void SetTemporaryDirectoryPathForTesting(const base::FilePath& path);
  static void ClearTemporaryDirectoryPathForTesting();
  static void SetDirhelperPathForTesting(const base::FilePath& path);
  static void ClearDirhelperPathForTesting();
  static base::FilePath GetCloneTemporaryDirectoryForTesting();
  bool get_needs_cleanup_for_testing() { return needs_cleanup_; }

 private:
  void Clone(const base::FilePath& src_path,
             const base::FilePath& main_executable_name,
             CloneCallback callback);
  void StartCloneExistsTimer(const base::FilePath& clone_app_path,
                             const base::FilePath& main_executable_name);
  void StopCloneExistsTimer();
  void CloneExistsTimerFire(const base::FilePath& clone_app_path,
                            const base::FilePath& main_executable_name);

  std::string unique_temp_dir_suffix_;
  base::RepeatingTimer clone_exists_timer_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  bool needs_cleanup_ = false;
};

namespace internal {

// The entry point into the background clone cleanup process. This is not a user
// API.
int ChromeCodeSignCloneCleanupMain(content::MainFunctionParams main_parameters);

//
// Checks if a file is open more than once, globally (across all processes on a
// host), including the calling process.
//
// Usage of this check is fairly niche. It does not provide any information
// about which process has a reference to an open file. For that see
// `proc_listpidspath` (The linked header is from macOS 14.5).
// https://github.com/apple-oss-distributions/xnu/blob/xnu-10063.121.3/libsyscall/wrappers/libproc/libproc.h
//
// This function simply checks if a file is exclusively opened. Performance
// concerns may be a reason to use this function over `proc_listpidspath`, which
// needs to loop over the process tree.
//
// Note: File descriptors that have been `dup`ed or inherited only count as
// being opened once for this check’s purpose.
//
enum class FileOpenMoreThanOnce {
  kNo = 0,
  kYes = 1,
  kError = 2,
};
FileOpenMoreThanOnce IsFileOpenMoreThanOnce(const base::FilePath& file_path);
FileOpenMoreThanOnce IsFileOpenMoreThanOnce(int file_descriptor);

}  // namespace internal

}  // namespace code_sign_clone_manager

#endif  // CHROME_BROWSER_MAC_CODE_SIGN_CLONE_MANAGER_H_
