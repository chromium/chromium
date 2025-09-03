// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/service/glic_instance.h"

#include "base/notimplemented.h"
#include "chrome/browser/glic/glic_zero_state_suggestions_manager.h"
#include "chrome/browser/glic/host/context/glic_screenshot_capturer.h"
#include "chrome/browser/glic/host/context/glic_sharing_manager_impl.h"
#include "chrome/browser/glic/host/glic_ui_embedder.h"
#include "chrome/browser/glic/host/host.h"
#include "chrome/browser/glic/widget/glic_floating_ui.h"
#include "chrome/browser/glic/widget/glic_side_panel_ui.h"
#include "chrome/browser/ui/browser_window.h"

namespace glic {

GlicInstance::GlicInstance(
    Profile* profile,
    BrowserWindowInterface* bwi,
    Host& host,
    base::WeakPtr<AttachmentDelegate> attachment_delegate)
    : profile_(profile),
      associated_bwi_(bwi),
      attachment_delegate_(attachment_delegate),
      host_(host) {}

GlicInstance::~GlicInstance() = default;

GlicUiEmbedder& GlicInstance::embedder() {
  CHECK(embedder_);
  return *embedder_;
}

void GlicInstance::AttachPanel() {
  if (!attachment_delegate_) {
    return;
  }
  attachment_delegate_->AttachInstance(this);
}

void GlicInstance::DetachPanel() {
  if (!attachment_delegate_) {
    return;
  }
  attachment_delegate_->DetachInstance(this);
}

bool GlicInstance::IsShowing() const {
  return embedder_ && embedder_->IsShowing();
}

void GlicInstance::SetEmbedderType(EmbedderType type) {
  embedder_type_ = type;
}

void GlicInstance::Show() {
  if (!embedder_) {
    switch (embedder_type_) {
      case EmbedderType::kSidePanel:
        CHECK(associated_bwi_);
        embedder_ = std::make_unique<GlicSidePanelUi>(associated_bwi_, *this);
        break;
      case EmbedderType::kFloating:
        embedder_ = std::make_unique<GlicFloatingUi>();
        break;
    }
  }
  host_->Initialize(embedder_.get());

  // Create the WebContents if it doesn't exist.
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
    Show();
  }
}

void GlicInstance::DisassociateWindow() {
  associated_bwi_ = nullptr;
}

void GlicInstance::ClosePanelAndShutdown() {
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

}  // namespace glic
