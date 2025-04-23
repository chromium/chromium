// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/host.h"

#include "base/containers/to_vector.h"
#include "chrome/browser/glic/glic_keyed_service.h"
#include "chrome/browser/glic/glic_profile_manager.h"
#include "chrome/browser/glic/host/glic_page_handler.h"
#include "chrome/browser/glic/host/webui_contents_container.h"
#include "components/guest_view/browser/guest_view_base.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/guest_view/web_view/web_view_guest.h"
#include "third_party/blink/public/common/web_preferences/web_preferences.h"

namespace glic {

Host::Host(Profile* profile) : profile_(profile) {}

Host::~Host() = default;

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

void Host::WebUIPageHandlerAdded(GlicPageHandler* page_handler) {
  page_handlers_.push_back({.page_handler = page_handler});
}

void Host::WebUIPageHandlerRemoved(GlicPageHandler* page_handler) {
  auto* info = FindInfo(page_handler);
  if (info) {
    int index = info - &page_handlers_[0];
    page_handlers_.erase(page_handlers_.begin() + index);
  }
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
  PageHandlerInfo* info = FindInfoForWebUiContents(top);
  CHECK(info);
  auto* webview = extensions::WebViewGuest::FromWebContents(guest_contents);
  CHECK(webview);
  info->page_handler->GuestAdded(webview);
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
  if (web_client && !primary_page_handler_) {
    primary_page_handler_ = page_handler;
  }
  if (primary_page_handler_ == page_handler) {
    if (!web_client) {
      primary_page_handler_ = nullptr;
    }
    // TODO(crbug.com/409332639): Hide direct access to the web client.
    glic_service().window_controller().SetWebClient(web_client);
  }
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

}  // namespace glic
