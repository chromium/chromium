// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_FILE_SYSTEM_SYNC_SERVICE_STATE_H_
#define CHROME_BROWSER_SYNC_FILE_SYSTEM_SYNC_SERVICE_STATE_H_

namespace sync_file_system {

// This enum is translated into syncFileSystem.ServiceStatus
// (defined in chrome/common/extensions/api/sync_file_system.idl).
// When you update this enum please consider updating the other enum in IDL.
enum SyncServiceState {
  // The sync service is up and running, or has not seen any errors yet.
  // The consumer of this service can make new requests while the
  // service is in this state.
  SYNC_SERVICE_RUNNING,

  // The sync service is not synchronizing files because the remote service
  // needs to be authenticated by the user to proceed.
  // This state may be automatically resolved when the underlying
  // network condition or service condition changes.
  // The consumer of this service can still make new requests but
  // they may fail (with recoverable error code).
  SYNC_SERVICE_AUTHENTICATION_REQUIRED,

  // The sync service is not synchronizing files because the remote service
  // is (temporarily) unavailable due to some recoverable errors, e.g.
  // network is offline, the remote service is down or not
  // reachable etc. More details should be given by |description| parameter
  // in OnSyncStateUpdated (which could contain service-specific details).
  SYNC_SERVICE_TEMPORARY_UNAVAILABLE,

  // The sync service is disabled by configuration change or due to some
  // unrecoverable errors, e.g. local database corruption.
  // Any new requests will immediately fail when the service is in
  // this state.
  SYNC_SERVICE_DISABLED,
};

}  // namespace sync_file_system

#endif  // CHROME_BROWSER_SYNC_FILE_SYSTEM_SYNC_SERVICE_STATE_H_
