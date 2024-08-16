// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_BOOKMARKS_BOOKMARKS_ERROR_CONSTANTS_H_
#define CHROME_BROWSER_EXTENSIONS_BOOKMARKS_BOOKMARKS_ERROR_CONSTANTS_H_

// Error constants used for bookmarks-related APIs.

namespace extensions::bookmarks_errors {

extern const char kNoNodeError[];
extern const char kNoParentError[];
extern const char kFolderNotEmptyError[];
extern const char kInvalidIdError[];
extern const char kInvalidIndexError[];
extern const char kInvalidParentError[];
extern const char kInvalidUrlError[];
extern const char kModifySpecialError[];
extern const char kEditBookmarksDisabled[];
extern const char kModifyManagedError[];
extern const char kCannotSetUrlOfFolderError[];
extern const char kInvalidMoveDestinationError[];

}  // namespace extensions::bookmarks_errors

#endif  // CHROME_BROWSER_EXTENSIONS_BOOKMARKS_BOOKMARKS_ERROR_CONSTANTS_H_
