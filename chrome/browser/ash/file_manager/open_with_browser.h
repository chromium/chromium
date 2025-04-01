// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file provides utilities for opening files with the browser.

#ifndef CHROME_BROWSER_ASH_FILE_MANAGER_OPEN_WITH_BROWSER_H_
#define CHROME_BROWSER_ASH_FILE_MANAGER_OPEN_WITH_BROWSER_H_

#include <optional>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "chrome/browser/apps/app_service/launch_result_type.h"
#include "url/gurl.h"

class Profile;

namespace storage {
class FileSystemURL;
}

namespace file_manager::util {

using LaunchAppCallback =
    base::OnceCallback<void(std::optional<apps::LaunchResult::State>)>;

// Opens the file specified by `file_path` with the browser for `profile`. In
// the event the `file_path` refers to a hosted document AND the document has an
// app installed, it will try to launch the app instead of a browser.

// Opens the file specified by |file_path| with the browser for
// |profile|. This function takes care of the following intricacies:
//
// - If there is no active browser window, open it.
// - If the file is a Drive hosted document, the hosted document will be
//   opened in the browser by extracting the right URL for the file.
// - If the file is a Drive hosted document, check if there is an installed app
// that can handle the document type, if it can launch the app instead.
// - If the file is on Drive, the file will be downloaded from Drive as
//   needed.
//
// Returns false if failed to open. This happens if the file type is unknown.
bool OpenFileWithAppOrBrowser(Profile* profile,
                              const storage::FileSystemURL& file_system_url,
                              const std::string& action_id,
                              LaunchAppCallback callback = base::DoNothing());

// Opens the hosted file specified by `file_path`, and its hosted path
// `hosted_url`, with its app if it's installed or in the browser otherwise.
bool OpenHostedFileInNewTabOrApp(Profile* profile,
                                 const base::FilePath& file_path,
                                 LaunchAppCallback callback,
                                 const GURL& hosted_url);

}  // namespace file_manager::util

#endif  // CHROME_BROWSER_ASH_FILE_MANAGER_OPEN_WITH_BROWSER_H_
