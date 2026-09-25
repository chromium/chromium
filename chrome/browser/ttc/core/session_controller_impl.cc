// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/core/session_controller_impl.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/notimplemented.h"
#include "base/task/single_thread_task_runner.h"
#include "base/types/pass_key.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ttc/app/public/conversation.h"
#include "chrome/browser/ttc/core/session_view.h"
#include "chrome/browser/ttc/core/ttc_keyed_service.h"
#include "chrome/browser/ttc/core/ttc_page_context_monitor.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#else
#include "chrome/browser/ttc/core/session_view_impl.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/profile_browser_collection.h"
#endif

namespace ttc {

namespace {

std::unique_ptr<SessionView> MakeSessionView(
    [[maybe_unused]] SessionViewDelegate& delegate) {
#if BUILDFLAG(IS_ANDROID)
  // SessionViewImpl is views-based; Android will need its own implementation.
  return nullptr;
#else
  return std::make_unique<SessionViewImpl>(delegate);
#endif
}

}  // namespace

SessionControllerImpl::SessionControllerImpl(TtcKeyedService& service)
    : service_(service),
      session_view_(MakeSessionView(*this)),
      tool_controller_(service) {
  // Created here rather than in the initializer list because MakeConversation()
  // calls back into GetProfile() on this object.
  conversation_ =
      service.MakeConversation(base::PassKey<SessionControllerImpl>(), *this);

  if (content::WebContents* contents = GetObservedWebContents()) {
    // TODO(b/555800359): Reset page_context_monitor_ on active tab changes.
    page_context_monitor_ = std::make_unique<TtcPageContextMonitor>(
        *contents,
        base::BindRepeating(&SessionControllerImpl::OnPageContextChanged,
                            base::Unretained(this)));
  }

  if (conversation_) {
    conversation_->Start();
  }
}

SessionControllerImpl::~SessionControllerImpl() {
  if (conversation_) {
    conversation_->Stop();
  }
}

SessionLifecycle SessionControllerImpl::GetSessionLifecycle() const {
  return session_lifecycle_;
}

void SessionControllerImpl::SetSessionLifecycle(SessionLifecycle lifecycle) {
  if (session_lifecycle_ == lifecycle) {
    return;
  }

  session_lifecycle_ = lifecycle;

  if (session_lifecycle_ == SessionLifecycle::kFinished) {
    EndSessionAsync();
  }
}

void SessionControllerImpl::GetPageContext(FetchCompleteCallback callback) {
  if (!page_context_monitor_) {
    std::move(callback).Run(base::unexpected(
        page_content_annotations::FetchPageContextError::kWebContentsWentAway));
    return;
  }

  page_context_monitor_->StartNewFetch(std::move(callback));
}

Profile* SessionControllerImpl::GetProfile() {
  return service_->profile();
}

void SessionControllerImpl::ProcessToolCall(
    const ToolRequest& tool_request,
    ToolResponseCallback tool_response_callback) {
  tool_controller_.ProcessToolCall(tool_request,
                                   std::move(tool_response_callback));
}

std::vector<ToolDefinition> SessionControllerImpl::GetToolDefinitions() {
  return tool_controller_.GetToolDefinitions();
}

void SessionControllerImpl::UserAudioLevelUpdate(float audio_level) {
  // Android doesn't have a SessionView implementation yet.
  if (!session_view_) {
    NOTIMPLEMENTED();
    return;
  }

  session_view_->UpdateAudioLevel(audio_level);
}

BrowserWindowInterface* SessionControllerImpl::GetBrowserWindowInterface() {
#if BUILDFLAG(IS_ANDROID)
  return nullptr;
#else
  ProfileBrowserCollection* browsers =
      ProfileBrowserCollection::GetForProfile(service_->profile());
  return browsers ? browsers->GetLastActiveBrowser() : nullptr;
#endif
}

void SessionControllerImpl::OnSessionInitialized() {
  if (!session_view_) {
    NOTIMPLEMENTED();
    return;
  }

  session_view_->OnSessionInitialized();
}

void SessionControllerImpl::EndSessionAsync() {
  // Ending the session destroys this object so it must be done asynchronously.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&TtcKeyedService::EndSession, service_->GetWeakPtr()));
}

content::WebContents* SessionControllerImpl::GetObservedWebContents() {
#if BUILDFLAG(IS_ANDROID)
  Profile* profile = service_->profile();
  for (TabModel* model : TabModelList::models()) {
    if (model->GetProfile() == profile && model->IsActiveModel()) {
      tabs::TabInterface* active_tab = model->GetActiveTab();
      return active_tab ? active_tab->GetContents() : nullptr;
    }
  }
  return nullptr;
#else
  BrowserWindowInterface* window = GetBrowserWindowInterface();
  tabs::TabInterface* active_tab =
      window ? window->GetActiveTabInterface() : nullptr;
  return active_tab ? active_tab->GetContents() : nullptr;
#endif
}

void SessionControllerImpl::OnPageContextChanged() {
  if (conversation_) {
    conversation_->OnPageContextChanged();
  }
}

}  // namespace ttc
