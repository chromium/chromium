// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/selection/selection_context_sender.h"

#include <utility>

#include "base/functional/bind.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/public/glic_instance.h"
#include "chrome/browser/glic/public/glic_invoke_options.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/service/glic_instance_coordinator.h"
#include "chrome/browser/glic/selection/selection_context.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/page_context_eligibility_helper.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/web_contents.h"

namespace glic {

DEFINE_SAFE_CAST_TARGET(SelectionContextSender)

// static
SelectionContextSender* SelectionContextSender::From(
    base::PassKey<GlicSelectionObserver>,
    tabs::TabInterface* tab) {
  auto* page_selection = ::selection::PageSelection::From(tab);
  return page_selection ? page_selection->AsA<SelectionContextSender>()
                        : nullptr;
}

SelectionContextSender::SelectionContextSender(tabs::TabInterface& tab)
    : tab_(tab),
      glic_keyed_service_(GlicKeyedService::Get(
          Profile::FromBrowserContext(tab.GetContents()->GetBrowserContext()))),
      scoped_page_selection_user_data_(tab.GetUnownedUserDataHost(), *this) {
  if (glic_keyed_service_) {
    panel_state_subscription_ =
        glic_keyed_service_->instance_coordinator().AddGlobalShowHideCallback(
            base::BindRepeating(&SelectionContextSender::OnGlobalPanelShowHide,
                                weak_ptr_factory_.GetWeakPtr()));
  }

  if (auto* helper = tabs::PageContextEligibilityHelper::From(&*tab_)) {
    page_context_eligibility_subscription_ =
        helper->RegisterEligibilityChangeCallback(base::BindRepeating(
            &SelectionContextSender::OnPageContextEligibilityChanged,
            weak_ptr_factory_.GetWeakPtr()));
  }

  discard_contents_subscription_ = tab_->RegisterWillDiscardContents(
      base::BindRepeating(&SelectionContextSender::WillDiscardContents,
                          weak_ptr_factory_.GetWeakPtr()));
}

SelectionContextSender::~SelectionContextSender() = default;

void SelectionContextSender::SetText(std::u16string_view text) {
  text_ = text;
  Sync();
}

void SelectionContextSender::Sync() {
  if (text_.empty()) {
    if (has_sent_selection_context_) {
      Push(u"");
      has_sent_selection_context_ = false;
    }
    return;
  }

  has_sent_selection_context_ = IsPanelShowing() && Push(text_);
}

const std::u16string& SelectionContextSender::GetText() const {
  return text_;
}

void SelectionContextSender::MarkTextSent(std::u16string_view text) {
  text_ = text;
  has_sent_selection_context_ = !text_.empty();
}

bool SelectionContextSender::IsPanelShowing() const {
  BrowserWindowInterface* bwi = tab_->GetBrowserWindowInterface();
  if (bwi && glic_keyed_service_ &&
      glic_keyed_service_->GetInstanceForTab(&*tab_)) {
    return glic_keyed_service_->IsPanelShowingForBrowser(*bwi);
  }
  return false;
}

void SelectionContextSender::OnGlobalPanelShowHide() {
  if (text_.empty()) {
    return;
  }
  Sync();
}

void SelectionContextSender::OnPageContextEligibilityChanged(
    optimization_guide::PageContextEligibilityStatus status) {
  if (status != optimization_guide::PageContextEligibilityStatus::kEligible &&
      has_sent_selection_context_) {
    Push(u"");
    has_sent_selection_context_ = false;
  } else if (status ==
                 optimization_guide::PageContextEligibilityStatus::kEligible &&
             !text_.empty() && IsPanelShowing()) {
    has_sent_selection_context_ = Push(text_);
  }
}

void SelectionContextSender::WillDiscardContents(
    tabs::TabInterface* tab,
    content::WebContents* old_contents,
    content::WebContents* new_contents) {
  text_.clear();
  has_sent_selection_context_ = false;
}

bool SelectionContextSender::Push(const std::u16string& selected_text) {
  if (!glic_keyed_service_) {
    return false;
  }

  if (!IsPageContextEligible() && !selected_text.empty()) {
    return false;
  }

  mojom::AdditionalContextPtr context = CreateTextSelectionContext(
      tab_->GetContents(), selected_text, SelectionTextLength::kElided);

  // Common case: the tab's instance already exists and is showing. Hand the
  // update straight to the instance instead of starting a full invocation.
  if (GlicInstance* instance = glic_keyed_service_->GetInstanceForTab(&*tab_)) {
    if (instance->IsShowing()) {
      if (instance->IsLiveMode()) {
        return false;
      }
      instance->SendAdditionalContext(std::move(context));
      return true;
    }
  }

  // Never start an invocation just to send an empty selection.
  if (selected_text.empty()) {
    return false;
  }

  GlicInvokeOptions options(glic::Target(*tab_),
                            mojom::InvocationSource::kTextSelectionWidget);
  options.preserve_active_surface = true;
  options.target.live_mode_behavior = LiveModeBehavior::kFail;
  options.additional_context = AdditionalTabContext(
      std::move(context), content::GlobalRenderFrameHostId(),
      PolicyCheck::kNone);
  glic_keyed_service_->Invoke(std::move(options));
  return true;
}

bool SelectionContextSender::IsPageContextEligible() const {
  auto* helper = tabs::PageContextEligibilityHelper::From(&*tab_);
  return helper &&
         helper->IsPageContextEligible() ==
             optimization_guide::PageContextEligibilityStatus::kEligible;
}

}  // namespace glic
