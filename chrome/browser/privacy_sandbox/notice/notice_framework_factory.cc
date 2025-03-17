// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_sandbox/notice/notice_framework_factory.h"

#include <memory>

#include "base/no_destructor.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_service_factory.h"
#include "chrome/browser/profiles/profile.h"

PrivacySandboxNoticeFrameworkFactory*
PrivacySandboxNoticeFrameworkFactory::GetInstance() {
  static base::NoDestructor<PrivacySandboxNoticeFrameworkFactory> instance;
  return instance.get();
}

privacy_sandbox::PrivacySandboxNoticeFramework*
PrivacySandboxNoticeFrameworkFactory::GetForProfile(Profile* profile) {
  return static_cast<privacy_sandbox::PrivacySandboxNoticeFramework*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

PrivacySandboxNoticeFrameworkFactory::PrivacySandboxNoticeFrameworkFactory()
    : ProfileKeyedServiceFactory("PrivacySandboxNoticeFramework") {
  DependsOn(PrivacySandboxServiceFactory::GetInstance());
}

std::unique_ptr<KeyedService>
PrivacySandboxNoticeFrameworkFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return std::make_unique<privacy_sandbox::PrivacySandboxNoticeFramework>(
      profile);
}
