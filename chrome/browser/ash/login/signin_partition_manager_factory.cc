// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/signin_partition_manager_factory.h"

#include <memory>
#include <utility>

#include "base/memory/singleton.h"
#include "chrome/browser/ash/login/signin_partition_manager.h"
#include "chrome/browser/profiles/profile_selections.h"

namespace ash::login {

SigninPartitionManagerFactory::SigninPartitionManagerFactory()
    : ProfileKeyedServiceFactory(
          "SigninPartitionManager",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOwnInstance)
              .Build()) {}

SigninPartitionManagerFactory::~SigninPartitionManagerFactory() = default;

// static
SigninPartitionManager* SigninPartitionManagerFactory::GetForBrowserContext(
    content::BrowserContext* browser_context) {
  return static_cast<SigninPartitionManager*>(
      GetInstance()->GetServiceForBrowserContext(browser_context,
                                                 true /* create */));
}

// static
SigninPartitionManagerFactory* SigninPartitionManagerFactory::GetInstance() {
  return base::Singleton<SigninPartitionManagerFactory>::get();
}

std::unique_ptr<KeyedService>
SigninPartitionManagerFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<SigninPartitionManager>(context);
}

}  // namespace ash::login
