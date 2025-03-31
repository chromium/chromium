// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_sandbox/notice/notice_service.h"

#include "chrome/browser/privacy_sandbox/notice/notice_features.h"
#include "chrome/browser/privacy_sandbox/notice/notice_model.h"
#include "components/prefs/pref_service.h"
#include "components/privacy_sandbox/privacy_sandbox_notice_storage.h"
#include "components/privacy_sandbox/privacy_sandbox_prefs.h"

namespace privacy_sandbox {
namespace {

using privacy_sandbox::notice::mojom::PrivacySandboxNotice;

// Defines all existing notices and populates the notice catalog.
void PopulateNoticeCatalog(std::unique_ptr<NoticeCatalog>& catalog) {
  // Define APIs.
  NoticeApi* topics = catalog->RegisterAndRetrieveNewApi();
  NoticeApi* fledge = catalog->RegisterAndRetrieveNewApi();
  NoticeApi* measurement = catalog->RegisterAndRetrieveNewApi();

  // Define Notices.
  catalog->RegisterNoticeGroup<privacy_sandbox::Consent>(
      {{{PrivacySandboxNotice::kTopicsConsentNotice,
         SurfaceType::kDesktopNewTab},
        &privacy_sandbox::kTopicsConsentDesktopModalFeature},
       {{PrivacySandboxNotice::kTopicsConsentNotice, SurfaceType::kClankBrApp},
        &privacy_sandbox::kTopicsConsentModalClankBrAppFeature},
       {{PrivacySandboxNotice::kTopicsConsentNotice,
         SurfaceType::kClankCustomTab},
        &privacy_sandbox::kTopicsConsentModalClankCCTFeature}},
      {topics});

  catalog->RegisterNoticeGroup<Notice>(
      {{{PrivacySandboxNotice::kThreeAdsApisNotice,
         SurfaceType::kDesktopNewTab},
        &privacy_sandbox::kThreeAdsAPIsNoticeModalFeature},
       {{PrivacySandboxNotice::kThreeAdsApisNotice, SurfaceType::kClankBrApp},
        &privacy_sandbox::kThreeAdsAPIsNoticeModalClankBrAppFeature},
       {{PrivacySandboxNotice::kThreeAdsApisNotice,
         SurfaceType::kClankCustomTab},
        &privacy_sandbox::kThreeAdsAPIsNoticeModalClankCCTFeature}},
      {topics, fledge, measurement});

  catalog->RegisterNoticeGroup<Notice>(
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

  catalog->RegisterNoticeGroup<Notice>(
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

PrivacySandboxNoticeService::PrivacySandboxNoticeService(Profile* profile)
    : profile_(profile) {
  notice_storage_ = std::make_unique<PrivacySandboxNoticeStorage>();
  catalog_ = std::make_unique<NoticeCatalog>();
  CHECK(profile_);
  CHECK(notice_storage_);
  CHECK(catalog_);

  PopulateNoticeCatalog(catalog_);
}

PrivacySandboxNoticeService::~PrivacySandboxNoticeService() = default;

void PrivacySandboxNoticeService::Shutdown() {
  profile_ = nullptr;
  notice_storage_ = nullptr;
  catalog_ = nullptr;
}

void PrivacySandboxNoticeService::EventOccurred(NoticeId notice_id,
                                                NoticeEvent event) {
  // Crash if notice_id could not be found.
  auto it = catalog_->GetNoticeMap().find(notice_id);
  CHECK(it != catalog_->GetNoticeMap().end())
      << "EventOccurred on unregistered notice id for noticeId "
      << notice_id.first << " and surfaceType " << notice_id.second;

  Notice* notice = it->second.get();
  std::string_view name = notice->GetFeature()->name;

  // TODO(crbug.com/392612108): Consolidate to single function call after
  // consolidate these two methods on notice storage side.
  if (event == NoticeEvent::kShown) {
    GetNoticeStorage()->SetNoticeShown(GetPrefService(), name,
                                       base::Time::Now());
  } else {
    GetNoticeStorage()->SetNoticeActionTaken(GetPrefService(), name, event,
                                             base::Time::Now());
  }
}

// TODO(crbug.com/392612108): Implement this function.
std::vector<PrivacySandboxNotice> GetRequiredNotices(SurfaceType surface) {
  std::vector<PrivacySandboxNotice> required_notices;
  return required_notices;
}

PrivacySandboxNoticeStorage* PrivacySandboxNoticeService::GetNoticeStorage() {
  return notice_storage_.get();
}

PrefService* PrivacySandboxNoticeService::GetPrefService() {
  return profile_->GetPrefs();
}

NoticeCatalog* PrivacySandboxNoticeService::GetCatalog() {
  return catalog_.get();
}

}  // namespace privacy_sandbox
