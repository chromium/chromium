// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/glic_fre_page_handler.h"

#include "chrome/browser/glic/glic_fre_controller.h"
#include "chrome/browser/glic/glic_keyed_service.h"
#include "chrome/browser/glic/glic_keyed_service_factory.h"
#include "content/public/browser/web_contents.h"

namespace glic {

GlicFrePageHandler::GlicFrePageHandler(
    content::WebContents* webui_contents,
    mojo::PendingReceiver<glic::mojom::FrePageHandler> receiver)
    : webui_contents_(webui_contents), receiver_(this, std::move(receiver)) {}

GlicFrePageHandler::~GlicFrePageHandler() = default;

content::BrowserContext* GlicFrePageHandler::browser_context() const {
  return webui_contents_->GetBrowserContext();
}

GlicKeyedService* GlicFrePageHandler::GetGlicService() {
  return GlicKeyedServiceFactory::GetGlicKeyedService(browser_context());
}

void GlicFrePageHandler::AcceptFre() {
  GetGlicService()->window_controller().fre_controller()->AcceptFre(
      Profile::FromBrowserContext(browser_context()));
}

void GlicFrePageHandler::DismissFre() {
  GetGlicService()->window_controller().fre_controller()->DismissFre();
}

}  // namespace glic
