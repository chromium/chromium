// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRIVACY_SANDBOX_NOTICE_NOTICE_SERVICE_H_
#define CHROME_BROWSER_PRIVACY_SANDBOX_NOTICE_NOTICE_SERVICE_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/privacy_sandbox/notice/notice.mojom.h"
#include "chrome/browser/privacy_sandbox/notice/notice_model.h"
#include "chrome/browser/privacy_sandbox/notice/notice_service_interface.h"
#include "chrome/browser/privacy_sandbox/notice/notice_storage.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/privacy_sandbox/notice/desktop_view_manager.h"
#endif  // !BUILDFLAG(IS_ANDROID)

class Profile;

namespace privacy_sandbox {

class NoticeCatalog;

// This class will:
// 1. Communicate with the Notice Storage Service
// 2. Communicate with the API Services to determine eligibility
// 3. Determine which views are required to get them to the latest API version
// 4. Keeps an internal registry to keep track of when notices were shown,
// what actions were taken on them and how
class PrivacySandboxNoticeService
    : public PrivacySandboxNoticeServiceInterface {
 public:
  PrivacySandboxNoticeService(Profile* profile,
                              std::unique_ptr<NoticeCatalog> catalog,
                              std::unique_ptr<NoticeStorage> storage);

  ~PrivacySandboxNoticeService() override;

  // NoticeServiceInterface:
  std::vector<notice::mojom::PrivacySandboxNotice> GetRequiredNotices(
      SurfaceType surface) override;

  void EventOccurred(NoticeId notice_id,
                     notice::mojom::PrivacySandboxNoticeEvent event) override;

#if !BUILDFLAG(IS_ANDROID)
  DesktopViewManagerInterface* GetDesktopViewManager() override;
#endif  // !BUILDFLAG(IS_ANDROID)

  // KeyedService:
  void Shutdown() override;

 private:
  void EmitStartupHistograms();

  NoticeStorage* notice_storage() { return notice_storage_.get(); }

  raw_ptr<Profile> profile_;
  std::unique_ptr<NoticeCatalog> catalog_;
  std::unique_ptr<NoticeStorage> notice_storage_;

#if !BUILDFLAG(IS_ANDROID)
  std::unique_ptr<DesktopViewManagerInterface> desktop_view_manager_;
#endif  // !BUILDFLAG(IS_ANDROID)
};

}  // namespace privacy_sandbox

#endif  // CHROME_BROWSER_PRIVACY_SANDBOX_NOTICE_NOTICE_SERVICE_H_
