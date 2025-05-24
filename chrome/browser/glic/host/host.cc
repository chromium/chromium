// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/host.h"

#include "base/containers/to_vector.h"
#include "chrome/browser/glic/glic_keyed_service.h"
#include "chrome/browser/glic/glic_profile_manager.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/host/glic_page_handler.h"
#include "chrome/browser/glic/host/webui_contents_container.h"
#include "chrome/browser/glic/widget/glic_window_controller.h"
#include "components/guest_view/browser/guest_view_base.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/guest_view/web_view/web_view_guest.h"
#include "third_party/blink/public/common/web_preferences/web_preferences.h"

namespace glic {

Host::PageHandlerInfo::PageHandlerInfo() = default;
Host::PageHandlerInfo::~PageHandlerInfo() = default;
Host::PageHandlerInfo::PageHandlerInfo(PageHandlerInfo&&) = default;
Host::PageHandlerInfo& Host::PageHandlerInfo::operator=(PageHandlerInfo&&) =
    default;

Host::Host(Profile* profile) : profile_(profile) {}

Host::~Host() = default;

void Host::Initialize(Delegate* delegate) {
  delegate_ = delegate;
}

void Host::Destroy() {
  Shutdown();
  delegate_ = nullptr;
}

void Host::Shutdown() {
  contents_.reset();
}

void Host::CreateContents() {
  if (!contents_) {
    contents_ = std::make_unique<WebUIContentsContainer>(
        profile_, &glic_service().window_controller());
    glic::GlicProfileManager::GetInstance()->OnLoadingClientForService(
        &glic_service());
  }
}

void Host::PanelWillOpen(mojom::InvocationSource invocation_source) {
  CHECK(delegate_);
  invocation_source_ = invocation_source;
  for (auto& entry : page_handlers_) {
    if (!entry.web_client) {
      continue;
    }
    entry.web_client->PanelWillOpen(
        mojom::PanelOpeningData::New(delegate_->GetPanelState().Clone(),
                                     invocation_source),
        base::BindOnce(
            &Host::PanelWillOpenComplete,
            // Unretained is safe because web client is owned by `contents_`.
            base::Unretained(this),
            // Unretained is safe because web_client is calling us.
            base::Unretained(entry.web_client)));
  }
}

void Host::PanelWasClosed() {
  invocation_source_ = std::nullopt;
  for (auto& entry : page_handlers_) {
    if (entry.web_client) {
      entry.web_client->PanelWasClosed(base::DoNothing());
    }
    entry.open_complete = false;
  }
}

void Host::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void Host::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void Host::WebUIPageHandlerAdded(GlicPageHandler* page_handler) {
  PageHandlerInfo info;
  info.page_handler = page_handler;
  page_handlers_.push_back(std::move(info));

  // We effectively just want to pick the first page handler as the primary one.
  // There are two reasons why there can be more than one glic page handler:
  // 1. The chrome://glic URL can be loaded in a tab, which is used for
  //    development sometimes.
  // 2. The glic window supports right-click->Reload. When this happens, there
  //    is momentarily two page handlers for the same web contents. Since this
  //    can affect real users, it needs to be handled specially here.
  if (contents_ &&
      contents_->web_contents() == page_handler->webui_contents()) {
    if (primary_page_handler_) {
      // Allow replacing the primary page handler if the new handler is running
      // in the primary web contents. Note that the old handler may also have
      // the same web contents, but in the case of Reload, it will be removed
      // soon.
      WebUiStateChanged(primary_page_handler_,
                        mojom::WebUiState::kUninitialized);
      primary_page_handler_ = nullptr;
    }
    primary_page_handler_ = page_handler;
  }
  if (!primary_page_handler_) {
    primary_page_handler_ = page_handler;
  }
}

void Host::WebUIPageHandlerRemoved(GlicPageHandler* page_handler) {
  auto* info = FindInfo(page_handler);
  if (info) {
    int index = info - &page_handlers_[0];
    page_handlers_.erase(page_handlers_.begin() + index);
  }
  if (primary_page_handler_ == page_handler) {
    WebUiStateChanged(page_handler, mojom::WebUiState::kUninitialized);
    primary_page_handler_ = nullptr;
  }
}

void Host::LoginPageCommitted(GlicPageHandler* page_handler) {
  observers_.Notify(&Observer::LoginPageCommitted);
}

GlicKeyedService& Host::glic_service() {
  return *GlicKeyedService::Get(profile_);
}

Host::PageHandlerInfo* Host::FindInfo(GlicPageHandler* handler) {
  for (auto& info : page_handlers_) {
    if (info.page_handler == handler) {
      return &info;
    }
  }
  return nullptr;
}

Host::PageHandlerInfo* Host::FindInfoForClient(GlicWebClientAccess* client) {
  for (auto& info : page_handlers_) {
    if (info.web_client == client) {
      return &info;
    }
  }
  return nullptr;
}

Host::PageHandlerInfo* Host::FindInfoForWebUiContents(
    content::WebContents* web_contents) {
  for (auto& info : page_handlers_) {
    if (info.page_handler->webui_contents() == web_contents) {
      return &info;
    }
  }
  return nullptr;
}

GlicPageHandler* Host::FindPageHandlerForWebUiContents(
    const content::WebContents* webui_contents) {
  for (auto& entry : page_handlers_) {
    if (entry.page_handler->webui_contents() == webui_contents) {
      return entry.page_handler;
    }
  }
  return nullptr;
}

void Host::GuestAdded(content::WebContents* guest_contents) {
  content::WebContents* top =
      guest_view::GuestViewBase::GetTopLevelWebContents(guest_contents);

  if (contents_) {
    // TODO(harringtond): This looks wrong, either fix or document this.
    blink::web_pref::WebPreferences prefs(top->GetOrCreateWebPreferences());
    prefs.default_font_size = contents_->web_contents()
                                  ->GetOrCreateWebPreferences()
                                  .default_font_size;
    top->SetWebPreferences(prefs);
  }
}

void Host::NotifyWindowIntentToShow() {
  for (auto& entry : page_handlers_) {
    entry.page_handler->NotifyWindowIntentToShow();
  }
}

void Host::SetWebClient(GlicPageHandler* page_handler,
                        GlicWebClientAccess* web_client) {
  PageHandlerInfo* info = FindInfo(page_handler);
  CHECK(info);
  info->web_client = web_client;

  if (invocation_source_ && web_client) {
    web_client->PanelWillOpen(
        mojom::PanelOpeningData::New(delegate_->GetPanelState().Clone(),
                                     *invocation_source_),
        base::BindOnce(
            &Host::PanelWillOpenComplete,
            // Unretained is safe because web client is owned by `contents_`.
            base::Unretained(this),
            // Unretained is safe because web_client is calling us.
            base::Unretained(web_client)));
  }
}

void Host::WebClientInitializeFailed(GlicWebClientAccess* web_client) {
  auto* primary_info = FindInfo(primary_page_handler_);
  if (primary_info && primary_info->web_client == web_client) {
    observers_.Notify(&Observer::WebClientInitializeFailed);
  }
}

GlicWebClientAccess* Host::GetPrimaryWebClient() {
  Host::PageHandlerInfo* info = FindInfo(primary_page_handler_);
  return info ? info->web_client : nullptr;
}

bool Host::IsPrimaryClientOpen() {
  Host::PageHandlerInfo* info = FindInfo(primary_page_handler_);
  return info ? info->open_complete : false;
}

content::WebContents* Host::webui_contents() {
  return contents_ ? contents_->web_contents() : nullptr;
}

bool Host::IsGlicWebUiHost(content::RenderProcessHost* host) {
  for (auto& entry : page_handlers_) {
    if (entry.page_handler->webui_contents()
            ->GetPrimaryMainFrame()
            ->GetProcess() == host) {
      return true;
    }
  }
  return false;
}

bool Host::IsGlicWebUi(content::WebContents* contents) {
  return FindInfoForWebUiContents(contents) != nullptr;
}

std::vector<GlicPageHandler*> Host::GetPageHandlersForTesting() {
  return base::ToVector(
      page_handlers_,
      [](PageHandlerInfo& e) -> GlicPageHandler* { return e.page_handler; });
}

GlicPageHandler* Host::GetPrimaryPageHandlerForTesting() {
  return primary_page_handler_;
}

void Host::PanelWillOpenComplete(GlicWebClientAccess* client,
                                 mojom::OpenPanelInfoPtr open_info) {
  CHECK(client);
  // If the panel was closed before opening finished, return early.
  if (!invocation_source_) {
    return;
  }
  PageHandlerInfo* info = FindInfoForClient(client);
  CHECK(info);
  if (info->page_handler == primary_page_handler_) {
    info->open_complete = true;
    observers_.Notify(&Observer::ClientReadyToShow, *open_info);
  }
}

bool Host::IsReady() const {
  for (auto& entry : page_handlers_) {
    if (entry.page_handler == primary_page_handler_) {
      return entry.web_client != nullptr;
    }
  }
  return false;
}

void Host::WebUiStateChanged(GlicPageHandler* page_handler,
                             mojom::WebUiState new_state) {
  if (page_handler != primary_page_handler_) {
    return;
  }
  base::UmaHistogramEnumeration("Glic.PanelWebUiState", new_state);
  if (primary_webui_state_ != new_state) {
    // UI State has changed
    primary_webui_state_ = new_state;
    observers_.Notify(&Observer::WebUiStateChanged, primary_webui_state_);
  }
}

}  // namespace glic
