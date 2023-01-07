// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_ARC_DEMO_MODE_DELEGATE_IMPL_H_
#define CHROME_BROWSER_ASH_ARC_ARC_DEMO_MODE_DELEGATE_IMPL_H_

#include "ash/components/arc/session/arc_client_adapter.h"
#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"

namespace arc {

class ArcDemoModeDelegateImpl : public ArcClientAdapter::DemoModeDelegate {
 public:
  ArcDemoModeDelegateImpl() = default;
  ~ArcDemoModeDelegateImpl() override = default;
  ArcDemoModeDelegateImpl(const ArcDemoModeDelegateImpl&) = delete;
  ArcDemoModeDelegateImpl& operator=(const ArcDemoModeDelegateImpl&) = delete;

  // ArcClientAdapter::DemoModeDelegate overrides:
  void EnsureResourcesLoaded(base::OnceClosure callback) override;
  base::FilePath GetDemoAppsPath() override;
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_ARC_DEMO_MODE_DELEGATE_IMPL_H_
