// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/bluetooth/debug_logs_manager_factory.h"

#include "base/memory/singleton.h"
#include "chrome/browser/ash/bluetooth/debug_logs_manager.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/keyed_service/core/keyed_service.h"

namespace ash {

namespace bluetooth {

namespace {

// Wraps a DebugLogsManager instance in a KeyedService.
class DebugLogsManagerService : public KeyedService {
 public:
  explicit DebugLogsManagerService(Profile* profile)
      : debug_logs_manager_(
            ProfileHelper::Get()->GetUserByProfile(profile)->GetDisplayEmail(),
            profile->GetPrefs()) {}

  DebugLogsManagerService(const DebugLogsManagerService&) = delete;
  DebugLogsManagerService& operator=(const DebugLogsManagerService&) = delete;

  ~DebugLogsManagerService() override = default;

  DebugLogsManager* debug_logs_manager() { return &debug_logs_manager_; }

 private:
  DebugLogsManager debug_logs_manager_;
};

}  // namespace

// static
DebugLogsManager* DebugLogsManagerFactory::GetForProfile(Profile* profile) {
  if (!profile)
    return nullptr;

  DebugLogsManagerService* service = static_cast<DebugLogsManagerService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));

  return service ? service->debug_logs_manager() : nullptr;
}

// static
DebugLogsManagerFactory* DebugLogsManagerFactory::GetInstance() {
  return base::Singleton<DebugLogsManagerFactory>::get();
}

DebugLogsManagerFactory::DebugLogsManagerFactory()
    : ProfileKeyedServiceFactory("DebugLogsManagerFactory") {
  DependsOn(IdentityManagerFactory::GetInstance());
}

DebugLogsManagerFactory::~DebugLogsManagerFactory() = default;

KeyedService* DebugLogsManagerFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);

  // Only primary profiles have an associated logs manager.
  if (!ProfileHelper::Get()->IsPrimaryProfile(profile))
    return nullptr;

  return new DebugLogsManagerService(profile);
}

bool DebugLogsManagerFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

bool DebugLogsManagerFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

}  // namespace bluetooth

}  // namespace ash
