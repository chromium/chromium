// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_sandbox/notice/notice_framework.h"

#include "chrome/browser/privacy_sandbox/notice/notice_model.h"
#include "components/prefs/pref_service.h"
#include "components/privacy_sandbox/privacy_sandbox_notice_storage.h"
#include "components/privacy_sandbox/privacy_sandbox_prefs.h"

using privacy_sandbox::notice::mojom::PrivacySandboxNotice;
namespace privacy_sandbox {
namespace {

// Defines all existing notices and populates the notice catalog.
void PopulateNoticeCatalog(std::unique_ptr<NoticeCatalog>& catalog) {
  // Define APIs.
  NoticeApi* topics = catalog->RegisterAndRetrieveNewApi();
  NoticeApi* fledge = catalog->RegisterAndRetrieveNewApi();
  NoticeApi* measurement = catalog->RegisterAndRetrieveNewApi();

  // Define Notices.
  catalog->RegisterNoticeGroup<privacy_sandbox::Consent>(
      {{PrivacySandboxNotice::kTopicsConsentNotice,
        SurfaceType::kDesktopNewTab},
       {PrivacySandboxNotice::kTopicsConsentNotice, SurfaceType::kClankBrApp},
       {PrivacySandboxNotice::kTopicsConsentNotice,
        SurfaceType::kClankCustomTab}},
      {topics});

  catalog->RegisterNoticeGroup<Notice>(
      {{PrivacySandboxNotice::kThreeAdsApisNotice, SurfaceType::kDesktopNewTab},
       {PrivacySandboxNotice::kThreeAdsApisNotice, SurfaceType::kClankBrApp},
       {PrivacySandboxNotice::kThreeAdsApisNotice,
        SurfaceType::kClankCustomTab}},
      {topics, fledge, measurement});

  catalog->RegisterNoticeGroup<Notice>(
      {{PrivacySandboxNotice::kProtectedAudienceMeasurementNotice,
        SurfaceType::kDesktopNewTab},
       {PrivacySandboxNotice::kProtectedAudienceMeasurementNotice,
        SurfaceType::kClankBrApp},
       {PrivacySandboxNotice::kProtectedAudienceMeasurementNotice,
        SurfaceType::kClankCustomTab}},
      {fledge, measurement});

  catalog->RegisterNoticeGroup<Notice>(
      {{PrivacySandboxNotice::kMeasurementNotice, SurfaceType::kDesktopNewTab},
       {PrivacySandboxNotice::kMeasurementNotice, SurfaceType::kClankBrApp},
       {PrivacySandboxNotice::kMeasurementNotice,
        SurfaceType::kClankCustomTab}},
      {measurement});

  // TODO(crbug.com/392612108): Add other notices like re-notice, or v2 notices.
}
}  // namespace

PrivacySandboxNoticeFramework::PrivacySandboxNoticeFramework(Profile* profile)
    : profile_(profile) {
  notice_storage_ = std::make_unique<PrivacySandboxNoticeStorage>();
  catalog_ = std::make_unique<NoticeCatalog>();
  CHECK(profile_);
  CHECK(notice_storage_);
  CHECK(catalog_);

  PopulateNoticeCatalog(catalog_);
}

PrivacySandboxNoticeFramework::~PrivacySandboxNoticeFramework() = default;

void PrivacySandboxNoticeFramework::Shutdown() {
  profile_ = nullptr;
  notice_storage_ = nullptr;
  catalog_ = nullptr;
}

// TODO(crbug.com/392612108): Implement this function.
void PrivacySandboxNoticeFramework::EventOccurred(NoticeId notice,
                                                  NoticeEvent event) {}

// TODO(crbug.com/392612108): Implement this function.
std::vector<PrivacySandboxNotice> GetRequiredNotices(SurfaceType surface) {
  std::vector<PrivacySandboxNotice> required_notices;
  return required_notices;
}

PrivacySandboxNoticeStorage* PrivacySandboxNoticeFramework::GetNoticeStorage() {
  return notice_storage_.get();
}

PrefService* PrivacySandboxNoticeFramework::GetPrefService() {
  return profile_->GetPrefs();
}

}  // namespace privacy_sandbox
