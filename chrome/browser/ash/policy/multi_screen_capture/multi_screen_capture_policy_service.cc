// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/multi_screen_capture/multi_screen_capture_policy_service.h"

#include "base/memory/ptr_util.h"
#include "chrome/browser/media/webrtc/capture_policy_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_service.h"

namespace policy {

MultiScreenCapturePolicyService::MultiScreenCapturePolicyService(
    Profile* profile)
    : profile_(profile) {}

MultiScreenCapturePolicyService::~MultiScreenCapturePolicyService() = default;

std::unique_ptr<MultiScreenCapturePolicyService>
MultiScreenCapturePolicyService::Create(Profile* profile) {
  auto service = base::WrapUnique(new MultiScreenCapturePolicyService(profile));
  service->Init();
  return service;
}

void MultiScreenCapturePolicyService::Init() {
  // Fetch the initial value of the multi screen capture allowlist for later
  // matching to prevent dynamic refresh.
  multi_screen_capture_allow_list_on_login_ =
      profile_->GetPrefs()
          ->GetList(capture_policy::kManagedMultiScreenCaptureAllowedForUrls)
          .Clone();
}

bool MultiScreenCapturePolicyService::IsMultiScreenCaptureAllowed(
    const GURL& url) const {
  CHECK(BUILDFLAG(IS_CHROMEOS_ASH));
  for (auto const& value : multi_screen_capture_allow_list_on_login_) {
    ContentSettingsPattern pattern =
        ContentSettingsPattern::FromString(value.GetString());
    if (!pattern.IsValid()) {
      continue;
    }

    // Despite |url| being a GURL, the path is ignored when matching.
    if (pattern.Matches(url)) {
      return true;
    }
  }
  return false;
}

size_t MultiScreenCapturePolicyService::GetAllowListSize() const {
  return multi_screen_capture_allow_list_on_login_.size();
}

void MultiScreenCapturePolicyService::Shutdown() {
  profile_ = nullptr;
}

}  // namespace policy
