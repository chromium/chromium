// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_SYSTEM_PROVIDER_FILE_SYSTEM_PROVIDER_API_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_SYSTEM_PROVIDER_FILE_SYSTEM_PROVIDER_API_H_

#include "base/files/file.h"
#include "base/values.h"
#include "chrome/common/extensions/api/file_system_provider_internal.h"
#include "chromeos/crosapi/mojom/file_system_provider.mojom.h"
#include "extensions/browser/extension_function.h"

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
  void RespondWithInfos(std::vector<crosapi::mojom::FileSystemInfoPtr>);
  ~FileSystemProviderGetAllFunction() override = default;
  ResponseAction Run() override;
};

class FileSystemProviderGetFunction : public FileSystemProviderBase {
 public:
  DECLARE_EXTENSION_FUNCTION("fileSystemProvider.get", FILESYSTEMPROVIDER_GET)

 protected:
  void RespondWithInfo(crosapi::mojom::FileSystemInfoPtr info);
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

  // Returns the operation metadata from FileSystemProviderInternal methods via
  // output parameters.
  template <typename Params>
  void GetOperationMetadata(const Params& params,
                            crosapi::mojom::FileSystemIdPtr* file_system_id,
                            int64_t* request_id) {
    *file_system_id = crosapi::mojom::FileSystemId::New();
    (*file_system_id)->provider = GetProviderId();
    (*file_system_id)->id = params->file_system_id;
    *request_id = params->request_id;
  }

  // Forwards the result of the mount request to the file system provider
  // service. Returns false if the forwarding failed.
  bool ForwardMountResult(int64_t request_id, base::Value::List& args);

  // Forwards the result of the operation to the file system provider service.
  // Returns false if the forwarding failed.
  template <typename Params>
  bool ForwardOperationResult(const Params& params,
                              base::Value::List& args,
                              crosapi::mojom::FSPOperationResponse response) {
    crosapi::mojom::FileSystemIdPtr file_system_id;
    int64_t request_id;
    GetOperationMetadata(params, &file_system_id, &request_id);
    return ForwardOperationResultImpl(response, std::move(file_system_id),
                                      request_id, std::move(args));
  }

  // Forwards the result of the `OpenFileSuccess` callback to the file system
  // provider service. Falls back to a generic success callback if the remote
  // interface doesn't support the optional `EntryMetadata` callback.
  bool ForwardOpenFileFinishedSuccessullyResult(
      std::optional<
          api::file_system_provider_internal::OpenFileRequestedSuccess::Params>
          params,
      base::Value::List& args);

 private:
  bool ForwardOperationResultImpl(
      crosapi::mojom::FSPOperationResponse response,
      crosapi::mojom::FileSystemIdPtr file_system_id,
      int request_id,
      base::Value::List args);
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
