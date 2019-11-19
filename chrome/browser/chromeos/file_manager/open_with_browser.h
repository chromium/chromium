// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file provides utilities for opening files with the browser.

#ifndef CHROME_BROWSER_CHROMEOS_FILE_MANAGER_OPEN_WITH_BROWSER_H_
#define CHROME_BROWSER_CHROMEOS_FILE_MANAGER_OPEN_WITH_BROWSER_H_

#include "base/files/file_path.h"

class Profile;

namespace storage {
class FileSystemURL;
}

namespace file_manager {
namespace util {

// Opens the file specified by |file_path| with the browser for
// |profile|. This function takes care of the following intricacies:
//
// - If there is no active browser window, open it.
// - If the file is a Drive hosted document, the hosted document will be
//   opened in the browser by extracting the right URL for the file.
// - If the file is on Drive, the file will be downloaded from Drive as
//   needed.
//
// Returns false if failed to open. This happens if the file type is unknown.
bool OpenFileWithBrowser(Profile* profile,
                         const storage::FileSystemURL& file_system_url,
                         const std::string& action_id);

// Checks whether a pepper plugin for |file_extension| is enabled.
bool ShouldBeOpenedWithPlugin(Profile* profile,
                              const base::FilePath::StringType& file_extension,
                              const std::string& action_id);

}  // namespace util
}  // namespace file_manager

#endif  // CHROME_BROWSER_CHROMEOS_FILE_MANAGER_OPEN_WITH_BROWSER_H_
