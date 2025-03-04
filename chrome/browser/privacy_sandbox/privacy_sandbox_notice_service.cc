// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_sandbox/privacy_sandbox_notice_service.h"

#include "base/json/values_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "components/prefs/pref_service.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "components/privacy_sandbox/privacy_sandbox_notice_constants.h"
#include "components/privacy_sandbox/privacy_sandbox_notice_storage.h"
#include "components/privacy_sandbox/privacy_sandbox_prefs.h"

namespace privacy_sandbox {

PrivacySandboxNoticeService::PrivacySandboxNoticeService(
    PrefService* pref_service)
    : pref_service_(pref_service) {
  notice_storage_ = std::make_unique<PrivacySandboxNoticeStorage>();
  CHECK(pref_service_);
  CHECK(notice_storage_);
}

PrivacySandboxNoticeService::~PrivacySandboxNoticeService() = default;

void PrivacySandboxNoticeService::Shutdown() {
  pref_service_ = nullptr;
  notice_storage_ = nullptr;
}

PrivacySandboxNoticeStorage* PrivacySandboxNoticeService::GetNoticeStorage() {
  return notice_storage_.get();
}

}  // namespace privacy_sandbox
