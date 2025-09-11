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
#include "chrome/browser/glic/widget/glic_inactive_side_panel_ui.h"
#include "chrome/browser/glic/widget/glic_side_panel_ui.h"
#include "chrome/browser/ui/browser_window.h"
#include "components/tabs/public/tab_interface.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"

namespace glic {

GlicInstance::EmbedderEntry::EmbedderEntry() = default;
GlicInstance::EmbedderEntry::~EmbedderEntry() = default;
GlicInstance::EmbedderEntry::EmbedderEntry(EmbedderEntry&&) = default;
GlicInstance::EmbedderEntry& GlicInstance::EmbedderEntry::operator=(
    EmbedderEntry&&) = default;

GlicInstance::GlicInstance(
    Profile* profile,
    std::unique_ptr<Host> host,
    InstanceId instance_id,
    base::WeakPtr<AttachmentDelegate> attachment_delegate)
    : profile_(profile),
      attachment_delegate_(attachment_delegate),
      id_(instance_id),
      host_(std::move(host)) {}

GlicInstance::~GlicInstance() = default;

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
  return active_embedder_key_.has_value();
}

void GlicInstance::Show(EmbedderType type, tabs::TabInterface* tab) {
  EmbedderKey new_key = GetEmbedderKey(type, tab);

  if (active_embedder_key_.has_value() &&
      active_embedder_key_.value() == new_key) {
    return;
  }

  DeactivateCurrentEmbedder();
  auto* embedder_to_show = CreateActiveEmbedderFor(new_key);
  active_embedder_key_ = new_key;

  MaybeShowHostUi(embedder_to_show);
  embedder_to_show->Show();
}

void GlicInstance::Close() {
  DeactivateCurrentEmbedder();
}

void GlicInstance::Toggle(EmbedderType type, tabs::TabInterface* tab) {
  EmbedderKey key = GetEmbedderKey(type, tab);
  if (active_embedder_key_.has_value() && active_embedder_key_.value() == key) {
    Close();
  } else {
    Show(type, tab);
  }
}

std::unique_ptr<views::View> GlicInstance::CreateViewForSidePanel(
    tabs::TabInterface* tab) {
  if (auto* embedder = GetEmbedderForTab(tab)) {
    return embedder->CreateView();
  }
  return nullptr;
}

GlicUiEmbedder* GlicInstance::GetEmbedderForTab(tabs::TabInterface* tab) {
  auto it = embedders_.find(EmbedderKey(tab));
  if (it != embedders_.end()) {
    return it->second.embedder.get();
  }
  return nullptr;
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

void GlicInstance::DisassociateFromTab(tabs::TabInterface* tab) {
  if (active_embedder_key_.has_value() &&
      active_embedder_key_.value() == EmbedderKey(tab)) {
    DeactivateCurrentEmbedder();
  }
  embedders_.erase(EmbedderKey(tab));
}

bool GlicInstance::IsOrphaned() const {
  // An instance is orphaned if it has no tab associations. The floating
  // embedder does not count.
  for (const auto& [key, entry] : embedders_) {
    if (std::holds_alternative<tabs::TabInterface*>(key)) {
      return false;
    }
  }
  return true;
}

GlicInstance::EmbedderKey GlicInstance::GetEmbedderKey(
    EmbedderType type,
    tabs::TabInterface* tab) {
  if (type == EmbedderType::kSidePanel) {
    CHECK(tab);
    return tab;
  }
  return FloatingEmbedderKey();
}

GlicUiEmbedder* GlicInstance::GetActiveEmbedder() {
  if (!active_embedder_key_.has_value()) {
    return nullptr;
  }
  auto it = embedders_.find(active_embedder_key_.value());
  if (it != embedders_.end()) {
    return it->second.embedder.get();
  }
  return nullptr;
}

void GlicInstance::DeactivateCurrentEmbedder() {
  auto* old_embedder = GetActiveEmbedder();
  if (!old_embedder) {
    active_embedder_key_.reset();
    return;
  }

  auto it = embedders_.find(active_embedder_key_.value());
  CHECK(it != embedders_.end());
  it->second.embedder = old_embedder->CreateInactiveEmbedder();

  active_embedder_key_.reset();
}

GlicUiEmbedder* GlicInstance::CreateActiveEmbedderFor(const EmbedderKey& key) {
  EmbedderEntry new_entry;
  std::visit(
      absl::Overload{
          [&](FloatingEmbedderKey) {
            new_entry.embedder = std::make_unique<GlicFloatingUi>();
          },
          [&](tabs::TabInterface* tab) {
            new_entry.embedder =
                std::make_unique<GlicSidePanelUi>(tab->GetWeakPtr(), *this);
            auto* helper = GlicInstanceHelper::From(tab);
            CHECK(helper);
            new_entry.destruction_subscription = helper->SubscribeToDestruction(
                base::BindRepeating(&GlicInstance::OnAssociatedTabDestroyed,
                                    weak_ptr_factory_.GetWeakPtr()));
          },
      },
      key);

  auto* embedder_ptr = new_entry.embedder.get();
  embedders_.insert_or_assign(key, std::move(new_entry));
  return embedder_ptr;
}

void GlicInstance::MaybeShowHostUi(GlicUiEmbedder* embedder) {
  Host::Delegate* delegate = embedder->GetHostDelegate();
  if (!delegate) {
    return;
  }

  host_->Initialize(delegate);

  // Create the WebContents if it's not already created.
  host_->CreateContents(/*initially_hidden=*/false);
  host_->NotifyWindowIntentToShow();

  // TODO: NotifyPanelStateChanged() here
  // TODO: pass in the correct invocation source
  host_->PanelWillOpen(mojom::InvocationSource::kTopChromeButton);
}

void GlicInstance::OnAssociatedTabDestroyed(tabs::TabInterface* tab,
                                            const InstanceId& instance_id) {
  DisassociateFromTab(tab);
  if (IsOrphaned() && attachment_delegate_) {
    attachment_delegate_->OnInstanceOrphaned(this);
  }
}

}  // namespace glic
