// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARESHEET_SHARESHEET_TEST_UTIL_H_
#define CHROME_BROWSER_SHARESHEET_SHARESHEET_TEST_UTIL_H_

#include "base/files/file_path.h"
#include "components/services/app_service/public/cpp/intent.h"
#include "storage/browser/file_system/file_system_url.h"

class Profile;

namespace sharesheet {

const char kTestText[] = "text";
const char kTestTitle[] = "title";
const char kTestUrl[] = "https://fake-url.com/fake";
const char kTestTextFile[] = "path/to/text.txt";
const char kTestPdfFile[] = "path/to/file.pdf";
const char kMimeTypeText[] = "text/plain";
const char kMimeTypePdf[] = "application/pdf";

apps::IntentPtr CreateValidTextIntent();

apps::IntentPtr CreateValidUrlIntent();

apps::IntentPtr CreateInvalidIntent();

apps::IntentPtr CreateDriveIntent();

storage::FileSystemURL FileInDownloads(Profile* profile, base::FilePath file);

storage::FileSystemURL FileInNonNativeFileSystemType(Profile* profile,
                                                     base::FilePath file);

}  // namespace sharesheet

#endif  // CHROME_BROWSER_SHARESHEET_SHARESHEET_TEST_UTIL_H_
