// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/critical_actions/critical_action_factory.h"

#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_selections.h"
#include "components/critical_actions/core/browser/critical_action_service.h"
#include "components/critical_actions/core/browser/features.h"
#include "content/public/browser/browser_context.h"

namespace critical_actions {

namespace {

constexpr base::FilePath::CharType kCriticalActionsDatabaseFilename[] =
    FILE_PATH_LITERAL("CriticalActions.db");
}  // namespace

// static
CriticalActionFactory* CriticalActionFactory::GetInstance() {
  static base::NoDestructor<CriticalActionFactory> instance;
  return instance.get();
}

// static
CriticalActionService* CriticalActionFactory::GetForProfile(Profile* profile) {
  return static_cast<CriticalActionService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

CriticalActionFactory::CriticalActionFactory()
    : ProfileKeyedServiceFactory(
          "CriticalActionService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              .WithGuest(ProfileSelection::kNone)
              .WithSystem(ProfileSelection::kNone)
              .Build()) {
  DependsOn(HistoryServiceFactory::GetInstance());
}

CriticalActionFactory::~CriticalActionFactory() = default;

std::unique_ptr<KeyedService>
CriticalActionFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  if (!base::FeatureList::IsEnabled(features::kCriticalActionHistory)) {
    return nullptr;
  }

  Profile* profile = Profile::FromBrowserContext(context);
  base::FilePath db_path =
      profile->GetPath().Append(kCriticalActionsDatabaseFilename);

  // Create a sequenced background runner for SQLite database disk I/O
  // operations that block shutdown to guarantee database consistency.
  auto backend_task_runner = base::ThreadPool::CreateSequencedTaskRunner(
      {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::BLOCK_SHUTDOWN});

  history::HistoryService* history_service =
      HistoryServiceFactory::GetForProfile(profile,
                                           ServiceAccessType::EXPLICIT_ACCESS);

  return std::make_unique<CriticalActionService>(db_path, backend_task_runner,
                                                 history_service);
}

}  // namespace critical_actions
