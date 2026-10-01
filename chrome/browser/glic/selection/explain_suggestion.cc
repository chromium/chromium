// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/selection/explain_suggestion.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/public/glic_invoke_options.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/selection/selection_overlay_controller.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/selection/mojom/action.mojom.h"
#include "components/search_engines/template_url_service.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/referrer.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

namespace glic {

ExplainSuggestion::ExplainSuggestion(tabs::TabInterface& tab) : tab_(tab) {
  // Unretained is safe: `this` owns the binder.
  SetInterface<selection::ExplainFulfillment>(
      base::BindRepeating(&ExplainSuggestion::Bind, base::Unretained(this)));
}

ExplainSuggestion::~ExplainSuggestion() = default;

const std::u16string& ExplainSuggestion::GetLabel() const {
  return label_;
}

void ExplainSuggestion::OnSuggestionPresented() {}

void ExplainSuggestion::OnSuggestionExecuted() {}

::selection::mojom::ActionPtr ExplainSuggestion::GetAction() const {
  return ::selection::mojom::Action::NewInlineFulfillment(
      ::selection::mojom::InlineFulfillment::New("explain_fulfillment.js"));
}

void ExplainSuggestion::GetExplanation(GetExplanationCallback callback) {
  // TODO(liuwilliam): Wire it up to the real response.
  std::move(callback).Run(kPlaceholderText);
}

void ExplainSuggestion::OpenTabForSearch(const std::string& query) {
  auto* controller =
      SelectionOverlayController::FromTabWebContents(tab_->GetContents());
  content::RenderFrameHost* initiator =
      controller ? controller->GetOverlayMainFrame() : nullptr;
  if (!initiator) {
    return;
  }
  TemplateURLService* template_url_service =
      TemplateURLServiceFactory::GetForProfile(tab_->GetProfile());
  CHECK(template_url_service);
  const GURL url =
      template_url_service->GenerateSearchURLForDefaultSearchProvider(
          base::UTF8ToUTF16(query));
  // Empty if there's no default search engine, e.g. turned off by policy.
  if (!url.is_valid()) {
    return;
  }
  // The request comes from the untrusted overlay WebUI. Only open the tab
  // right after a click in the overlay.
  if (!initiator->ConsumeTransientUserActivation()) {
    return;
  }
  content::OpenURLParams params(url, content::Referrer(),
                                WindowOpenDisposition::NEW_FOREGROUND_TAB,
                                ui::PAGE_TRANSITION_LINK,
                                /*is_renderer_initiated=*/true);
  params.user_gesture = true;
  // The overlay is in its own WebContents, so it's the initiator but not the
  // `source_*` frame, which must be in `tab_`'s WebContents.
  params.initiator_origin = initiator->GetLastCommittedOrigin();
  params.initiator_frame_token = initiator->GetFrameToken();
  params.initiator_process_id = initiator->GetProcess()->GetDeprecatedID();
  tab_->GetContents()->OpenURL(params, /*navigation_handle_callback=*/{});
}

void ExplainSuggestion::AskGemini() {
  GlicKeyedService* service = GlicKeyedService::Get(tab_->GetProfile());
  CHECK(service);
  // TODO(liuwilliam): Add the text.
  service->Invoke(GlicInvokeOptions(
      Target(*tab_), mojom::InvocationSource::kTextSelectionWidget));
}

void ExplainSuggestion::Bind(
    mojo::PendingAssociatedReceiver<selection::ExplainFulfillment> receiver) {
  // Each click shows a new card with its own channel. Drop the old card's.
  receiver_.reset();
  receiver_.Bind(std::move(receiver));
}

}  // namespace glic
