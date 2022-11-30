// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/consent_auditor/consent_auditor_test_utils.h"
#include "components/consent_auditor/fake_consent_auditor.h"

std::unique_ptr<KeyedService> BuildFakeConsentAuditor(
    content::BrowserContext* context) {
  return std::make_unique<consent_auditor::FakeConsentAuditor>();
}
