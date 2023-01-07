// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file provides functions for opening an item (file or directory) using
// the file manager.

#ifndef CHROME_BROWSER_ASH_FILE_MANAGER_OPEN_UTIL_H_
#define CHROME_BROWSER_ASH_FILE_MANAGER_OPEN_UTIL_H_

#include "base/functional/callback_forward.h"
#include "chrome/browser/platform_util.h"

class Profile;

namespace base {
class FilePath;
}

namespace file_manager::util {

// If |item_type| is OPEN_FILE: Opens an item using a file handler, a file
// browser handler, or the browser (open in a tab). The default handler has
// precedence over other handlers, if defined for the type of the target file.
//
// If |item_type| is OPEN_FOLDER: Open the directory at |file_path| using the
// file browser.
//
// It is an error for |file_path| to not match the type implied by |item_type|.
// This error will be reported to |callback|.
//
// If |callback| is null, shows an error message to the user indicating the
// error if the operation is unsuccessful. No error messages will be displayed
// if |callback| is non-null.
void OpenItem(Profile* profile,
              const base::FilePath& file_path,
              platform_util::OpenItemType item_type,
              platform_util::OpenOperationCallback callback);

// Opens the file manager for the folder containing the item specified by
// |file_path|, with the item selected.
void ShowItemInFolder(Profile* profile,
                      const base::FilePath& file_path,
                      platform_util::OpenOperationCallback callback);

// Change the behavior of the above functions to do everything except launch any
// extensions including a file browser.
void DisableShellOperationsForTesting();

}  // namespace file_manager::util

#endif  // CHROME_BROWSER_ASH_FILE_MANAGER_OPEN_UTIL_H_
