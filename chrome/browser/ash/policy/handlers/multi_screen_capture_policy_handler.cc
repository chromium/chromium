// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/handlers/multi_screen_capture_policy_handler.h"

#include <string>

#include "chrome/browser/media/webrtc/capture_policy_utils.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "components/policy/core/browser/configuration_policy_handler.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"

namespace policy {

MultiScreenCapturePolicyHandler::MultiScreenCapturePolicyHandler()
    : policy::ListPolicyHandler(policy::key::kMultiScreenCaptureAllowedForUrls,
                                /*list_entry_type=*/base::Value::Type::STRING) {

}

MultiScreenCapturePolicyHandler::~MultiScreenCapturePolicyHandler() = default;

bool MultiScreenCapturePolicyHandler::CheckListEntry(const base::Value& value) {
  return web_app::IsolatedWebAppUrlInfo::Create(GURL(value.GetString()))
      .has_value();
}

void MultiScreenCapturePolicyHandler::ApplyList(base::Value::List filtered_list,
                                                PrefValueMap* prefs) {
  prefs->SetValue(capture_policy::kManagedMultiScreenCaptureAllowedForUrls,
                  base::Value(std::move(filtered_list)));
}

}  // namespace policy
