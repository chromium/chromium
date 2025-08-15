// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_SYSTEM_PROVIDER_FILE_SYSTEM_PROVIDER_API_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_SYSTEM_PROVIDER_FILE_SYSTEM_PROVIDER_API_H_

#include <variant>

#include "base/files/file.h"
#include "base/values.h"
#include "chrome/common/extensions/api/file_system_provider_internal.h"
#include "extensions/browser/extension_function.h"

namespace ash::file_system_provider {
class RequestValue;
}  // namespace ash::file_system_provider

namespace extensions {

class FileSystemProviderBase : public ExtensionFunction {
 protected:
  ~FileSystemProviderBase() override = default;
  std::string GetProviderId() const;
  void RespondWithError(const std::string& error);
};

class FileSystemProviderMountFunction : public FileSystemProviderBase {
 public:
  DECLARE_EXTENSION_FUNCTION("fileSystemProvider.mount",
                             FILESYSTEMPROVIDER_MOUNT)

 protected:
  ~FileSystemProviderMountFunction() override = default;
  ResponseAction Run() override;
};

class FileSystemProviderUnmountFunction : public FileSystemProviderBase {
 public:
  DECLARE_EXTENSION_FUNCTION("fileSystemProvider.unmount",
                             FILESYSTEMPROVIDER_UNMOUNT)

 protected:
  ~FileSystemProviderUnmountFunction() override = default;
  ResponseAction Run() override;
};

class FileSystemProviderGetAllFunction : public FileSystemProviderBase {
 public:
  DECLARE_EXTENSION_FUNCTION("fileSystemProvider.getAll",
                             FILESYSTEMPROVIDER_GETALL)

 protected:
  ~FileSystemProviderGetAllFunction() override = default;
  ResponseAction Run() override;
};

class FileSystemProviderGetFunction : public FileSystemProviderBase {
 public:
  DECLARE_EXTENSION_FUNCTION("fileSystemProvider.get", FILESYSTEMPROVIDER_GET)

 protected:
  ~FileSystemProviderGetFunction() override = default;
  ResponseAction Run() override;
};

class FileSystemProviderNotifyFunction : public FileSystemProviderBase {
 public:
  DECLARE_EXTENSION_FUNCTION("fileSystemProvider.notify",
                             FILESYSTEMPROVIDER_NOTIFY)

 protected:
  ~FileSystemProviderNotifyFunction() override = default;
  ResponseAction Run() override;

 private:
  // Called when notifying is completed.
  void OnNotifyCompleted(base::File::Error result);
};

class FileSystemProviderInternal : public FileSystemProviderBase {
 protected:
  ~FileSystemProviderInternal() override = default;

  // Forwards the result of the operation to the file system provider service.
  ResponseAction ForwardOperationResult(
      const std::string& file_system_id,
      int64_t request_id,
      const ash::file_system_provider::RequestValue& value,
      std::variant<bool /*has_more*/, base::File::Error /*error*/> arg);
};

class FileSystemProviderInternalRespondToMountRequestFunction
    : public FileSystemProviderInternal {
 public:
  DECLARE_EXTENSION_FUNCTION("fileSystemProviderInternal.respondToMountRequest",
                             FILESYSTEMPROVIDERINTERNAL_RESPONDTOMOUNTREQUEST)

 protected:
  ~FileSystemProviderInternalRespondToMountRequestFunction() override = default;
  ResponseAction Run() override;
};

class FileSystemProviderInternalUnmountRequestedSuccessFunction
    : public FileSystemProviderInternal {
 public:
  DECLARE_EXTENSION_FUNCTION(
      "fileSystemProviderInternal.unmountRequestedSuccess",
      FILESYSTEMPROVIDERINTERNAL_UNMOUNTREQUESTEDSUCCESS)

 protected:
  ~FileSystemProviderInternalUnmountRequestedSuccessFunction() override =
      default;
  ResponseAction Run() override;
};

class FileSystemProviderInternalGetMetadataRequestedSuccessFunction
    : public FileSystemProviderInternal {
 public:
  DECLARE_EXTENSION_FUNCTION(
      "fileSystemProviderInternal.getMetadataRequestedSuccess",
      FILESYSTEMPROVIDERINTERNAL_GETMETADATAREQUESTEDSUCCESS)

 protected:
  ~FileSystemProviderInternalGetMetadataRequestedSuccessFunction() override =
      default;
  ResponseAction Run() override;
};

class FileSystemProviderInternalGetActionsRequestedSuccessFunction
    : public FileSystemProviderInternal {
 public:
  DECLARE_EXTENSION_FUNCTION(
      "fileSystemProviderInternal.getActionsRequestedSuccess",
      FILESYSTEMPROVIDERINTERNAL_GETACTIONSREQUESTEDSUCCESS)

 protected:
  ~FileSystemProviderInternalGetActionsRequestedSuccessFunction() override =
      default;
  ResponseAction Run() override;
};

class FileSystemProviderInternalReadDirectoryRequestedSuccessFunction
    : public FileSystemProviderInternal {
 public:
  DECLARE_EXTENSION_FUNCTION(
      "fileSystemProviderInternal.readDirectoryRequestedSuccess",
      FILESYSTEMPROVIDERINTERNAL_READDIRECTORYREQUESTEDSUCCESS)

 protected:
  ~FileSystemProviderInternalReadDirectoryRequestedSuccessFunction() override =
      default;
  ResponseAction Run() override;
};

class FileSystemProviderInternalReadFileRequestedSuccessFunction
    : public FileSystemProviderInternal {
 public:
  DECLARE_EXTENSION_FUNCTION(
      "fileSystemProviderInternal.readFileRequestedSuccess",
      FILESYSTEMPROVIDERINTERNAL_READFILEREQUESTEDSUCCESS)

 protected:
  ~FileSystemProviderInternalReadFileRequestedSuccessFunction() override =
      default;
  ResponseAction Run() override;
};

class FileSystemProviderInternalOpenFileRequestedSuccessFunction
    : public FileSystemProviderInternal {
 public:
  DECLARE_EXTENSION_FUNCTION(
      "fileSystemProviderInternal.openFileRequestedSuccess",
      FILESYSTEMPROVIDERINTERNAL_OPENFILEREQUESTEDSUCCESS)

 protected:
  ~FileSystemProviderInternalOpenFileRequestedSuccessFunction() override =
      default;
  ResponseAction Run() override;
};

class FileSystemProviderInternalOperationRequestedSuccessFunction
    : public FileSystemProviderInternal {
 public:
  DECLARE_EXTENSION_FUNCTION(
      "fileSystemProviderInternal.operationRequestedSuccess",
      FILESYSTEMPROVIDERINTERNAL_OPERATIONREQUESTEDSUCCESS)

 protected:
  ~FileSystemProviderInternalOperationRequestedSuccessFunction() override =
      default;
  ResponseAction Run() override;
};

class FileSystemProviderInternalOperationRequestedErrorFunction
    : public FileSystemProviderInternal {
 public:
  DECLARE_EXTENSION_FUNCTION(
      "fileSystemProviderInternal.operationRequestedError",
      FILESYSTEMPROVIDERINTERNAL_OPERATIONREQUESTEDERROR)

 protected:
  ~FileSystemProviderInternalOperationRequestedErrorFunction() override =
      default;
  ResponseAction Run() override;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_SYSTEM_PROVIDER_FILE_SYSTEM_PROVIDER_API_H_
