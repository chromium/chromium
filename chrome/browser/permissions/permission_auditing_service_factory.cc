// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/permission_auditing_service_factory.h"

#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/no_destructor.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/after_startup_task_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_features.h"
#include "components/permissions/permission_auditing_service.h"

PermissionAuditingServiceFactory::PermissionAuditingServiceFactory()
    : ProfileKeyedServiceFactory(
          "PermissionAuditingService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOriginalOnly)
              .Build()) {}

PermissionAuditingServiceFactory::~PermissionAuditingServiceFactory() = default;

// static
PermissionAuditingServiceFactory*
PermissionAuditingServiceFactory::GetInstance() {
  static base::NoDestructor<PermissionAuditingServiceFactory> instance;
  return instance.get();
}

// static
permissions::PermissionAuditingService*
PermissionAuditingServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<permissions::PermissionAuditingService*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/true));
}

bool PermissionAuditingServiceFactory::ServiceIsCreatedWithBrowserContext()
    const {
  return true;
}

std::unique_ptr<KeyedService>
PermissionAuditingServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  if (!base::FeatureList::IsEnabled(features::kPermissionAuditing)) {
    return nullptr;
  }
  auto backend_task_runner = base::ThreadPool::CreateSequencedTaskRunner(
      {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::BLOCK_SHUTDOWN});
  std::unique_ptr<permissions::PermissionAuditingService> instance =
      std::make_unique<permissions::PermissionAuditingService>(
          backend_task_runner);
  base::FilePath database_path =
      context->GetPath().Append(FILE_PATH_LITERAL("Permission Auditing Logs"));
  instance->Init(database_path);
  AfterStartupTaskUtils::PostTask(
      FROM_HERE, base::SequencedTaskRunner::GetCurrentDefault(),
      base::BindOnce(&permissions::PermissionAuditingService::
                         StartPeriodicCullingOfExpiredSessions,
                     instance->AsWeakPtr()));
  return instance;
}
