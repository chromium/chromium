// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_sandbox/privacy_sandbox_activity_types_service.h"

#include "components/prefs/pref_service.h"

namespace privacy_sandbox {

PrivacySandboxActivityTypesService::PrivacySandboxActivityTypesService(
    PrefService* pref_service)
    : pref_service_(pref_service) {
  CHECK(pref_service_);
}

PrivacySandboxActivityTypesService::~PrivacySandboxActivityTypesService() =
    default;

void PrivacySandboxActivityTypesService::Shutdown() {
  pref_service_ = nullptr;
}

}  // namespace privacy_sandbox
