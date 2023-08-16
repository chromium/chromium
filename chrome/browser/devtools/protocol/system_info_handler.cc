// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/protocol/system_info_handler.h"

#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/common/dips_utils.h"

SystemInfoHandler::SystemInfoHandler(protocol::UberDispatcher* dispatcher) {
  protocol::SystemInfo::Dispatcher::wire(dispatcher, this);
}

SystemInfoHandler::~SystemInfoHandler() = default;

protocol::Response SystemInfoHandler::GetFeatureState(
    const std::string& in_featureState,
    bool* featureEnabled) {
  if (in_featureState == "DIPS") {
    *featureEnabled = base::FeatureList::IsEnabled(features::kDIPS) &&
                      features::kDIPSDeletionEnabled.Get() &&
                      (features::kDIPSTriggeringAction.Get() !=
                       content::DIPSTriggeringAction::kNone);
    return protocol::Response::Success();
  }

  return protocol::Response::FallThrough();
}
