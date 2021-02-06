// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/scoped_active_install.h"

#include "chrome/browser/extensions/active_install_data.h"

namespace extensions {

ScopedActiveInstall::ScopedActiveInstall(InstallTracker* tracker,
                                         const ActiveInstallData& install_data)
    : tracker_(tracker), extension_id_(install_data.extension_id) {
  Init();
  tracker_->AddActiveInstall(install_data);
}

ScopedActiveInstall::ScopedActiveInstall(InstallTracker* tracker,
                                         const std::string& extension_id)
    : tracker_(tracker), extension_id_(extension_id) {
  Init();
}

ScopedActiveInstall::~ScopedActiveInstall() {
  if (tracker_)
    tracker_->RemoveActiveInstall(extension_id_);
}

void ScopedActiveInstall::CancelDeregister() {
  tracker_observer_.RemoveAll();
  tracker_ = nullptr;
}

void ScopedActiveInstall::Init() {
  DCHECK(!extension_id_.empty());
  DCHECK(tracker_);
  tracker_observer_.Add(tracker_);
}

void ScopedActiveInstall::OnShutdown() {
  CancelDeregister();
}

}  // namespace extensions
