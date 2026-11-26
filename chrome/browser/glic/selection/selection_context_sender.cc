// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/selection/selection_context_sender.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/strings/string_util.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/public/glic_instance.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/service/glic_instance_coordinator.h"
#include "chrome/browser/glic/selection/selection_context.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/tabs/page_context_eligibility_helper.h"
#include "components/tabs/public/tab_interface.h"
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
  // Null without a `GlicKeyedService` (e.g. in tests).
  if (auto* coordinator = GlicSidePanelCoordinator::GetForTab(&*tab_)) {
    side_panel_state_subscription_ = coordinator->AddStateCallback(
        base::BindRepeating(&SelectionContextSender::OnSidePanelStateChanged,
                            weak_ptr_factory_.GetWeakPtr()));
  }

  if (glic_keyed_service_) {
    global_show_hide_subscription_ =
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
  did_activate_subscription_ = tab_->RegisterDidActivate(base::BindRepeating(
      &SelectionContextSender::OnTabActivated, weak_ptr_factory_.GetWeakPtr()));
}

SelectionContextSender::~SelectionContextSender() = default;

void SelectionContextSender::SetText(std::u16string_view text) {
  text_ = text;
  Sync();
}

const std::u16string& SelectionContextSender::GetText() const {
  return text_;
}

void SelectionContextSender::MarkTextSent(std::u16string_view text,
                                          SelectionTextLength length) {
  text_ = text;
  sent_ = {.text = text_, .length = length};
  GlicInstance* instance = glic_keyed_service_
                               ? glic_keyed_service_->GetInstanceForTab(&*tab_)
                               : nullptr;
  sent_instance_id_ = instance ? instance->id() : InstanceId();
}

bool SelectionContextSender::IsPanelShowing() const {
  // Matches `OnSidePanelStateChanged()`, which only syncs on `kShown`.
  auto* coordinator = GlicSidePanelCoordinator::GetForTab(&*tab_);
  return (coordinator && coordinator->IsShowing()) || IsFloatingPanelShowing();
}

bool SelectionContextSender::IsFloatingPanelShowing() const {
  if (!glic_keyed_service_) {
    return false;
  }
  GlicInstance* instance = glic_keyed_service_->GetInstanceForTab(&*tab_);
  return tab_->IsActivated() && instance && instance->IsShowing() &&
         instance->GetPanelState()->kind == mojom::PanelStateKind::kDetached;
}

void SelectionContextSender::OnSidePanelStateChanged(
    GlicSidePanelCoordinator::State state) {
  if (state == GlicSidePanelCoordinator::State::kShown) {
    Sync(Resend::kSelection);
  }
}

// The floating window has no per-tab state callback.
void SelectionContextSender::OnGlobalPanelShowHide() {
  if (!IsFloatingPanelShowing()) {
    return;
  }
  Sync(Resend::kSelection);
}

// Another tab may have changed the floating window's chip, so tell it this
// tab's selection, even "none".
void SelectionContextSender::OnTabActivated(tabs::TabInterface* tab) {
  if (IsFloatingPanelShowing()) {
    Sync(Resend::kAlways);
  }
}

void SelectionContextSender::OnPageContextEligibilityChanged(
    optimization_guide::PageContextEligibilityStatus status) {
  Sync();
}

// Keeps what the panel was told, so returning to the tab clears its chip.
void SelectionContextSender::WillDiscardContents(
    tabs::TabInterface* tab,
    content::WebContents* old_contents,
    content::WebContents* new_contents) {
  text_.clear();
}

void SelectionContextSender::Sync(Resend resend) {
  if (!IsPanelShowing() || !glic_keyed_service_) {
    return;
  }
  GlicInstance* instance = glic_keyed_service_->GetInstanceForTab(&*tab_);
  // TODO(b:579078690): Sync again when the invocation finishes.
  if (!instance || !instance->IsShowing() || instance->IsLiveMode() ||
      instance->IsInvoking()) {
    return;
  }
  if (instance->id() != sent_instance_id_) {
    // Invalid when `MarkTextSent()` ran before the tab had an instance; the
    // invocation that creates it already carries the text.
    if (sent_instance_id_.IsValid()) {
      sent_ = {};
    }
    sent_instance_id_ = instance->id();
  }
  const std::u16string& target =
      IsPageContextEligible() ? text_ : base::EmptyString16();
  bool resend_now = resend == Resend::kAlways ||
                    (resend == Resend::kSelection && !target.empty());
  if (target == sent_.text && !resend_now) {
    return;
  }
  SelectionTextLength length =
      target == sent_.text ? sent_.length : SelectionTextLength::kElided;
  instance->SendAdditionalContext(
      CreateTextSelectionContext(tab_->GetContents(), target, length));
  sent_ = {.text = target, .length = length};
}

bool SelectionContextSender::IsPageContextEligible() const {
  auto* helper = tabs::PageContextEligibilityHelper::From(&*tab_);
  return helper &&
         helper->IsPageContextEligible() ==
             optimization_guide::PageContextEligibilityStatus::kEligible;
}

}  // namespace glic
