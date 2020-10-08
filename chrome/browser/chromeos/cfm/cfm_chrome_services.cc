// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cfm/cfm_chrome_services.h"

#include "chromeos/dbus/cfm/cfm_hotline_client.h"
#include "chromeos/services/cfm/public/features/features.h"

namespace chromeos {
namespace cfm {

void InitializeCfmServices() {
  if (!base::FeatureList::IsEnabled(
          chromeos::cfm::features::kCfmMojoServices) ||
      !CfmHotlineClient::Get()) {
    return;
  }

  // TODO(kdgwill) Add Initial CfM Chromium Service
}

}  // namespace cfm
}  // namespace chromeos
