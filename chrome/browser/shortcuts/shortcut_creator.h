// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHORTCUTS_SHORTCUT_CREATOR_H_
#define CHROME_BROWSER_SHORTCUTS_SHORTCUT_CREATOR_H_

#include <string>

#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/task/sequenced_task_runner.h"
#include "ui/gfx/image/image_family.h"
#include "url/gurl.h"

namespace shortcuts {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class ShortcutCreatorResult {
  kSuccess = 0,
  kSuccessWithErrors = 1,
  kError = 2,
  kMaxValue = kError
};

using ShortcutCreatorCallback =
    base::OnceCallback<void(const base::FilePath& created_shortcut_path,
                            ShortcutCreatorResult shortcut_creation_result)>;

// Requirements for ShortcutMetadata to be valid:
// - The `shortcut_url` must be valid.
// - The `shortcut_title` must not be empty.
// - The `shortcut_images` must not be empty.
// - The `profile_path` must be valid / non-empty, and the full profile path
//   (not just the base name).
struct ShortcutMetadata {
  ShortcutMetadata();
  ShortcutMetadata(base::FilePath profile_path,
                   GURL shortcut_url,
                   std::u16string shortcut_title,
                   gfx::ImageFamily shortcut_images);
  ~ShortcutMetadata();

  // Move ctor since gfx::ImageFamily's copy ctor is implicitly deleted.
  ShortcutMetadata(ShortcutMetadata&& metadata);
  ShortcutMetadata& operator=(ShortcutMetadata&& metadata);

  // Returns true if the requirements for using ShortcutMetadata are upheld.
  bool IsValid();

  base::FilePath profile_path;
  GURL shortcut_url;
  std::u16string shortcut_title;
  gfx::ImageFamily shortcut_images;
};

// Path in user profile directory to store shortcut icons on Windows and Linux.
inline constexpr base::FilePath::StringPieceType kWebShortcutsIconDirName =
    FILE_PATH_LITERAL("Web Shortcut Icons");

// Creates a shortcut on the OS desktop with the given shortcut_metadata. When
// clicked / launched, it will launch the given url in a new chrome tab. This
// must be called on a sequence that allows blocking calls, such as the one
// returned by the GetShortcutsTaskRunner().
//
// `complete` will be called on the same sequence as
// `CreateShortcutOnUserDesktop` was called on. Use BindPostTask or similar if
// you want the callback to be called on a different sequence.
void CreateShortcutOnUserDesktop(ShortcutMetadata shortcut_metadata,
                                 ShortcutCreatorCallback complete);

// Get the task runner to call CreateShortcutOnUserDesktop on. This is obtained
// from the threadpool specifically to perform OS tasks on.
scoped_refptr<base::SequencedTaskRunner> GetShortcutsTaskRunner();

// Emits the "Shortcuts.Icons.StorageCount" metric to record the number of icon
// in the given directory.
void EmitIconStorageCountMetric(const base::FilePath& icon_directory);

}  // namespace shortcuts

#endif  // CHROME_BROWSER_SHORTCUTS_SHORTCUT_CREATOR_H_
