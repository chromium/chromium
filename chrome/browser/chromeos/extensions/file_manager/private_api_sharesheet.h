// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file provides task related API functions.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_MANAGER_PRIVATE_API_SHARESHEET_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_MANAGER_PRIVATE_API_SHARESHEET_H_

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "chrome/browser/chromeos/extensions/file_manager/private_api_base.h"
#include "chrome/browser/extensions/chrome_extension_function_details.h"
#include "storage/browser/file_system/file_system_url.h"

namespace base {
class File;
}

namespace extensions {

namespace api {
namespace file_manager_private {
struct EntryProperties;
}
}  // namespace api

namespace app_file_handler_util {
class IsDirectoryCollector;
class MimeTypeCollector;
}  // namespace app_file_handler_util

// Implements the chrome.fileManagerPrivateInternal.sharesheetHasTargets
// method.
class FileManagerPrivateInternalSharesheetHasTargetsFunction
    : public LoggedExtensionFunction {
 public:
  FileManagerPrivateInternalSharesheetHasTargetsFunction();

  DECLARE_EXTENSION_FUNCTION("fileManagerPrivateInternal.sharesheetHasTargets",
                             FILEMANAGERPRIVATEINTERNAL_SHARESHEETHASTARGETS)

 protected:
  ~FileManagerPrivateInternalSharesheetHasTargetsFunction() override;

  // ExtensionFunction overrides.
  ResponseAction Run() override;

 private:
  void OnMimeTypesCollected(
      std::unique_ptr<std::vector<std::string>> mime_types);

  void OnDrivePropertyCollected(
      std::unique_ptr<std::vector<std::string>> mime_types,
      std::unique_ptr<api::file_manager_private::EntryProperties> properties,
      base::File::Error error);

  void OnIsDirectoryCollected(
      std::unique_ptr<std::vector<std::string>> mime_types,
      std::unique_ptr<api::file_manager_private::EntryProperties> properties,
      std::unique_ptr<std::set<base::FilePath>> path_directory_set);

  std::unique_ptr<app_file_handler_util::MimeTypeCollector>
      mime_type_collector_;
  std::unique_ptr<app_file_handler_util::IsDirectoryCollector>
      is_directory_collector_;
  std::vector<GURL> urls_;
  const ChromeExtensionFunctionDetails chrome_details_;
  std::vector<storage::FileSystemURL> file_system_urls_;
  bool contains_hosted_document_ = false;
};

// Implements the chrome.fileManagerPrivateInternal.invokeSharesheet method.
class FileManagerPrivateInternalInvokeSharesheetFunction
    : public LoggedExtensionFunction {
 public:
  FileManagerPrivateInternalInvokeSharesheetFunction();

  DECLARE_EXTENSION_FUNCTION("fileManagerPrivateInternal.invokeSharesheet",
                             FILEMANAGERPRIVATEINTERNAL_INVOKESHARESHEET)

 protected:
  ~FileManagerPrivateInternalInvokeSharesheetFunction() override;

  // ExtensionFunction overrides.
  ResponseAction Run() override;

 private:
  void OnMimeTypesCollected(
      std::unique_ptr<std::vector<std::string>> mime_types);

  void OnDrivePropertyCollected(
      std::unique_ptr<std::vector<std::string>> mime_types,
      std::unique_ptr<api::file_manager_private::EntryProperties> properties,
      base::File::Error error);

  void OnIsDirectoryCollected(
      std::unique_ptr<std::vector<std::string>> mime_types,
      std::unique_ptr<api::file_manager_private::EntryProperties> properties,
      std::unique_ptr<std::set<base::FilePath>> path_directory_set);

  std::unique_ptr<app_file_handler_util::MimeTypeCollector>
      mime_type_collector_;
  std::unique_ptr<app_file_handler_util::IsDirectoryCollector>
      is_directory_collector_;
  std::vector<GURL> urls_;
  const ChromeExtensionFunctionDetails chrome_details_;
  std::vector<storage::FileSystemURL> file_system_urls_;
  bool contains_hosted_document_ = false;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_MANAGER_PRIVATE_API_SHARESHEET_H_
