// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_SYSTEM_PROVIDER_PROVIDER_FUNCTION_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_SYSTEM_PROVIDER_PROVIDER_FUNCTION_H_

#include <memory>
#include <string>

#include "base/files/file.h"
#include "chrome/browser/extensions/chrome_extension_function.h"
#include "chrome/common/extensions/api/file_system_provider.h"

namespace chromeos {
namespace file_system_provider {

class RequestManager;
class RequestValue;

}  // namespace file_system_provider
}  // namespace chromeos

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

// Base class for internal API functions handling request results, either
// a success or a failure.
class FileSystemProviderInternalFunction : public ExtensionFunction {
 public:
  FileSystemProviderInternalFunction();

 protected:
  ~FileSystemProviderInternalFunction() override {}

  // Rejects the request and returns a response for this API function.
  ResponseAction RejectRequest(
      std::unique_ptr<chromeos::file_system_provider::RequestValue> value,
      base::File::Error error);

  // Fulfills the request with parsed arguments of this API function
  // encapsulated as a RequestValue instance and returns a response.
  // If |has_more| is set to true, then the function will be called again for
  // this request.
  ResponseAction FulfillRequest(
      std::unique_ptr<chromeos::file_system_provider::RequestValue> value,
      bool has_more);

 private:
  // Guarantees |request_id_| and |request_manager_| are valid.
  bool PreRunValidation(std::string* error) override;

  int request_id_;
  chromeos::file_system_provider::RequestManager* request_manager_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_SYSTEM_PROVIDER_PROVIDER_FUNCTION_H_
