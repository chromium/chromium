// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNGRADE_DOWNGRADE_UTILS_H_
#define CHROME_BROWSER_DOWNGRADE_DOWNGRADE_UTILS_H_

#include <optional>

#include "base/files/file_path.h"
#include "base/functional/callback.h"

namespace downgrade {

// Returns a unique name for a path of the form |dir|/|name|.CHROME_DELETE, or
// an empty path if none such can be found. The path may contain " (N)" with
// some integer N before the final file extension.
base::FilePath GetTempDirNameForDelete(const base::FilePath& dir,
                                       const base::FilePath& name);

// Attempts to move/rename |source| to |target| without falling back to
// copy-and-delete. Returns true on success.
bool MoveWithoutFallback(const base::FilePath& source,
                         const base::FilePath& target);

// A callback that returns true when its argument names a path that should not
// be moved by MoveContents.
using ExclusionPredicate = base::RepeatingCallback<bool(const base::FilePath&)>;

// Moves the contents of directory |source| into the directory |target| (which
// may or may not exist) for deletion at a later time. Any directories that
// cannot be moved (most likely due to open files therein) are recursed into.
// |exclusions_predicate| is an optional callback that evaluates items in
// |source| to determine whether or not they should be skipped. Returns true if
// all items were moved successfully, false otherwise.
bool MoveContents(const base::FilePath& source,
                  const base::FilePath& target,
                  ExclusionPredicate exclusion_predicate);

}  // namespace downgrade

#endif  // CHROME_BROWSER_DOWNGRADE_DOWNGRADE_UTILS_H_
