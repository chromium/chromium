// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/bookmarks/bookmarks_error_constants.h"

namespace extensions::bookmarks_errors {

const char kNoNodeError[] = "Can't find bookmark for id.";
const char kNoParentError[] = "Can't find parent bookmark for id.";
const char kFolderNotEmptyError[] =
    "Can't remove non-empty folder (use recursive to force).";
const char kInvalidIdError[] = "Bookmark id is invalid.";
const char kInvalidIndexError[] = "Index out of bounds.";
const char kInvalidParentError[] =
    "Parameter 'parentId' does not specify a folder.";
const char kInvalidUrlError[] = "Invalid URL.";
const char kModifySpecialError[] = "Can't modify the root bookmark folders.";
const char kEditBookmarksDisabled[] = "Bookmark editing is disabled.";
const char kModifyManagedError[] = "Can't modify managed bookmarks.";
const char kCannotSetUrlOfFolderError[] = "Can't set URL of a bookmark folder.";
const char kInvalidMoveDestinationError[] =
    "Can't move a folder to itself or its descendant.";

}  // namespace extensions::bookmarks_errors
