// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/bookmarks/bookmark_api_constants.h"

namespace extensions {
namespace bookmark_api_constants {

const char kParentIdKey[] = "parentId";
const char kUrlKey[] = "url";
const char kTitleKey[] = "title";

const char kNoNodeError[] = "Can't find bookmark for id.";
const char kNoParentError[] = "Can't find parent bookmark for id.";
const char kFolderNotEmptyError[] =
    "Can't remove non-empty folder (use recursive to force).";
const char kInvalidIdError[] = "Bookmark id is invalid.";
const char kInvalidIndexError[] = "Index out of bounds.";
const char kInvalidUrlError[] = "Invalid URL.";
const char kModifySpecialError[] = "Can't modify the root bookmark folders.";
const char kEditBookmarksDisabled[] = "Bookmark editing is disabled.";
const char kModifyManagedError[] = "Can't modify managed bookmarks.";
const char kInvalidParamError[] = "Parameter 'key' is invalid.";
const char kCannotSetUrlOfFolderError[] = "Can't set URL of a bookmark folder.";
const char kBookmarkNodesNotFoundFromIdListError[] =
    "Could not find bookmark nodes with given ids: [*]";

}  // namespace bookmark_api_constants
}  // namespace extensions
