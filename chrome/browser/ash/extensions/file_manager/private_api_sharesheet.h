// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file provides task related API functions.

#ifndef CHROME_BROWSER_ASH_EXTENSIONS_FILE_MANAGER_PRIVATE_API_SHARESHEET_H_
#define CHROME_BROWSER_ASH_EXTENSIONS_FILE_MANAGER_PRIVATE_API_SHARESHEET_H_

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/files/file.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/extensions/file_manager/logged_extension_function.h"
#include "chrome/browser/sharesheet/sharesheet_metrics.h"
#include "chromeos/components/sharesheet/constants.h"
#include "storage/browser/file_system/file_system_url.h"

class Profile;

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

// Implements the chrome.fileManagerPrivate.sharesheetHasTargets
// method.
class FileManagerPrivateSharesheetHasTargetsFunction
    : public LoggedExtensionFunction {
 public:
  FileManagerPrivateSharesheetHasTargetsFunction();

  DECLARE_EXTENSION_FUNCTION("fileManagerPrivate.sharesheetHasTargets",
                             FILEMANAGERPRIVATEINTERNAL_SHARESHEETHASTARGETS)

 protected:
  ~FileManagerPrivateSharesheetHasTargetsFunction() override;

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
  raw_ptr<Profile> profile_ = nullptr;
  std::vector<storage::FileSystemURL> file_system_urls_;
};

// Implements the chrome.fileManagerPrivateInternal.invokeSharesheet method.
class FileManagerPrivateInvokeSharesheetFunction
    : public LoggedExtensionFunction {
 public:
  FileManagerPrivateInvokeSharesheetFunction();

  DECLARE_EXTENSION_FUNCTION("fileManagerPrivate.invokeSharesheet",
                             FILEMANAGERPRIVATEINTERNAL_INVOKESHARESHEET)

 protected:
  ~FileManagerPrivateInvokeSharesheetFunction() override;

  // ExtensionFunction overrides.
  ResponseAction Run() override;

 private:
  void OnMimeTypesCollected(
      sharesheet::LaunchSource launch_source,
      std::unique_ptr<std::vector<std::string>> mime_types);

  void OnDrivePropertyCollected(
      sharesheet::LaunchSource launch_source,
      std::unique_ptr<std::vector<std::string>> mime_types,
      std::unique_ptr<api::file_manager_private::EntryProperties> properties,
      base::File::Error error);

  void OnIsDirectoryCollected(
      sharesheet::LaunchSource launch_source,
      std::unique_ptr<std::vector<std::string>> mime_types,
      std::unique_ptr<api::file_manager_private::EntryProperties> properties,
      std::unique_ptr<std::set<base::FilePath>> path_directory_set);

  std::unique_ptr<app_file_handler_util::MimeTypeCollector>
      mime_type_collector_;
  std::unique_ptr<app_file_handler_util::IsDirectoryCollector>
      is_directory_collector_;
  std::vector<GURL> urls_;
  raw_ptr<Profile> profile_ = nullptr;
  std::vector<storage::FileSystemURL> file_system_urls_;
  std::vector<std::string> dlp_source_urls_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_ASH_EXTENSIONS_FILE_MANAGER_PRIVATE_API_SHARESHEET_H_
