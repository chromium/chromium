// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/credentials_cleaner_runner_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile.h"
#include "components/password_manager/core/browser/credentials_cleaner_runner.h"
#include "content/public/browser/browser_context.h"

CredentialsCleanerRunnerFactory::CredentialsCleanerRunnerFactory()
    : ProfileKeyedServiceFactory(
          "CredentialsCleanerRunner",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOriginalOnly)
              .Build()) {}

CredentialsCleanerRunnerFactory::~CredentialsCleanerRunnerFactory() = default;

CredentialsCleanerRunnerFactory*
CredentialsCleanerRunnerFactory::GetInstance() {
  static base::NoDestructor<CredentialsCleanerRunnerFactory> instance;
  return instance.get();
}

password_manager::CredentialsCleanerRunner*
CredentialsCleanerRunnerFactory::GetForProfile(Profile* profile) {
  return static_cast<password_manager::CredentialsCleanerRunner*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

std::unique_ptr<KeyedService>
CredentialsCleanerRunnerFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<password_manager::CredentialsCleanerRunner>();
}
