// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility_annotator/accessibility_annotator_backend_factory.h"

#include "base/files/file_path.h"
#include "base/functional/callback_helpers.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/data_type_store_service_factory.h"
#include "components/accessibility_annotator/core/accessibility_annotator_features.h"
#include "components/accessibility_annotator/core/storage/accessibility_annotator_backend_impl.h"
#include "components/history/core/browser/history_service.h"
#include "components/sync/model/data_type_store_service.h"
#include "sql/database.h"

constexpr base::FilePath::CharType kAccessibilityAnnotatorDatabaseFileName[] =
    FILE_PATH_LITERAL("AccessibilityAnnotatorDB");

// static
accessibility_annotator::AccessibilityAnnotatorBackend*
AccessibilityAnnotatorBackendFactory::GetForProfile(Profile* profile) {
  return static_cast<accessibility_annotator::AccessibilityAnnotatorBackend*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
AccessibilityAnnotatorBackendFactory*
AccessibilityAnnotatorBackendFactory::GetInstance() {
  static base::NoDestructor<AccessibilityAnnotatorBackendFactory> instance;
  return instance.get();
}

AccessibilityAnnotatorBackendFactory::AccessibilityAnnotatorBackendFactory()
    : ProfileKeyedServiceFactory(
          "AccessibilityAnnotatorBackend",
          ProfileSelections::BuildRedirectedInIncognito()) {
  DependsOn(DataTypeStoreServiceFactory::GetInstance());
  DependsOn(HistoryServiceFactory::GetInstance());
}

AccessibilityAnnotatorBackendFactory::~AccessibilityAnnotatorBackendFactory() =
    default;

std::unique_ptr<KeyedService>
AccessibilityAnnotatorBackendFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  base::FilePath db_path =
      profile->GetPath().Append(kAccessibilityAnnotatorDatabaseFileName);

  // The backend is shared between the content annotator and the accessibility
  // annotator services. Disable if BOTH features are disabled.
  if (!base::FeatureList::IsEnabled(
          accessibility_annotator::features::kContentAnnotator) &&
      !base::FeatureList::IsEnabled(
          accessibility_annotator::features::kAccessibilityAnnotator)) {
    // If the Accessibility Annotator and Content Annotator features are both
    // disabled, attempt to delete the database.
    base::ThreadPool::PostTaskAndReply(
        FROM_HERE,
        {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
         base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
        base::BindOnce(
            [](const base::FilePath& path) { sql::Database::Delete(path); },
            db_path),
        base::DoNothing());
    return nullptr;
  }

  return std::make_unique<
      accessibility_annotator::AccessibilityAnnotatorBackendImpl>(
      HistoryServiceFactory::GetForProfile(profile,
                                           ServiceAccessType::EXPLICIT_ACCESS),
      g_browser_process->os_crypt_async(),
      DataTypeStoreServiceFactory::GetForProfile(profile)->GetStoreFactory(),
      db_path);
}
