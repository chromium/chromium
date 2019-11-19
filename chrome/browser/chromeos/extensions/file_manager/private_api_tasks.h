// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file provides task related API functions.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_MANAGER_PRIVATE_API_TASKS_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_MANAGER_PRIVATE_API_TASKS_H_

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "chrome/browser/chromeos/extensions/file_manager/private_api_base.h"
#include "chrome/browser/chromeos/file_manager/file_tasks.h"
#include "chrome/browser/extensions/chrome_extension_function_details.h"

namespace base {
class FilePath;
}  // namespace base

namespace extensions {

namespace app_file_handler_util {
class IsDirectoryCollector;
class MimeTypeCollector;
}  // namespace app_file_handler_util

// Implements the chrome.fileManagerPrivateInternal.executeTask method.
class FileManagerPrivateInternalExecuteTaskFunction
    : public LoggedExtensionFunction {
 public:
  FileManagerPrivateInternalExecuteTaskFunction();

  DECLARE_EXTENSION_FUNCTION("fileManagerPrivateInternal.executeTask",
                             FILEMANAGERPRIVATEINTERNAL_EXECUTETASK)

 protected:
  ~FileManagerPrivateInternalExecuteTaskFunction() override = default;

  // ExtensionFunction overrides.
  ResponseAction Run() override;

 private:
  void OnTaskExecuted(extensions::api::file_manager_private::TaskResult success,
                      std::string failure_reason);

  const ChromeExtensionFunctionDetails chrome_details_;
};

// Implements the chrome.fileManagerPrivateInternal.getFileTasks method.
class FileManagerPrivateInternalGetFileTasksFunction
    : public LoggedExtensionFunction {
 public:
  FileManagerPrivateInternalGetFileTasksFunction();

  DECLARE_EXTENSION_FUNCTION("fileManagerPrivateInternal.getFileTasks",
                             FILEMANAGERPRIVATEINTERNAL_GETFILETASKS)

 protected:
  ~FileManagerPrivateInternalGetFileTasksFunction() override;

  // ExtensionFunction overrides.
  ResponseAction Run() override;

 private:
  void OnMimeTypesCollected(
      std::unique_ptr<std::vector<std::string>> mime_types);

  void OnAreDirectoriesAndMimeTypesCollected(
      std::unique_ptr<std::vector<std::string>> mime_types,
      std::unique_ptr<std::set<base::FilePath>> path_directory_set);

  void OnFileTasksListed(
      std::unique_ptr<std::vector<file_manager::file_tasks::FullTaskDescriptor>>
          tasks);

  std::unique_ptr<app_file_handler_util::IsDirectoryCollector>
      is_directory_collector_;
  std::unique_ptr<app_file_handler_util::MimeTypeCollector>
      mime_type_collector_;
  std::vector<GURL> urls_;
  std::vector<base::FilePath> local_paths_;
  const ChromeExtensionFunctionDetails chrome_details_;
};

// Implements the chrome.fileManagerPrivateInternal.setDefaultTask method.
class FileManagerPrivateInternalSetDefaultTaskFunction
    : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileManagerPrivateInternal.setDefaultTask",
                             FILEMANAGERPRIVATEINTERNAL_SETDEFAULTTASK)

 protected:
  ~FileManagerPrivateInternalSetDefaultTaskFunction() override = default;

  // ExtensionFunction:
  ResponseAction Run() override;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_MANAGER_PRIVATE_API_TASKS_H_
