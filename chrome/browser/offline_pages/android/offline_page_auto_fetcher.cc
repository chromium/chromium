// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/offline_pages/android/offline_page_auto_fetcher.h"

#include <utility>

#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/offline_pages/android/offline_page_auto_fetcher_service.h"
#include "chrome/browser/offline_pages/android/offline_page_auto_fetcher_service_factory.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "url/gurl.h"

namespace offline_pages {
namespace {

TabAndroid* FindTab(content::RenderFrameHost* render_frame_host) {
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(render_frame_host);
  if (!web_contents)
    return nullptr;
  TabModel* tab_model = TabModelList::GetTabModelForWebContents(web_contents);
  if (!tab_model)
    return nullptr;
  // For this use-case, it's OK to fail if the active tab doesn't match
  // web_contents.
  if (tab_model->GetActiveWebContents() != web_contents)
    return nullptr;

  return tab_model->GetTabAt(tab_model->GetActiveIndex());
}

}  // namespace

OfflinePageAutoFetcher::OfflinePageAutoFetcher(
    content::RenderFrameHost* render_frame_host)
    : last_committed_url_(render_frame_host->GetLastCommittedURL()) {
  TabAndroid* tab = FindTab(render_frame_host);
  if (!tab) {
    return;
  }
  auto_fetcher_service_ =
      OfflinePageAutoFetcherServiceFactory::GetForBrowserContext(
          render_frame_host->GetProcess()->GetBrowserContext());
  android_tab_id_ = tab->GetAndroidId();
}

OfflinePageAutoFetcher::~OfflinePageAutoFetcher() = default;

void OfflinePageAutoFetcher::TrySchedule(bool user_requested,
                                         TryScheduleCallback callback) {
  if (!auto_fetcher_service_) {
    std::move(callback).Run(OfflinePageAutoFetcherScheduleResult::kOtherError);
    return;
  }

  auto_fetcher_service_->TrySchedule(user_requested, last_committed_url_,
                                     android_tab_id_, std::move(callback));
}

void OfflinePageAutoFetcher::CancelSchedule() {
  if (!auto_fetcher_service_)
    return;
  auto_fetcher_service_->CancelSchedule(last_committed_url_);
}

// static
void OfflinePageAutoFetcher::Create(
    content::RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<chrome::mojom::OfflinePageAutoFetcher> receiver) {
  // Lifetime of the self owned receiver can exceed the RenderFrameHost, so
  // OfflinePageAutoFetcher does not retain a reference.
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<OfflinePageAutoFetcher>(render_frame_host),
      std::move(receiver));
}

}  // namespace offline_pages
