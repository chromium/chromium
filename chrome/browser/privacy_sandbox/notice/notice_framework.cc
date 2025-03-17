// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_sandbox/notice/notice_framework.h"

#include "components/prefs/pref_service.h"
#include "components/privacy_sandbox/privacy_sandbox_notice_storage.h"
#include "components/privacy_sandbox/privacy_sandbox_prefs.h"

namespace privacy_sandbox {

PrivacySandboxNoticeFramework::PrivacySandboxNoticeFramework(Profile* profile)
    : profile_(profile) {
  notice_storage_ = std::make_unique<PrivacySandboxNoticeStorage>();
  CHECK(profile_);
  CHECK(notice_storage_);

  // TODO(crbug.com/392612108): Write a function to define all of the existing
  // notices.
}

PrivacySandboxNoticeFramework::~PrivacySandboxNoticeFramework() = default;

void PrivacySandboxNoticeFramework::Shutdown() {
  profile_ = nullptr;
  notice_storage_ = nullptr;
}

// TODO(crbug.com/392612108): Implement this function.
void PrivacySandboxNoticeFramework::EventOccurred(PrivacySandboxNotice notice,
                                                  NoticeEvent event) {}

// TODO(crbug.com/392612108): Implement this function.
std::vector<PrivacySandboxNotice> GetRequiredNotices() {
  std::vector<PrivacySandboxNotice> required_notices;
  return required_notices;
}

PrivacySandboxNoticeStorage* PrivacySandboxNoticeFramework::GetNoticeStorage() {
  return notice_storage_.get();
}

PrefService* PrivacySandboxNoticeFramework::GetPrefService() {
  return profile_->GetPrefs();
}

}  // namespace privacy_sandbox
