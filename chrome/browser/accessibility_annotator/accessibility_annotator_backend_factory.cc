// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility_annotator/accessibility_annotator_backend_factory.h"

#include "base/files/file_path.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/data_type_store_service_factory.h"
#include "chrome/common/channel_info.h"
#include "components/accessibility_annotator/core/accessibility_annotator_features.h"
#include "components/accessibility_annotator/core/storage/accessibility_annotator_backend.h"
#include "components/history/core/browser/history_service.h"
#include "components/sync/model/data_type_store_service.h"

constexpr base::FilePath::CharType kAccessibilityAnnotatorDatabaseFileName[] =
    FILE_PATH_LITERAL("AccessibilityAnnotatorDatabase");

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
  // The backend is shared between the content annotator and the accessibility
  // annotator services. Disable if BOTH features are disabled.
  if (!base::FeatureList::IsEnabled(
          accessibility_annotator::kContentAnnotator) &&
      !base::FeatureList::IsEnabled(
          accessibility_annotator::kAccessibilityAnnotator)) {
    return nullptr;
  }

  Profile* profile = Profile::FromBrowserContext(context);
  auto backend =
      std::make_unique<accessibility_annotator::AccessibilityAnnotatorBackend>(
          chrome::GetChannel(),
          HistoryServiceFactory::GetForProfile(
              profile, ServiceAccessType::EXPLICIT_ACCESS),
          DataTypeStoreServiceFactory::GetForProfile(profile)
              ->GetStoreFactory(),
          profile->GetPath().Append(kAccessibilityAnnotatorDatabaseFileName));
  backend->Init();
  return backend;
}
