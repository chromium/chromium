// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file provides utility functions for file browser handlers.
// https://developer.chrome.com/extensions/fileBrowserHandler.html

#ifndef CHROME_BROWSER_ASH_FILE_MANAGER_FILE_BROWSER_HANDLERS_H_
#define CHROME_BROWSER_ASH_FILE_MANAGER_FILE_BROWSER_HANDLERS_H_

#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "chrome/browser/ash/file_manager/file_tasks.h"

class Profile;

namespace extensions {
class Extension;
}

namespace storage {
class FileSystemURL;
}

namespace file_manager::file_browser_handlers {

// Executes a file browser handler specified by |extension| of the given
// action ID for |file_urls|. Returns false if undeclared handlers are
// found. |done| is on completion. See also the comment at ExecuteFileTask()
// for other parameters.
bool ExecuteFileBrowserHandler(
    Profile* profile,
    const extensions::Extension* extension,
    const std::string& action_id,
    const std::vector<storage::FileSystemURL>& file_urls,
    file_tasks::FileTaskFinishedCallback done);

}  // namespace file_manager::file_browser_handlers

#endif  // CHROME_BROWSER_ASH_FILE_MANAGER_FILE_BROWSER_HANDLERS_H_
