// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/session_controller_impl.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/types/pass_key.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ttc/conversation.h"
#include "chrome/browser/ttc/core/ttc_page_context_monitor.h"
#include "chrome/browser/ttc/session_view.h"
#include "chrome/browser/ttc/ttc_keyed_service.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#else
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/profile_browser_collection.h"
#endif

namespace ttc {

SessionControllerImpl::SessionControllerImpl(TtcKeyedService& service)
    : service_(service),
      conversation_(
          service.MakeConversation(base::PassKey<SessionControllerImpl>())),
      session_view_(std::make_unique<SessionView>(*this)) {
  if (content::WebContents* contents = GetObservedWebContents()) {
    // TODO(b/555800359): Reset page_context_monitor_ on active tab changes.
    page_context_monitor_ = std::make_unique<TtcPageContextMonitor>(
        *contents,
        base::BindRepeating(&SessionControllerImpl::OnPageContextChanged,
                            base::Unretained(this)));
  }
}

SessionControllerImpl::~SessionControllerImpl() = default;

content::WebContents* SessionControllerImpl::GetObservedWebContents() {
  Profile* profile = service_->profile();

#if BUILDFLAG(IS_ANDROID)
  for (TabModel* model : TabModelList::models()) {
    if (model->GetProfile() == profile && model->IsActiveModel()) {
      tabs::TabInterface* active_tab = model->GetActiveTab();
      return active_tab ? active_tab->GetContents() : nullptr;
    }
  }
  return nullptr;
#else
  ProfileBrowserCollection* browsers =
      ProfileBrowserCollection::GetForProfile(profile);
  BrowserWindowInterface* window =
      browsers ? browsers->GetLastActiveBrowser() : nullptr;
  tabs::TabInterface* active_tab =
      window ? window->GetActiveTabInterface() : nullptr;
  return active_tab ? active_tab->GetContents() : nullptr;
#endif
}

void SessionControllerImpl::GetPageContext(FetchCompleteCallback callback) {
  if (!page_context_monitor_) {
    std::move(callback).Run(base::unexpected(
        page_content_annotations::FetchPageContextError::kWebContentsWentAway));
    return;
  }

  page_context_monitor_->StartNewFetch(std::move(callback));
}

void SessionControllerImpl::OnPageContextChanged() {
  if (conversation_) {
    conversation_->OnPageContextChanged();
  }
}

}  // namespace ttc
