// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEW_TAB_PAGE_MODULES_MICROSOFT_MODULES_HELPER_H_
#define CHROME_BROWSER_NEW_TAB_PAGE_MODULES_MICROSOFT_MODULES_HELPER_H_

#include <string>

#include "url/gurl.h"

namespace microsoft_modules_helper {
std::string GetFileExtension(std::string mime_type);
// Remove the file extension that is appended to the file name. E.g. A file
// with the full name "Document.docx" will return "Document"
std::string GetFileName(std::string full_name, std::string file_extension);
// Maps files to their icon url.
GURL GetFileIconUrl(std::string mime_type);

}  // namespace microsoft_modules_helper

#endif  // CHROME_BROWSER_NEW_TAB_PAGE_MODULES_MICROSOFT_MODULES_HELPER_H_
