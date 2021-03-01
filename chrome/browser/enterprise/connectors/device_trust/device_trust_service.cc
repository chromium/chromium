// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/device_trust_service.h"

#include "base/base64.h"
#include "base/logging.h"
#include "base/values.h"
#include "chrome/browser/enterprise/connectors/connectors_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"

namespace policy {

DeviceTrustService::DeviceTrustService() = default;

DeviceTrustService::DeviceTrustService(Profile* profile)
    : profile_(profile), prefs_(profile_->GetPrefs()) {
  if (prefs_->FindPreference(
          enterprise_connectors::kContextAwareAccessSignalsAllowlistPref) &&
      prefs_->HasPrefPath(
          enterprise_connectors::kContextAwareAccessSignalsAllowlistPref))
    key_pair_ = std::make_unique<DeviceTrustKeyPair>(profile_);
}

DeviceTrustService::~DeviceTrustService() = default;

}  // namespace policy
