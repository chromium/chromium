// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/publishers/extension_apps_enable_flow.h"

#include <utility>

#include "chrome/browser/ui/extensions/extension_enable_flow.h"

namespace apps {

ExtensionAppsEnableFlow::ExtensionAppsEnableFlow(Profile* profile,
                                                 const std::string& app_id)
    : profile_(profile), app_id_(app_id) {}

ExtensionAppsEnableFlow::~ExtensionAppsEnableFlow() = default;
void ExtensionAppsEnableFlow::Run(FinishedCallback callback) {
  callback_ = std::move(callback);

  if (!flow_) {
    flow_ = std::make_unique<ExtensionEnableFlow>(profile_, app_id_, this);
    flow_->Start();
  }
}

void ExtensionAppsEnableFlow::ExtensionEnableFlowFinished() {
  flow_.reset();
  if (!callback_.is_null()) {
    std::move(callback_).Run(/*success=*/true);
  }
}

void ExtensionAppsEnableFlow::ExtensionEnableFlowAborted(bool user_initiated) {
  flow_.reset();
  if (!callback_.is_null()) {
    std::move(callback_).Run(/*success=*/false);
  }
}
}  // namespace apps
