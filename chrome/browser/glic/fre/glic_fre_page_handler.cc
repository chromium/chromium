// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/fre/glic_fre_page_handler.h"

#include <optional>

#include "base/functional/callback_helpers.h"
#include "chrome/browser/glic/fre/glic_fre_controller.h"
#include "chrome/browser/glic/glic_keyed_service.h"
#include "chrome/browser/glic/glic_keyed_service_factory.h"
#include "chrome/browser/glic/widget/glic_window_controller.h"
#include "content/public/browser/web_contents.h"

namespace glic {

GlicFrePageHandler::GlicFrePageHandler(
    content::WebContents* webui_contents,
    mojo::PendingReceiver<glic::mojom::FrePageHandler> receiver)
    : webui_contents_(webui_contents), receiver_(this, std::move(receiver)) {}

GlicFrePageHandler::~GlicFrePageHandler() {
  WebUiStateChanged(mojom::FreWebUiState::kUninitialized);
}

content::BrowserContext* GlicFrePageHandler::browser_context() const {
  return webui_contents_->GetBrowserContext();
}

GlicKeyedService* GlicFrePageHandler::GetGlicService() {
  return GlicKeyedServiceFactory::GetGlicKeyedService(browser_context());
}

void GlicFrePageHandler::AcceptFre() {
  GetGlicService()->window_controller().fre_controller()->AcceptFre();
}

void GlicFrePageHandler::DismissFre() {
  GetGlicService()->window_controller().fre_controller()->OnNoThanksClicked();
}

void GlicFrePageHandler::PrepareForClient(
    base::OnceCallback<void(bool)> callback) {
  GetGlicService()->window_controller().fre_controller()->PrepareForClient(
      std::move(callback));
}

void GlicFrePageHandler::ValidateAndOpenLinkInNewTab(const GURL& url) {
  if (url.DomainIs("google.com")) {
    GetGlicService()->CreateTab(url, /*open_in_background=*/true, std::nullopt,
                                base::DoNothing());
    GetGlicService()->window_controller().fre_controller()->OnLinkClicked(url);
  }
}

void GlicFrePageHandler::WebUiStateChanged(mojom::FreWebUiState new_state) {
  GetGlicService()->window_controller().fre_controller()->WebUiStateChanged(
      new_state);
}

}  // namespace glic
