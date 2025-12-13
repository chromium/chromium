// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_sandbox/notice/notice_service_factory.h"

#include <memory>

#include "base/no_destructor.h"
#include "chrome/browser/privacy_sandbox/notice/notice_catalog.h"
#include "chrome/browser/privacy_sandbox/notice/notice_service.h"
#include "chrome/browser/privacy_sandbox/notice/notice_storage.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_service_factory.h"
#include "chrome/browser/profiles/profile.h"

PrivacySandboxNoticeServiceFactory*
PrivacySandboxNoticeServiceFactory::GetInstance() {
  static base::NoDestructor<PrivacySandboxNoticeServiceFactory> instance;
  return instance.get();
}

privacy_sandbox::PrivacySandboxNoticeServiceInterface*
PrivacySandboxNoticeServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<privacy_sandbox::PrivacySandboxNoticeServiceInterface*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

PrivacySandboxNoticeServiceFactory::PrivacySandboxNoticeServiceFactory()
    : ProfileKeyedServiceFactory("PrivacySandboxNoticeService") {
  DependsOn(PrivacySandboxServiceFactory::GetInstance());
}

std::unique_ptr<KeyedService>
PrivacySandboxNoticeServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return std::make_unique<privacy_sandbox::PrivacySandboxNoticeService>(
      profile, std::make_unique<privacy_sandbox::NoticeCatalogImpl>(profile),
      std::make_unique<privacy_sandbox::PrivacySandboxNoticeStorage>(
          profile->GetPrefs()));
}
