// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_sandbox/privacy_sandbox_notice_service_factory.h"

#include <memory>

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_selections.h"

PrivacySandboxNoticeServiceFactory*
PrivacySandboxNoticeServiceFactory::GetInstance() {
  static base::NoDestructor<PrivacySandboxNoticeServiceFactory> instance;
  return instance.get();
}

privacy_sandbox::PrivacySandboxNoticeService*
PrivacySandboxNoticeServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<privacy_sandbox::PrivacySandboxNoticeService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// The same profile selection rules that apply for the existing
// PrivacySandboxService must also apply to the PrivacySandboxNoticeService to
// ensure accurate pref migration.
// LINT.IfChange(PrivacySandboxNoticeService)
PrivacySandboxNoticeServiceFactory::PrivacySandboxNoticeServiceFactory()
    : ProfileKeyedServiceFactory("PrivacySandboxNoticeService",
                                 ProfileSelections::Builder()
                                     // Excluding Ash Internal profiles such as
                                     // the signin or the lockscreen profile.
                                     .WithAshInternals(ProfileSelection::kNone)
                                     .Build()) {}
// LINT.ThenChange(/chrome/browser/privacy_sandbox/privacy_sandbox_service_factory.cc:PrivacySandboxService)

std::unique_ptr<KeyedService>
PrivacySandboxNoticeServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return std::make_unique<privacy_sandbox::PrivacySandboxNoticeService>(
      profile->GetPrefs());
}
