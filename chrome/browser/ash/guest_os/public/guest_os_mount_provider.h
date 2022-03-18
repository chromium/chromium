// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_GUEST_OS_PUBLIC_GUEST_OS_MOUNT_PROVIDER_H_
#define CHROME_BROWSER_ASH_GUEST_OS_PUBLIC_GUEST_OS_MOUNT_PROVIDER_H_

#include <string>
#include "base/callback_forward.h"
#include "base/files/file_path.h"
#include "chrome/browser/ash/crostini/crostini_util.h"

class Profile;

namespace guest_os {

class GuestOsMountProviderInner;
class GuestOsMountProvider {
 public:
  GuestOsMountProvider();
  virtual ~GuestOsMountProvider();

  GuestOsMountProvider(const GuestOsMountProvider&) = delete;
  GuestOsMountProvider& operator=(const GuestOsMountProvider&) = delete;

  // Get the profile for this provider.
  virtual Profile* profile() = 0;

  // The localised name to show in UI elements such as the files app sidebar.
  virtual std::string DisplayName() = 0;

  virtual crostini::ContainerId ContainerId() = 0;

  // TODO(crbug/1293229): How exactly we perform an SFTP mount is TBD, so these
  // are subject to change. For now we put random fake values in so we don't
  // need to keep changing subclasses as we figure out the format. Assuming we
  // keep a similar format to now we get cid from concierge or cicerone, port
  // from garcon and homedir is either hardcoded or from tremplin or garcon.
  virtual int cid();
  virtual int port();
  virtual base::FilePath homedir();

  // Requests the provider to mount its volume.
  void Mount(base::OnceCallback<void(bool)> callback);

  // Requests the provider to unmount.
  void Unmount();

 private:
  std::unique_ptr<GuestOsMountProviderInner> callback_;
};

}  // namespace guest_os

#endif  // CHROME_BROWSER_ASH_GUEST_OS_PUBLIC_GUEST_OS_MOUNT_PROVIDER_H_
