// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Utilities for ARC Print Spooler.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_PRINT_SPOOLER_ARC_PRINT_SPOOLER_UTIL_H_
#define CHROME_BROWSER_CHROMEOS_ARC_PRINT_SPOOLER_ARC_PRINT_SPOOLER_UTIL_H_

#include "mojo/public/cpp/system/platform_handle.h"

namespace base {
class FilePath;
}  // namespace base

namespace arc {

// Deletes a print document and logs any errors.
void DeletePrintDocument(const base::FilePath& file_path);

// Uses the provided scoped handle to save a print document from ARC and returns
// the document's file path.
base::FilePath SavePrintDocument(mojo::ScopedHandle scoped_handle);

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_PRINT_SPOOLER_ARC_PRINT_SPOOLER_UTIL_H_
