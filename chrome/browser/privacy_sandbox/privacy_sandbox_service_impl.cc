// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_sandbox/privacy_sandbox_service_impl.h"

#include "base/check.h"
#include "components/prefs/pref_service.h"
#include "components/privacy_sandbox/privacy_sandbox_prefs.h"

PrivacySandboxServiceImpl::PrivacySandboxServiceImpl(PrefService* pref_service)
    : pref_service_(pref_service) {
  CHECK(pref_service_, base::NotFatalUntil::M162);

  // Clears the Topics, Fledge, and Measurement Privacy Sandbox API Prefs, if
  // the PrivacySandboxAdPrivacyUxDeprecation feature is enabled.
  privacy_sandbox::MaybeClearAdPrivacyPrefs(pref_service_);
}

PrivacySandboxServiceImpl::~PrivacySandboxServiceImpl() = default;

void PrivacySandboxServiceImpl::Shutdown() {
  pref_service_ = nullptr;
}


