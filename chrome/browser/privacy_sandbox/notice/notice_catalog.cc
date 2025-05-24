// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_sandbox/notice/notice_catalog.h"

#include <utility>

#include "base/feature_list.h"
#include "chrome/browser/privacy_sandbox/notice/notice_features.h"
#include "chrome/browser/privacy_sandbox/notice/notice_model.h"

namespace privacy_sandbox {

namespace {

using enum privacy_sandbox::notice::mojom::PrivacySandboxNotice;
using enum privacy_sandbox::SurfaceType;

template <typename T>
std::unique_ptr<Notice> Make(NoticeId id) {
  return std::make_unique<T>(id);
}

}  // namespace

NoticeCatalogImpl::NoticeCatalogImpl() {
  Populate();
}
NoticeCatalogImpl::~NoticeCatalogImpl() = default;

NoticeApi* NoticeCatalogImpl::RegisterAndRetrieveNewApi() {
  NoticeApi* api = apis_.emplace_back(std::make_unique<NoticeApi>()).get();
  apis_ptrs_.push_back(api);
  return api;
}

base::span<NoticeApi*> NoticeCatalogImpl::GetNoticeApis() {
  return apis_ptrs_;
}

Notice* NoticeCatalogImpl::RegisterAndRetrieveNewNotice(
    std::unique_ptr<Notice> (*notice_creator)(NoticeId),
    NoticeId notice_id) {
  Notice* notice = notices_.emplace(notice_id, notice_creator(notice_id))
                       .first->second.get();
  notice_ptrs_.push_back(notice);
  return notice;
}

void NoticeCatalogImpl::RegisterNoticeGroup(
    std::unique_ptr<Notice> (*notice_creator)(NoticeId),
    std::vector<std::pair<NoticeId, const base::Feature*>>&& notice_ids,
    std::vector<NoticeApi*>&& target_apis,
    std::vector<NoticeApi*>&& pre_req_apis,
    std::pair<NoticeViewGroup, int> view_group) {
  const std::vector<NoticeApi*>& pre_req_apis1 = pre_req_apis;
  for (auto [notice_id, feature] : notice_ids) {
    RegisterAndRetrieveNewNotice(notice_creator, notice_id)
        ->SetFeature(feature)
        ->SetTargetApis(target_apis)
        ->SetPreReqApis(pre_req_apis1)
        ->SetViewGroup(view_group);
  }
}

base::span<Notice*> NoticeCatalogImpl::GetNotices() {
  return notice_ptrs_;
}

Notice* NoticeCatalogImpl::GetNotice(NoticeId notice_id) {
  auto notice_ptr = notices_.find(notice_id);
  return notice_ptr != notices_.end() ? notice_ptr->second.get() : nullptr;
}

void NoticeCatalogImpl::Populate() {
  // TODO(crbug.com/392612108): Add all eligibility and result callbacks.

  // Define APIs.
  NoticeApi* topics = RegisterAndRetrieveNewApi();
  NoticeApi* protected_audience = RegisterAndRetrieveNewApi();
  NoticeApi* measurement = RegisterAndRetrieveNewApi();

  // Define Notices.
  // Topics EEA Consent.
  RegisterNoticeGroup(&Make<Consent>,
                      {{{kTopicsConsentNotice, kDesktopNewTab},
                        &kTopicsConsentDesktopModalFeature},
                       {{kTopicsConsentNotice, kClankBrApp},
                        &kTopicsConsentModalClankBrAppFeature},
                       {{kTopicsConsentNotice, kClankCustomTab},
                        &kTopicsConsentModalClankCCTFeature}},
                      {topics}, {}, {NoticeViewGroup::kAdsNoticeEeaGroup, 1});

  // Protected Audience Measurement EEA Notice
  RegisterNoticeGroup(
      &Make<Notice>,
      {{{kProtectedAudienceMeasurementNotice, kDesktopNewTab},
        &kProtectedAudienceMeasurementNoticeModalFeature},
       {{kProtectedAudienceMeasurementNotice, kClankBrApp},
        &kProtectedAudienceMeasurementNoticeModalClankBrAppFeature},
       {{kProtectedAudienceMeasurementNotice, kClankCustomTab},
        &kProtectedAudienceMeasurementNoticeModalClankCCTFeature}},
      {protected_audience, measurement}, {},
      {NoticeViewGroup::kAdsNoticeEeaGroup, 2});

  // ROW Ads APIs Notice
  RegisterNoticeGroup(&Make<Notice>,
                      {{{kThreeAdsApisNotice, kDesktopNewTab},
                        &kThreeAdsAPIsNoticeModalFeature},
                       {{kThreeAdsApisNotice, kClankBrApp},
                        &kThreeAdsAPIsNoticeModalClankBrAppFeature},
                       {{kThreeAdsApisNotice, kClankCustomTab},
                        &kThreeAdsAPIsNoticeModalClankCCTFeature}},
                      {topics, protected_audience, measurement});

  // Restricted Measurement Notice.
  RegisterNoticeGroup(
      &Make<Notice>,
      {{{kMeasurementNotice, kDesktopNewTab}, &kMeasurementNoticeModalFeature},
       {{kMeasurementNotice, kClankBrApp},
        &kMeasurementNoticeModalClankBrAppFeature},
       {{kMeasurementNotice, kClankCustomTab},
        &kMeasurementNoticeModalClankCCTFeature}},
      {measurement});
}

}  // namespace privacy_sandbox
