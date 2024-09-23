// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_OBSERVER_H_
#define CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_OBSERVER_H_

#include "base/files/file.h"

namespace ash::file_system_provider {

class ProvidedFileSystemInfo;

// Context for mounting. Whether happened due to user interaction or after
// a reboot, when restoring.
enum MountContext { MOUNT_CONTEXT_USER, MOUNT_CONTEXT_RESTORE };

// Observes file_system_provider::Service for mounting and unmounting events.
class Observer {
 public:
  virtual ~Observer() = default;

  // Called when a file system mounting has been invoked. For success, the
  // |error| argument is set to FILE_OK. Otherwise, |error| contains a specific
  // error code.
  virtual void OnProvidedFileSystemMount(
      const ProvidedFileSystemInfo& file_system_info,
      MountContext context,
      base::File::Error error) = 0;

  // Called when a file system unmounting has been invoked. For success, the
  // |error| argument is set to FILE_OK. Otherwise, |error| contains a specific
  // error code.
  virtual void OnProvidedFileSystemUnmount(
      const ProvidedFileSystemInfo& file_system_info,
      base::File::Error error) = 0;

  // Called when the observed file_system_provider::Service is being shutdown.
  virtual void OnShutDown() {}
};

}  // namespace ash::file_system_provider

#endif  // CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_OBSERVER_H_
