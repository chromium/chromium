// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/component_updater/first_party_sets_component_loader.h"

#include <memory>
#include <string>
#include <vector>

#include "android_webview/common/aw_switches.h"
#include "base/command_line.h"
#include "base/files/file.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/version.h"
#include "components/component_updater/android/loader_policies/first_party_sets_component_loader_policy.h"
#include "content/public/browser/first_party_sets_handler.h"
#include "services/network/public/cpp/features.h"

namespace android_webview {

void LoadFpsComponent(ComponentLoaderPolicyVector& policies) {
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kWebViewFpsComponent)) {
    return;
  }

  DVLOG(1) << "Registering first party sets component for loading in "
              "embedded WebView.";

  policies.push_back(
      std::make_unique<component_updater::FirstPartySetComponentLoaderPolicy>(
          base::BindRepeating([](base::Version version, base::File sets_file) {
            content::FirstPartySetsHandler::GetInstance()
                ->SetPublicFirstPartySets(version, std::move(sets_file));
          })));
}

}  // namespace android_webview
