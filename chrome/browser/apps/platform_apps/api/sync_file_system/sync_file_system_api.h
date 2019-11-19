// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_PLATFORM_APPS_API_SYNC_FILE_SYSTEM_SYNC_FILE_SYSTEM_API_H_
#define CHROME_BROWSER_APPS_PLATFORM_APPS_API_SYNC_FILE_SYSTEM_SYNC_FILE_SYSTEM_API_H_

#include <stdint.h>

#include <map>

#include "chrome/browser/extensions/chrome_extension_function.h"
#include "chrome/browser/sync_file_system/conflict_resolution_policy.h"
#include "chrome/browser/sync_file_system/sync_file_status.h"
#include "chrome/browser/sync_file_system/sync_status_code.h"
#include "storage/browser/file_system/file_system_url.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom.h"

namespace storage {
class FileSystemContext;
}

namespace chrome_apps {
namespace api {

class SyncFileSystemDeleteFileSystemFunction
    : public ChromeAsyncExtensionFunction {
 public:
  // TODO(kinuko,calvinlo): Uncomment this or delete this class when
  // we decide if we want to revive this function.
  // DECLARE_EXTENSION_FUNCTION("syncFileSystem.deleteFileSystem",
  //                            SYNCFILESYSTEM_DELETEFILESYSTEM)

 protected:
  ~SyncFileSystemDeleteFileSystemFunction() override {}
  bool RunAsync() override;

 private:
  void DidDeleteFileSystem(base::File::Error error);
};

class SyncFileSystemGetFileStatusFunction
    : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("syncFileSystem.getFileStatus",
                             SYNCFILESYSTEM_GETFILESYNCSTATUS)

 protected:
  ~SyncFileSystemGetFileStatusFunction() override {}
  bool RunAsync() override;

 private:
  void DidGetFileStatus(
      const ::sync_file_system::SyncStatusCode sync_service_status,
      const ::sync_file_system::SyncFileStatus sync_file_status);
};

class SyncFileSystemGetFileStatusesFunction
    : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("syncFileSystem.getFileStatuses",
                             SYNCFILESYSTEM_GETFILESYNCSTATUSES)
  SyncFileSystemGetFileStatusesFunction();

 protected:
  ~SyncFileSystemGetFileStatusesFunction() override;
  bool RunAsync() override;

 private:
  typedef std::pair<::sync_file_system::SyncStatusCode,
                    ::sync_file_system::SyncFileStatus>
      FileStatusPair;
  typedef std::map<storage::FileSystemURL,
                   FileStatusPair,
                   storage::FileSystemURL::Comparator>
      URLToStatusMap;

  void DidGetFileStatus(const storage::FileSystemURL& file_system_url,
                        ::sync_file_system::SyncStatusCode sync_status_code,
                        ::sync_file_system::SyncFileStatus sync_file_statuses);

  unsigned int num_expected_results_;
  unsigned int num_results_received_;
  URLToStatusMap file_sync_statuses_;
};

class SyncFileSystemGetUsageAndQuotaFunction
    : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("syncFileSystem.getUsageAndQuota",
                             SYNCFILESYSTEM_GETUSAGEANDQUOTA)

 protected:
  ~SyncFileSystemGetUsageAndQuotaFunction() override {}
  bool RunAsync() override;

 private:
  void DidGetUsageAndQuota(blink::mojom::QuotaStatusCode status,
                           int64_t usage,
                           int64_t quota);
};

class SyncFileSystemRequestFileSystemFunction
    : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("syncFileSystem.requestFileSystem",
                             SYNCFILESYSTEM_REQUESTFILESYSTEM)

 protected:
  ~SyncFileSystemRequestFileSystemFunction() override {}
  bool RunAsync() override;

 private:
  typedef SyncFileSystemRequestFileSystemFunction self;

  // Returns the file system context for this extension.
  storage::FileSystemContext* GetFileSystemContext();

  void DidOpenFileSystem(const GURL& root_url,
                         const std::string& file_system_name,
                         base::File::Error error);
};

class SyncFileSystemSetConflictResolutionPolicyFunction
    : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("syncFileSystem.setConflictResolutionPolicy",
                             SYNCFILESYSTEM_SETCONFLICTRESOLUTIONPOLICY)

 protected:
  ~SyncFileSystemSetConflictResolutionPolicyFunction() override {}
  ResponseAction Run() override;
};

class SyncFileSystemGetConflictResolutionPolicyFunction
    : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("syncFileSystem.getConflictResolutionPolicy",
                             SYNCFILESYSTEM_GETCONFLICTRESOLUTIONPOLICY)

 protected:
  ~SyncFileSystemGetConflictResolutionPolicyFunction() override {}
  ResponseAction Run() override;
};

class SyncFileSystemGetServiceStatusFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("syncFileSystem.getServiceStatus",
                             SYNCFILESYSTEM_GETSERVICESTATUS)

 protected:
  ~SyncFileSystemGetServiceStatusFunction() override {}
  ResponseAction Run() override;
};

}  // namespace api
}  // namespace chrome_apps

#endif  // CHROME_BROWSER_APPS_PLATFORM_APPS_API_SYNC_FILE_SYSTEM_SYNC_FILE_SYSTEM_API_H_
