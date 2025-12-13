// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/startup_passwords_import_service_factory.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "build/buildflag.h"
#include "chrome/browser/affiliations/affiliation_service_factory.h"
#include "chrome/browser/password_manager/account_password_store_factory.h"
#include "chrome/browser/password_manager/profile_password_store_factory.h"
#include "chrome/browser/password_manager/startup_passwords_import_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_switches.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/version_info/channel.h"
#include "content/public/browser/browser_context.h"

namespace {

bool IsDevOrCanaryOrUnofficialChannel() {
  version_info::Channel channel = chrome::GetChannel();
  bool is_official_build = false;
#if defined(OFFICIAL_BUILD)
  is_official_build = true;
#endif

  return channel == version_info::Channel::UNKNOWN ||
         channel == version_info::Channel::CANARY ||
         channel == version_info::Channel::DEV || !is_official_build;
}

}  // namespace

StartupPasswordsImportService*
StartupPasswordsImportServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<StartupPasswordsImportService*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/false));
}

StartupPasswordsImportServiceFactory*
StartupPasswordsImportServiceFactory::GetInstance() {
  static base::NoDestructor<StartupPasswordsImportServiceFactory> instance;
  return instance.get();
}

StartupPasswordsImportServiceFactory::StartupPasswordsImportServiceFactory()
    : ProfileKeyedServiceFactory("StartupPasswordsImportService") {
  DependsOn(ProfilePasswordStoreFactory::GetInstance());
  DependsOn(AccountPasswordStoreFactory::GetInstance());
  DependsOn(AffiliationServiceFactory::GetInstance());
}

StartupPasswordsImportServiceFactory::~StartupPasswordsImportServiceFactory() =
    default;

bool StartupPasswordsImportServiceFactory::ServiceIsCreatedWithBrowserContext()
    const {
  // Eagerly create the service *only if* the command line switch is present
  // AND the channel is Dev or Canary.
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
             switches::kImportPasswords) &&
         IsDevOrCanaryOrUnofficialChannel();
}

std::unique_ptr<KeyedService>
StartupPasswordsImportServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  CHECK(base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kImportPasswords));

  if (!IsDevOrCanaryOrUnofficialChannel()) {
    LOG(WARNING) << "StartupPasswordsImportService is only available on Dev "
                    "and Canary channels.";
    return nullptr;
  }

  VLOG(1) << "kImportPasswords switch present, creating service.";
  Profile* profile = Profile::FromBrowserContext(context);
  return std::make_unique<StartupPasswordsImportService>(profile);
}
