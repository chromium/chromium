// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/ukm_background_recorder_service.h"

#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/history/core/browser/history_service.h"
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
    std::move(callback).Run(std::nullopt);
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
    : ProfileKeyedServiceFactory(
          "UkmBackgroundRecorderService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOwnInstance)
              .Build()) {
  DependsOn(HistoryServiceFactory::GetInstance());
}

UkmBackgroundRecorderFactory::~UkmBackgroundRecorderFactory() = default;

std::unique_ptr<KeyedService>
UkmBackgroundRecorderFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<UkmBackgroundRecorderService>(
      Profile::FromBrowserContext(context));
}

}  // namespace ukm
