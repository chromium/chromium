// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FIRST_RUN_BOOKMARK_IMPORTER_H_
#define CHROME_BROWSER_FIRST_RUN_BOOKMARK_IMPORTER_H_

#include "base/values.h"

class Profile;

namespace first_run {

// Starts the process of importing bookmarks from a dictionary. The import is
// asynchronous and will start when the bookmark model is loaded, and Profile is
// kept alive during the import process.
// Favicon fetching is also asynchronous and best-effort; it may not complete
// for all bookmarks if the profile is destroyed before the fetches complete.
//
// Refer to `InitialPreferences::GetBookmarksBlock` for the expected format of
// `bookmark_dict`. If `bookmark_dict` is not aligned with the contract it is
// skipped, same with individual nodes/folders.
void StartBookmarkImportFromDict(Profile* profile,
                                 base::Value::Dict bookmarks_dict);

}  // namespace first_run

#endif  // CHROME_BROWSER_FIRST_RUN_BOOKMARK_IMPORTER_H_
