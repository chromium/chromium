// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/k_anonymity_service/k_anonymity_service_factory.h"

#include <memory>
#include <string>

#include "base/files/file_util.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_selections.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/k_anonymity_service_delegate.h"
#include "content/public/browser/storage_partition.h"

namespace {
const char kKAnonymityServiceStoragePath[] = "KAnonymityService";

ProfileSelections BuildKAnonymityServiceProfileSelections() {
  return ProfileSelections::Builder()
      .WithRegular(ProfileSelection::kOwnInstance)
      .WithGuest(ProfileSelection::kOwnInstance)
      // TODO(crbug.com/41488885): Check if this service is needed for
      // Ash Internals.
      .WithAshInternals(ProfileSelection::kOwnInstance)
      .Build();
}

}  // namespace

// static
KAnonymityServiceFactory* KAnonymityServiceFactory::GetInstance() {
  static base::NoDestructor<KAnonymityServiceFactory> instance;
  return instance.get();
}

// static
content::KAnonymityServiceDelegate* KAnonymityServiceFactory::GetForProfile(
    Profile* profile) {
  // Delete the old database file if it exists.
  // TODO - crbug.com/477278281: Remove this when it is no longer needed.
  base::FilePath path =
      profile->GetDefaultStoragePartition()->GetPath().AppendASCII(
          kKAnonymityServiceStoragePath);
  base::ThreadPool::PostTask(
      FROM_HERE, {base::TaskPriority::BEST_EFFORT, base::MayBlock()},
      base::GetDeleteFileCallback(path));
  return nullptr;
}

KAnonymityServiceFactory::KAnonymityServiceFactory()
    : ProfileKeyedServiceFactory("KAnonymityServiceFactory",
                                 BuildKAnonymityServiceProfileSelections()) {
  DependsOn(IdentityManagerFactory::GetInstance());
}

KAnonymityServiceFactory::~KAnonymityServiceFactory() = default;

// BrowserContextKeyedServiceFactory:
std::unique_ptr<KeyedService>
KAnonymityServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  // Delete the old database file if it exists.
  // TODO - crbug.com/477278281: Remove this when it is no longer needed.
  base::FilePath path =
      context->GetDefaultStoragePartition()->GetPath().AppendASCII(
          kKAnonymityServiceStoragePath);
  base::ThreadPool::PostTask(
      FROM_HERE, {base::TaskPriority::BEST_EFFORT, base::MayBlock()},
      base::GetDeleteFileCallback(path));
  return nullptr;
}
