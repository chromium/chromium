// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/session_controller_impl.h"

#include <utility>

#include "base/functional/bind.h"
#include "build/build_config.h"
#include "chrome/browser/ttc/conversation.h"
#include "chrome/browser/ttc/core/ttc_page_context_monitor.h"
#include "chrome/browser/ttc/session_view.h"
#include "chrome/browser/ttc/ttc_keyed_service.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/profile_browser_collection.h"
#include "components/tabs/public/tab_interface.h"
#endif

namespace ttc {

SessionControllerImpl::SessionControllerImpl(TtcKeyedService& service)
    : service_(service),
      conversation_(Conversation::Create(service.profile())),
      session_view_(std::make_unique<SessionView>(*this)) {
  // TODO(bokan): How should we get the active tab/WebContents on Android?
#if !BUILDFLAG(IS_ANDROID)
  ProfileBrowserCollection* browsers =
      ProfileBrowserCollection::GetForProfile(service.profile());
  BrowserWindowInterface* window =
      browsers ? browsers->GetLastActiveBrowser() : nullptr;
  tabs::TabInterface* active_tab =
      window ? window->GetActiveTabInterface() : nullptr;
  if (active_tab) {
    // TODO(b/555800359): Reset page_context_monitor_ on active tab changes.
    page_context_monitor_ = std::make_unique<TtcPageContextMonitor>(
        *active_tab->GetContents(),
        base::BindRepeating(&SessionControllerImpl::OnPageContextChanged,
                            base::Unretained(this)));
  }
#endif
}

SessionControllerImpl::~SessionControllerImpl() = default;

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
