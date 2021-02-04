// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/device_trust_service.h"
#include "chrome/browser/profiles/profile.h"

namespace policy {

DeviceTrustService::DeviceTrustService() = default;

DeviceTrustService::DeviceTrustService(Profile* profile)
    : profile_(profile), prefs_(profile_->GetPrefs()) {}

DeviceTrustService::~DeviceTrustService() = default;

}  // namespace policy
