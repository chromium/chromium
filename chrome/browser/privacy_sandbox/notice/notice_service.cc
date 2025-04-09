// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_sandbox/notice/notice_service.h"

#include "chrome/browser/privacy_sandbox/notice/notice_catalog.h"
#include "chrome/browser/privacy_sandbox/notice/notice_features.h"
#include "chrome/browser/privacy_sandbox/notice/notice_model.h"
#include "chrome/browser/privacy_sandbox/notice/notice_storage.h"
#include "chrome/browser/profiles/profile.h"

namespace privacy_sandbox {
namespace {
using notice::mojom::PrivacySandboxNotice;
using notice::mojom::PrivacySandboxNoticeEvent;

using privacy_sandbox::notice::mojom::PrivacySandboxNotice;

template <typename T>
std::unique_ptr<Notice> Make(NoticeId id) {
  return std::make_unique<T>(id);
}

// Defines all existing notices and populates the notice catalog.
void PopulateNoticeCatalog(std::unique_ptr<NoticeCatalog>& catalog) {
  // TODO(crbug.com/392612108): Add all eligibility and result callbacks.
  // Define APIs.
  NoticeApi* topics = catalog->RegisterAndRetrieveNewApi();
  NoticeApi* fledge = catalog->RegisterAndRetrieveNewApi();
  NoticeApi* measurement = catalog->RegisterAndRetrieveNewApi();

  // Define Notices.
  catalog->RegisterNoticeGroup(
      &Make<Consent>,
      {{{PrivacySandboxNotice::kTopicsConsentNotice,
         SurfaceType::kDesktopNewTab},
        &privacy_sandbox::kTopicsConsentDesktopModalFeature},
       {{PrivacySandboxNotice::kTopicsConsentNotice, SurfaceType::kClankBrApp},
        &privacy_sandbox::kTopicsConsentModalClankBrAppFeature},
       {{PrivacySandboxNotice::kTopicsConsentNotice,
         SurfaceType::kClankCustomTab},
        &privacy_sandbox::kTopicsConsentModalClankCCTFeature}},
      {topics});

  catalog->RegisterNoticeGroup(
      &Make<Notice>,
      {{{PrivacySandboxNotice::kThreeAdsApisNotice,
         SurfaceType::kDesktopNewTab},
        &privacy_sandbox::kThreeAdsAPIsNoticeModalFeature},
       {{PrivacySandboxNotice::kThreeAdsApisNotice, SurfaceType::kClankBrApp},
        &privacy_sandbox::kThreeAdsAPIsNoticeModalClankBrAppFeature},
       {{PrivacySandboxNotice::kThreeAdsApisNotice,
         SurfaceType::kClankCustomTab},
        &privacy_sandbox::kThreeAdsAPIsNoticeModalClankCCTFeature}},
      {topics, fledge, measurement});

  catalog->RegisterNoticeGroup(
      &Make<Notice>,
      {{{PrivacySandboxNotice::kProtectedAudienceMeasurementNotice,
         SurfaceType::kDesktopNewTab},
        &privacy_sandbox::kProtectedAudienceMeasurementNoticeModalFeature},
       {{PrivacySandboxNotice::kProtectedAudienceMeasurementNotice,
         SurfaceType::kClankBrApp},
        &privacy_sandbox::
            kProtectedAudienceMeasurementNoticeModalClankBrAppFeature},
       {{PrivacySandboxNotice::kProtectedAudienceMeasurementNotice,
         SurfaceType::kClankCustomTab},
        &privacy_sandbox::
            kProtectedAudienceMeasurementNoticeModalClankCCTFeature}},
      {fledge, measurement});

  catalog->RegisterNoticeGroup(
      &Make<Notice>,
      {{{PrivacySandboxNotice::kMeasurementNotice, SurfaceType::kDesktopNewTab},
        &privacy_sandbox::kMeasurementNoticeModalFeature},
       {{PrivacySandboxNotice::kMeasurementNotice, SurfaceType::kClankBrApp},
        &privacy_sandbox::kMeasurementNoticeModalClankBrAppFeature},
       {{PrivacySandboxNotice::kMeasurementNotice,
         SurfaceType::kClankCustomTab},
        &privacy_sandbox::kMeasurementNoticeModalClankCCTFeature}},
      {measurement});

  // TODO(crbug.com/392612108): Add other notices like re-notice, or v2 notices.
}
}  // namespace

PrivacySandboxNoticeService::PrivacySandboxNoticeService(
    Profile* profile,
    std::unique_ptr<NoticeCatalog> catalog,
    std::unique_ptr<NoticeStorage> storage)
    : profile_(profile),
      catalog_(std::move(catalog)),
      notice_storage_(std::move(storage)) {
  CHECK(profile_);
  CHECK(notice_storage_);
  CHECK(catalog_);

#if !BUILDFLAG(IS_ANDROID)
  desktop_view_manager_ = std::make_unique<DesktopViewManager>(this);
  CHECK(desktop_view_manager_);
#endif  // !BUILDFLAG(IS_ANDROID)

  PopulateNoticeCatalog(catalog_);
}

PrivacySandboxNoticeService::~PrivacySandboxNoticeService() = default;

void PrivacySandboxNoticeService::Shutdown() {
  profile_ = nullptr;
  notice_storage_ = nullptr;
  catalog_ = nullptr;
}

void PrivacySandboxNoticeService::EventOccurred(
    NoticeId notice_id,
    PrivacySandboxNoticeEvent event) {
  // Crash if notice_id could not be found.
  auto it = catalog_->GetNoticeMap().find(notice_id);
  CHECK(it != catalog_->GetNoticeMap().end())
      << "EventOccurred on unregistered notice id for noticeId "
      << notice_id.first << " and surfaceType "
      << static_cast<int>(notice_id.second);

  Notice* notice = it->second.get();

  GetNoticeStorage()->RecordEvent(GetPrefService(), notice->GetStorageName(),
                                  event, base::Time::Now());

  notice->UpdateTargetApiResults(event);
}

// TODO(crbug.com/392612108): Implement this function.
std::vector<PrivacySandboxNotice>
PrivacySandboxNoticeService::GetRequiredNotices(SurfaceType surface) {
  std::vector<PrivacySandboxNotice> required_notices;
  return required_notices;
}

NoticeStorage* PrivacySandboxNoticeService::GetNoticeStorage() {
  return notice_storage_.get();
}

PrefService* PrivacySandboxNoticeService::GetPrefService() {
  return profile_->GetPrefs();
}

NoticeCatalog* PrivacySandboxNoticeService::GetCatalog() {
  return catalog_.get();
}

#if !BUILDFLAG(IS_ANDROID)
DesktopViewManager* PrivacySandboxNoticeService::GetDesktopViewManager() {
  return desktop_view_manager_.get();
}
#endif  // !BUILDFLAG(IS_ANDROID)

}  // namespace privacy_sandbox
