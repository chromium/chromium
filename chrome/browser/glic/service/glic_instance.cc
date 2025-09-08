// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/service/glic_instance.h"

#include "base/functional/bind.h"
#include "base/notimplemented.h"
#include "chrome/browser/glic/glic_zero_state_suggestions_manager.h"
#include "chrome/browser/glic/host/context/glic_screenshot_capturer.h"
#include "chrome/browser/glic/host/context/glic_sharing_manager_impl.h"
#include "chrome/browser/glic/host/glic_ui_embedder.h"
#include "chrome/browser/glic/host/host.h"
#include "chrome/browser/glic/service/glic_instance_coordinator_impl.h"
#include "chrome/browser/glic/widget/glic_floating_ui.h"
#include "chrome/browser/glic/widget/glic_side_panel_ui.h"
#include "chrome/browser/ui/browser_window.h"
#include "components/tabs/public/tab_interface.h"

namespace glic {

GlicInstance::GlicInstance(
    Profile* profile,
    std::unique_ptr<Host> host,
    ConversationId conversation_id,
    base::WeakPtr<AttachmentDelegate> attachment_delegate)
    : profile_(profile),
      attachment_delegate_(attachment_delegate),
      conversation_id_(conversation_id),
      host_(std::move(host)) {}

GlicInstance::~GlicInstance() = default;

GlicUiEmbedder& GlicInstance::embedder() {
  CHECK(embedder_);
  return *embedder_;
}

void GlicInstance::AttachInstance() {
  if (!attachment_delegate_) {
    return;
  }
  attachment_delegate_->AttachInstance(this);
}

void GlicInstance::DetachInstance() {
  if (!attachment_delegate_) {
    return;
  }
  attachment_delegate_->DetachInstance(this);
}

bool GlicInstance::IsShowing() const {
  return embedder_ && embedder_->IsShowing();
}

GlicInstance::EmbedderType GlicInstance::GetEmbedderType() {
  return embedder_type_;
}

void GlicInstance::SetEmbedderType(EmbedderType type) {
  embedder_type_ = type;
}

void GlicInstance::Show(tabs::TabInterface* tab) {
  if (tab) {
    AssociateWithTab(tab);
  }
  if (!embedder_) {
    switch (embedder_type_) {
      case EmbedderType::kSidePanel:
        CHECK(tab);
        embedder_ = std::make_unique<GlicSidePanelUi>(tab->GetWeakPtr(), *this);
        break;
      case EmbedderType::kFloating:
        embedder_ = std::make_unique<GlicFloatingUi>();
        break;
    }
  }
  host_->Initialize(embedder_.get());

  // Create the WebContents if it's not already created.
  host_->CreateContents(/*initially_hidden=*/false);
  host_->NotifyWindowIntentToShow();

  embedder_->Show();

  // TODO: NotifyPanelStateChanged() here
  // TODO: pass in the correct invocation source
  host_->PanelWillOpen(mojom::InvocationSource::kTopChromeButton);
}

void GlicInstance::Close() {
  if (embedder_) {
    // TODO: This function isn't in Host::Delegate, but we'll need it.
    // embedder_->Close();
    embedder_.reset();
  }
}

void GlicInstance::Toggle() {
  if (IsShowing()) {
    Close();
  } else {
    // TODO: Maybe it doesn't make sense to include toggle in this interface,
    // because it doesn't know which tab to show on.
    // Show();
    NOTIMPLEMENTED();
  }
}

void GlicInstance::CloseInstanceAndShutdown() {
  NOTIMPLEMENTED();
}
void GlicInstance::CreateTab() {
  NOTIMPLEMENTED();
}
void GlicInstance::CreateTask() {
  NOTIMPLEMENTED();
}
void GlicInstance::PerformActions() {
  NOTIMPLEMENTED();
}
void GlicInstance::StopActorTask() {
  NOTIMPLEMENTED();
}
void GlicInstance::PauseActorTask() {
  NOTIMPLEMENTED();
}
void GlicInstance::ResumeActorTask() {
  NOTIMPLEMENTED();
}
void GlicInstance::GetZeroStateSuggestionsAndSubscribe() {
  NOTIMPLEMENTED();
}
void GlicInstance::GetZeroStateSuggestionsForFocusedTab() {
  NOTIMPLEMENTED();
}

void GlicInstance::AssociateWithTab(tabs::TabInterface* tab) {
  auto* helper = GlicConversationHelper::From(tab);
  CHECK(helper);
  associated_tab_subscriptions_[tab] = helper->SubscribeToDestruction(
      base::BindRepeating(&GlicInstance::OnAssociatedTabDestroyed,
                          weak_ptr_factory_.GetWeakPtr()));
}

void GlicInstance::DisassociateFromTab(tabs::TabInterface* tab) {
  associated_tab_subscriptions_.erase(tab);
}

bool GlicInstance::IsOrphaned() const {
  return associated_tab_subscriptions_.empty();
}

void GlicInstance::OnAssociatedTabDestroyed(
    tabs::TabInterface* tab,
    const ConversationId& conversation_id) {
  DisassociateFromTab(tab);
  if (IsOrphaned() && attachment_delegate_) {
    attachment_delegate_->OnInstanceOrphaned(this);
  }
}

}  // namespace glic
