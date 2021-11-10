// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_DRIVEFS_FAKE_DRIVEFS_LAUNCHER_CLIENT_H_
#define ASH_COMPONENTS_DRIVEFS_FAKE_DRIVEFS_LAUNCHER_CLIENT_H_

#include "ash/components/drivefs/mojom/fake_drivefs_launcher.mojom.h"
#include "base/component_export.h"
#include "base/files/file_path.h"
#include "base/no_destructor.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace drivefs {

class FakeDriveFsLauncherClient {
 public:
  COMPONENT_EXPORT(DRIVEFS)
  static void Init(const base::FilePath& socket_path,
                   const base::FilePath& chroot_path);

  FakeDriveFsLauncherClient(const FakeDriveFsLauncherClient&) = delete;
  FakeDriveFsLauncherClient& operator=(const FakeDriveFsLauncherClient&) =
      delete;

 private:
  friend class base::NoDestructor<FakeDriveFsLauncherClient>;

  FakeDriveFsLauncherClient(const base::FilePath& chroot_path,
                            const base::FilePath& socket_path);
  ~FakeDriveFsLauncherClient();

  base::FilePath MaybeMountDriveFs(
      const std::string& source_path,
      const std::vector<std::string>& mount_options);

  const base::FilePath chroot_path_;
  const base::FilePath socket_path_;

  mojo::Remote<mojom::FakeDriveFsLauncher> launcher_;
};

}  // namespace drivefs

#endif  // ASH_COMPONENTS_DRIVEFS_FAKE_DRIVEFS_LAUNCHER_CLIENT_H_
