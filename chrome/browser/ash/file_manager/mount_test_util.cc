// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_manager/mount_test_util.h"
#include "base/memory/raw_ptr.h"

#include "base/run_loop.h"
#include "chrome/browser/ash/drive/drive_integration_service.h"

namespace file_manager {
namespace test_util {

namespace {

// Helper class used to wait for |OnFileSystemMounted| event from a drive file
// system.
class DriveMountPointWaiter : public drive::DriveIntegrationService::Observer {
 public:
  explicit DriveMountPointWaiter(drive::DriveIntegrationService* service)
      : service_(service) {
    Observe(service_);
  }

  // DriveIntegrationService::Observer implementation.
  void OnFileSystemMounted() override {
    // Note that it is OK for |run_loop_.Quit| to be called before
    // |run_loop_.Run|. In this case |Run| will return immediately.
    run_loop_.Quit();
  }

  // Runs loop until the file system is mounted.
  void Wait() { run_loop_.Run(); }

 private:
  const raw_ptr<drive::DriveIntegrationService> service_;
  base::RunLoop run_loop_;
};

}  // namespace

void WaitUntilDriveMountPointIsAdded(Profile* profile) {
  DCHECK(profile);

  // Drive mount point is added by the browser when the drive system service
  // is first initialized. It is done asynchronously after some other parts of
  // the service are initialized (e.g. resource metadata and cache), thus racy
  // with the test start. To handle this raciness, the test verifies that
  // drive mount point is added before continuing. If this is not the case,
  // drive file system is observed for FileSystemMounted event (by
  // |mount_point_waiter|) and test continues once the event is encountered.
  drive::DriveIntegrationService* integration_service =
      drive::DriveIntegrationServiceFactory::FindForProfile(profile);
  DCHECK(integration_service);
  DCHECK(integration_service->is_enabled());

  if (integration_service->IsMounted()) {
    return;
  }

  DriveMountPointWaiter waiter(integration_service);
  VLOG(1) << "Waiting for drive mount point to get mounted.";
  waiter.Wait();
  VLOG(1) << "Drive mount point found.";
}

}  // namespace test_util
}  // namespace file_manager
