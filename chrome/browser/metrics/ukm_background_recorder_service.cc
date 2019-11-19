// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/ukm_background_recorder_service.h"

#include "base/callback.h"
#include "base/callback_helpers.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "components/history/core/browser/history_service.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "url/origin.h"

namespace ukm {

UkmBackgroundRecorderService::UkmBackgroundRecorderService(Profile* profile)
    : history_service_(HistoryServiceFactory::GetForProfile(
          profile,
          ServiceAccessType::EXPLICIT_ACCESS)) {
  DCHECK(history_service_);
}

UkmBackgroundRecorderService::~UkmBackgroundRecorderService() = default;

void UkmBackgroundRecorderService::Shutdown() {
  task_tracker_.TryCancelAll();
}

void UkmBackgroundRecorderService::GetBackgroundSourceIdIfAllowed(
    const url::Origin& origin,
    GetBackgroundSourceIdCallback callback) {
  auto visit_callback = base::BindOnce(
      &UkmBackgroundRecorderService::DidGetVisibleVisitCount,
      weak_ptr_factory_.GetWeakPtr(), origin, std::move(callback));

  history_service_->GetVisibleVisitCountToHost(
      origin.GetURL(), std::move(visit_callback), &task_tracker_);
}

void UkmBackgroundRecorderService::DidGetVisibleVisitCount(
    const url::Origin& origin,
    GetBackgroundSourceIdCallback callback,
    history::VisibleVisitCountToHostResult result) {
  if (!result.success || !result.count) {
    std::move(callback).Run(base::nullopt);
    return;
  }

  ukm::SourceId source_id = ukm::ConvertToSourceId(
      ukm::AssignNewSourceId(), ukm::SourceIdType::HISTORY_ID);
  ukm::UkmRecorder* recorder = ukm::UkmRecorder::Get();
  DCHECK(recorder);
  recorder->UpdateSourceURL(source_id, origin.GetURL());

  std::move(callback).Run(source_id);
}

// static
UkmBackgroundRecorderFactory* UkmBackgroundRecorderFactory::GetInstance() {
  return base::Singleton<UkmBackgroundRecorderFactory>::get();
}

// static
UkmBackgroundRecorderService* UkmBackgroundRecorderFactory::GetForProfile(
    Profile* profile) {
  return static_cast<UkmBackgroundRecorderService*>(
      GetInstance()->GetServiceForBrowserContext(profile, /* create= */ true));
}

UkmBackgroundRecorderFactory::UkmBackgroundRecorderFactory()
    : BrowserContextKeyedServiceFactory(
          "UkmBackgroundRecorderService",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(HistoryServiceFactory::GetInstance());
}

UkmBackgroundRecorderFactory::~UkmBackgroundRecorderFactory() = default;

KeyedService* UkmBackgroundRecorderFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new UkmBackgroundRecorderService(Profile::FromBrowserContext(context));
}

content::BrowserContext* UkmBackgroundRecorderFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextOwnInstanceInIncognito(context);
}

}  // namespace ukm
