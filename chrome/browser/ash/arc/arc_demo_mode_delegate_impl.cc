// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/arc_demo_mode_delegate_impl.h"

#include <utility>

#include "chrome/browser/ash/login/demo_mode/demo_components.h"
#include "chrome/browser/ash/login/demo_mode/demo_session.h"

namespace arc {

void ArcDemoModeDelegateImpl::EnsureResourcesLoaded(
    base::OnceClosure callback) {
  if (!ash::DemoSession::IsDeviceInDemoMode()) {
    std::move(callback).Run();
    return;
  }
  ash::DemoSession::Get()->EnsureResourcesLoaded(std::move(callback));
}

base::FilePath ArcDemoModeDelegateImpl::GetDemoAppsPath() {
  if (!ash::DemoSession::IsDeviceInDemoMode())
    return base::FilePath();
  return ash::DemoSession::Get()->components()->GetDemoAndroidAppsPath();
}

}  // namespace arc
