// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_FILE_SYSTEM_REMOTE_FILE_SYNC_SERVICE_H_
#define CHROME_BROWSER_SYNC_FILE_SYSTEM_REMOTE_FILE_SYNC_SERVICE_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/macros.h"
#include "chrome/browser/sync_file_system/conflict_resolution_policy.h"
#include "chrome/browser/sync_file_system/sync_callbacks.h"
#include "chrome/browser/sync_file_system/sync_file_metadata.h"
#include "storage/browser/file_system/file_system_url.h"

class BrowserContextKeyedServiceFactory;
class GURL;

namespace base {
class ListValue;
}

namespace content {
class BrowserContext;
}

namespace storage {
class ScopedFile;
}

namespace sync_file_system {

class FileStatusObserver;
class LocalChangeProcessor;
class RemoteChangeProcessor;
class TaskLogger;

enum RemoteServiceState {
  // Remote service is up and running, or has not seen any errors yet.
  // The consumer of this service can make new requests while the
  // service is in this state.
  REMOTE_SERVICE_OK = 0,

  // Remote service is temporarily unavailable due to network,
  // authentication or some other temporary failure.
  // This state may be automatically resolved when the underlying
  // network condition or service condition changes.
  // The consumer of this service can still make new requests but
  // they may fail (with recoverable error code).
  REMOTE_SERVICE_TEMPORARY_UNAVAILABLE,

  // Remote service is temporarily unavailable due to authentication failure.
  // This state may be automatically resolved when the authentication token
  // has been refreshed internally (e.g. when the user signed in etc).
  // The consumer of this service can still make new requests but
  // they may fail (with recoverable error code).
  REMOTE_SERVICE_AUTHENTICATION_REQUIRED,

  // Remote service is temporarily unavailable due to lack of API permissions.
  // This state may be automatically resolved when the API gets right
  // permissions to access with.
  // The consumer of this service can still make new requests but
  // they may fail (with recoverable error code).
  REMOTE_SERVICE_ACCESS_FORBIDDEN,

  // Remote service is disabled by configuration change or due to some
  // unrecoverable errors, e.g. local database corruption.
  // Any new requests will immediately fail when the service is in
  // this state.
  REMOTE_SERVICE_DISABLED,

  REMOTE_SERVICE_STATE_MAX,
};

// This class represents a backing service of the sync filesystem.
// This also maintains conflict information, i.e. a list of conflicting files
// (at least in the current design).
// Owned by SyncFileSystemService.
class RemoteFileSyncService {
 public:
  class Observer {
   public:
    Observer() {}
    virtual ~Observer() {}

    // This is called when RemoteFileSyncService updates its internal queue
    // of pending remote changes.
    // |pending_changes_hint| indicates the pending queue length to help sync
    // scheduling but the value may not be accurately reflect the real-time
    // value.
    virtual void OnRemoteChangeQueueUpdated(int64_t pending_changes_hint) = 0;

    // This is called when RemoteFileSyncService updates its state.
    virtual void OnRemoteServiceStateUpdated(
        RemoteServiceState state,
        const std::string& description) {}

   private:
    DISALLOW_COPY_AND_ASSIGN(Observer);
  };

  struct Version {
    std::string id;
    SyncFileMetadata metadata;
  };

  enum UninstallFlag {
    UNINSTALL_AND_PURGE_REMOTE,
    UNINSTALL_AND_KEEP_REMOTE,
  };

  // For GetOriginStatusMap.
  typedef std::map<GURL, std::string> OriginStatusMap;
  typedef base::Callback<void(std::unique_ptr<OriginStatusMap> status_map)>
      StatusMapCallback;

  // For GetRemoteVersions.
  typedef base::Callback<void(SyncStatusCode status,
                              const std::vector<Version>& versions)>
      RemoteVersionsCallback;
  typedef base::Callback<
      void(SyncStatusCode status, storage::ScopedFile downloaded)>
      DownloadVersionCallback;

  // For DumpFile.
  typedef base::Callback<void(std::unique_ptr<base::ListValue> list)>
      ListCallback;

  // Creates an initialized RemoteFileSyncService for backend |version|
  // for |context|.
  static std::unique_ptr<RemoteFileSyncService> CreateForBrowserContext(
      content::BrowserContext* context,
      TaskLogger* task_logger);

  // Returns BrowserContextKeyedServiceFactory's an instance of
  // RemoteFileSyncService for backend |version| depends on.
  static void AppendDependsOnFactories(
      std::set<BrowserContextKeyedServiceFactory*>* factories);

  RemoteFileSyncService() {}
  virtual ~RemoteFileSyncService() {}

  // Adds and removes observers.
  virtual void AddServiceObserver(Observer* observer) = 0;
  virtual void AddFileStatusObserver(FileStatusObserver* observer) = 0;

  // Registers |origin| to track remote side changes for the |origin|.
  // Upon completion, invokes |callback|.
  // The caller may call this method again when the remote service state
  // migrates to REMOTE_SERVICE_OK state if the error code returned via
  // |callback| was retriable ones.
  virtual void RegisterOrigin(
      const GURL& origin,
      const SyncStatusCallback& callback) = 0;

  // Re-enables |origin| that was previously disabled. If |origin| is not a
  // SyncFS app, then the origin is effectively ignored.
  virtual void EnableOrigin(
      const GURL& origin,
      const SyncStatusCallback& callback) = 0;

  virtual void DisableOrigin(
      const GURL& origin,
      const SyncStatusCallback& callback) = 0;

  // Uninstalls the |origin| by deleting its remote data copy and then removing
  // the origin from the metadata store.
  virtual void UninstallOrigin(
      const GURL& origin,
      UninstallFlag flag,
      const SyncStatusCallback& callback) = 0;

  // Called by the sync engine to process one remote change.
  // After a change is processed |callback| will be called (to return
  // the control to the sync engine).
  // It is invalid to call this before calling SetRemoteChangeProcessor().
  virtual void ProcessRemoteChange(const SyncFileCallback& callback) = 0;

  // Sets a remote change processor.  This must be called before any
  // ProcessRemoteChange().
  virtual void SetRemoteChangeProcessor(
      RemoteChangeProcessor* processor) = 0;

  // Returns a LocalChangeProcessor that applies a local change to the remote
  // storage backed by this service.
  virtual LocalChangeProcessor* GetLocalChangeProcessor() = 0;

  // Returns the current remote service state (should equal to the value
  // returned by the last OnRemoteServiceStateUpdated notification.
  virtual RemoteServiceState GetCurrentState() const = 0;

  // Returns all origins along with an arbitrary string description of their
  // corresponding sync statuses.
  virtual void GetOriginStatusMap(const StatusMapCallback& callback) = 0;

  // Returns file metadata for |origin| to call |callback|.
  virtual void DumpFiles(const GURL& origin,
                         const ListCallback& callback) = 0;

  // Returns the dump of internal database.
  virtual void DumpDatabase(const ListCallback& callback) = 0;

  // Enables or disables the background sync.
  // Setting this to false should disable the synchronization (and make
  // the service state to REMOTE_SERVICE_DISABLED), while setting this to
  // true does not necessarily mean the service is actually turned on
  // (for example if Chrome is offline the service state will become
  // REMOTE_SERVICE_TEMPORARY_UNAVAILABLE).
  virtual void SetSyncEnabled(bool enabled) = 0;

  virtual void PromoteDemotedChanges(const base::Closure& callback) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(RemoteFileSyncService);
};

}  // namespace sync_file_system

#endif  // CHROME_BROWSER_SYNC_FILE_SYSTEM_REMOTE_FILE_SYNC_SERVICE_H_
