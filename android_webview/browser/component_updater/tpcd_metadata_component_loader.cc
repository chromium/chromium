// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/component_updater/tpcd_metadata_component_loader.h"

#include <memory>
#include <string>
#include <vector>

#include "android_webview/common/aw_switches.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "components/component_updater/android/loader_policies/tpcd_metadata_component_loader_policy.h"
#include "components/tpcd/metadata/browser/parser.h"
#include "services/network/public/cpp/features.h"

namespace android_webview {

// Add TpcdMetadataComponentInstallerPolicy to the given policies vector, if
// the component is enabled.
void LoadTpcMetadataComponent(ComponentLoaderPolicyVector& policies) {
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kWebViewTpcdMetadaComponent)) {
    return;
  }

  DVLOG(1) << "Registering TPCD metadata component for loading in "
              "embedded WebView.";

  policies.push_back(
      std::make_unique<component_updater::TpcdMetadataComponentLoaderPolicy>(
          base::BindRepeating([](const std::string& raw_metadata) {
            tpcd::metadata::Parser::GetInstance()->ParseMetadata(raw_metadata);
          })));
}

}  // namespace android_webview
