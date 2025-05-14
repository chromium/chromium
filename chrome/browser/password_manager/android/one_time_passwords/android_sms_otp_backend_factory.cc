// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/one_time_passwords/android_sms_otp_backend_factory.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_selections.h"
#include "content/public/browser/browser_context.h"

AndroidSmsOtpBackendFactory::AndroidSmsOtpBackendFactory()
    : ProfileKeyedServiceFactory(
          "AndroidSmsOtpBackendFactory",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              .WithGuest(ProfileSelection::kOwnInstance)
              .Build()) {}
AndroidSmsOtpBackendFactory::~AndroidSmsOtpBackendFactory() = default;

AndroidSmsOtpBackendFactory* AndroidSmsOtpBackendFactory::GetInstance() {
  static base::NoDestructor<AndroidSmsOtpBackendFactory> instance;
  return instance.get();
}

AndroidSmsOtpBackend* AndroidSmsOtpBackendFactory::GetForProfile(
    Profile* profile) {
  return static_cast<AndroidSmsOtpBackend*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/true));
}

std::unique_ptr<KeyedService>
AndroidSmsOtpBackendFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<AndroidSmsOtpBackend>();
}
