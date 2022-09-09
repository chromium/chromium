// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_SYSTEM_PROVIDER_PROVIDER_FUNCTION_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_SYSTEM_PROVIDER_PROVIDER_FUNCTION_H_

#include <memory>
#include <string>

#include "base/files/file.h"
#include "chrome/common/extensions/api/file_system_provider.h"
#include "extensions/browser/extension_function.h"

namespace extensions {

// Error names from
// http://www.w3.org/TR/file-system-api/#errors-and-exceptions
extern const char kNotFoundErrorName[];
extern const char kSecurityErrorName[];

// Error messages.
extern const char kEmptyNameErrorMessage[];
extern const char kEmptyIdErrorMessage[];
extern const char kMountFailedErrorMessage[];
extern const char kUnmountFailedErrorMessage[];
extern const char kResponseFailedErrorMessage[];

// Creates an identifier from |error|. For FILE_OK, an empty string is returned.
// These values are passed to JavaScript as lastError.message value.
std::string FileErrorToString(base::File::Error error);

// Converts ProviderError to base::File::Error. This could be redundant, if it
// was possible to create DOMError instances in Javascript easily.
base::File::Error ProviderErrorToFileError(
    api::file_system_provider::ProviderError error);

}  // namespace extensions

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_SYSTEM_PROVIDER_PROVIDER_FUNCTION_H_
