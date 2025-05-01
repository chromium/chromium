// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_sandbox/notice/notice_service.h"

#include "chrome/browser/privacy_sandbox/notice/notice_catalog.h"
#include "chrome/browser/privacy_sandbox/notice/notice_model.h"
#include "chrome/browser/privacy_sandbox/notice/notice_storage.h"
#include "chrome/browser/profiles/profile.h"

namespace privacy_sandbox {

using ::privacy_sandbox::notice::mojom::PrivacySandboxNotice;
using ::privacy_sandbox::notice::mojom::PrivacySandboxNoticeEvent;

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

  // Refresh fulfillment status for all notices at service initialization.
  for (auto& [_, notice] : catalog_->GetNoticeMap()) {
    CHECK(notice);
    notice->RefreshFulfillmentStatus(*notice_storage_);
  }

  EmitStartupHistograms();
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
  notice_storage()->RecordEvent(notice_id, event);

  auto notice_ptr = catalog_->GetNoticeMap().find(notice_id);
  CHECK(notice_ptr != catalog_->GetNoticeMap().end());
  CHECK(notice_ptr->second != nullptr);

  // Refresh fulfillment status after an event has occurred.
  notice_ptr->second->RefreshFulfillmentStatus(*notice_storage());
  notice_ptr->second->UpdateTargetApiResults(event);
}

// TODO(crbug.com/392612108): Implement this function.
std::vector<PrivacySandboxNotice>
PrivacySandboxNoticeService::GetRequiredNotices(SurfaceType surface) {
  std::vector<PrivacySandboxNotice> required_notices;
  return required_notices;
}

void PrivacySandboxNoticeService::EmitStartupHistograms() {
  notice_storage()->RecordStartupHistograms();
}

#if !BUILDFLAG(IS_ANDROID)
DesktopViewManagerInterface*
PrivacySandboxNoticeService::GetDesktopViewManager() {
  return desktop_view_manager_.get();
}
#endif  // !BUILDFLAG(IS_ANDROID)

}  // namespace privacy_sandbox
