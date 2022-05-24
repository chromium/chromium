// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_SYSTEM_PROVIDER_FILE_SYSTEM_PROVIDER_API_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_SYSTEM_PROVIDER_FILE_SYSTEM_PROVIDER_API_H_

#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/crosapi/file_system_provider_service_ash.h"
#include "chrome/browser/chromeos/extensions/file_system_provider/provider_function.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/crosapi/mojom/file_system_provider.mojom.h"
#include "extensions/browser/extension_function.h"

namespace extensions {

class FileSystemProviderBase : public ExtensionFunction {
 protected:
  ~FileSystemProviderBase() override {}
  void RespondWithError(const std::string& error);
};

class FileSystemProviderMountFunction : public FileSystemProviderBase {
 public:
  DECLARE_EXTENSION_FUNCTION("fileSystemProvider.mount",
                             FILESYSTEMPROVIDER_MOUNT)

 protected:
  ~FileSystemProviderMountFunction() override {}
  ResponseAction Run() override;
};

class FileSystemProviderUnmountFunction : public FileSystemProviderBase {
 public:
  DECLARE_EXTENSION_FUNCTION("fileSystemProvider.unmount",
                             FILESYSTEMPROVIDER_UNMOUNT)

 protected:
  ~FileSystemProviderUnmountFunction() override {}
  ResponseAction Run() override;
};

class FileSystemProviderGetAllFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileSystemProvider.getAll",
                             FILESYSTEMPROVIDER_GETALL)

 protected:
  void RespondWithInfos(std::vector<crosapi::mojom::FileSystemInfoPtr>);
  ~FileSystemProviderGetAllFunction() override {}
  ResponseAction Run() override;
};

class FileSystemProviderGetFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileSystemProvider.get", FILESYSTEMPROVIDER_GET)

 protected:
  void RespondWithInfo(crosapi::mojom::FileSystemInfoPtr info);
  ~FileSystemProviderGetFunction() override {}
  ResponseAction Run() override;
};

class FileSystemProviderNotifyFunction : public FileSystemProviderBase {
 public:
  DECLARE_EXTENSION_FUNCTION("fileSystemProvider.notify",
                             FILESYSTEMPROVIDER_NOTIFY)

 protected:
  ~FileSystemProviderNotifyFunction() override {}
  ResponseAction Run() override;

 private:
  // Called when notifying is completed.
  void OnNotifyCompleted(base::File::Error result);
};

class FileSystemProviderInternal : public FileSystemProviderBase {
 protected:
  ~FileSystemProviderInternal() override {}

  // Returns the operation metadata from FileSystemProviderInternal methods via
  // output parameters.
  template <typename Params>
  void GetOperationMetadata(const Params& params,
                            crosapi::mojom::FileSystemIdPtr* file_system_id,
                            int64_t* request_id) {
    *file_system_id = crosapi::mojom::FileSystemId::New();
    (*file_system_id)->provider = extension_id();
    (*file_system_id)->id = params->file_system_id;
    *request_id = params->request_id;
  }

  // Forwards the result of the operation to the file system provider service.
  template <typename Params>
  void ForwardOperationResult(const Params& params,
                              std::vector<base::Value>& args,
                              crosapi::mojom::FSPOperationResponse response) {
    crosapi::mojom::FileSystemIdPtr file_system_id;
    int64_t request_id;
    GetOperationMetadata(params, &file_system_id, &request_id);
    auto callback =
        base::BindOnce(&FileSystemProviderInternal::RespondWithError, this);
    crosapi::CrosapiManager::Get()
        ->crosapi_ash()
        ->file_system_provider_service_ash()
        ->OperationFinishedWithProfile(
            response, std::move(file_system_id), request_id, std::move(args),
            std::move(callback),
            Profile::FromBrowserContext(browser_context()));
  }
};

class FileSystemProviderInternalUnmountRequestedSuccessFunction
    : public FileSystemProviderInternal {
 public:
  DECLARE_EXTENSION_FUNCTION(
      "fileSystemProviderInternal.unmountRequestedSuccess",
      FILESYSTEMPROVIDERINTERNAL_UNMOUNTREQUESTEDSUCCESS)

 protected:
  ~FileSystemProviderInternalUnmountRequestedSuccessFunction() override {}
  ResponseAction Run() override;
};

class FileSystemProviderInternalGetMetadataRequestedSuccessFunction
    : public FileSystemProviderInternal {
 public:
  DECLARE_EXTENSION_FUNCTION(
      "fileSystemProviderInternal.getMetadataRequestedSuccess",
      FILESYSTEMPROVIDERINTERNAL_GETMETADATAREQUESTEDSUCCESS)

 protected:
  ~FileSystemProviderInternalGetMetadataRequestedSuccessFunction() override {}
  ResponseAction Run() override;
};

class FileSystemProviderInternalGetActionsRequestedSuccessFunction
    : public FileSystemProviderInternal {
 public:
  DECLARE_EXTENSION_FUNCTION(
      "fileSystemProviderInternal.getActionsRequestedSuccess",
      FILESYSTEMPROVIDERINTERNAL_GETACTIONSREQUESTEDSUCCESS)

 protected:
  ~FileSystemProviderInternalGetActionsRequestedSuccessFunction() override {}
  ResponseAction Run() override;
};

class FileSystemProviderInternalReadDirectoryRequestedSuccessFunction
    : public FileSystemProviderInternal {
 public:
  DECLARE_EXTENSION_FUNCTION(
      "fileSystemProviderInternal.readDirectoryRequestedSuccess",
      FILESYSTEMPROVIDERINTERNAL_READDIRECTORYREQUESTEDSUCCESS)

 protected:
  ~FileSystemProviderInternalReadDirectoryRequestedSuccessFunction() override {}
  ResponseAction Run() override;
};

class FileSystemProviderInternalReadFileRequestedSuccessFunction
    : public FileSystemProviderInternal {
 public:
  DECLARE_EXTENSION_FUNCTION(
      "fileSystemProviderInternal.readFileRequestedSuccess",
      FILESYSTEMPROVIDERINTERNAL_READFILEREQUESTEDSUCCESS)

 protected:
  ~FileSystemProviderInternalReadFileRequestedSuccessFunction() override {}
  ResponseAction Run() override;
};

class FileSystemProviderInternalOperationRequestedSuccessFunction
    : public FileSystemProviderInternal {
 public:
  DECLARE_EXTENSION_FUNCTION(
      "fileSystemProviderInternal.operationRequestedSuccess",
      FILESYSTEMPROVIDERINTERNAL_OPERATIONREQUESTEDSUCCESS)

 protected:
  ~FileSystemProviderInternalOperationRequestedSuccessFunction() override {}
  ResponseAction Run() override;
};

class FileSystemProviderInternalOperationRequestedErrorFunction
    : public FileSystemProviderInternal {
 public:
  DECLARE_EXTENSION_FUNCTION(
      "fileSystemProviderInternal.operationRequestedError",
      FILESYSTEMPROVIDERINTERNAL_OPERATIONREQUESTEDERROR)

 protected:
  ~FileSystemProviderInternalOperationRequestedErrorFunction() override {}
  ResponseAction Run() override;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_SYSTEM_PROVIDER_FILE_SYSTEM_PROVIDER_API_H_
