// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/service/glic_instance_impl.h"

#include "base/functional/bind.h"
#include "base/notimplemented.h"
#include "chrome/browser/contextual_cueing/contextual_cueing_service.h"
#include "chrome/browser/contextual_cueing/contextual_cueing_service_factory.h"
#include "chrome/browser/glic/glic_zero_state_suggestions_manager.h"
#include "chrome/browser/glic/host/context/glic_empty_focused_browser_manager.h"
#include "chrome/browser/glic/host/context/glic_empty_focused_tab_manager.h"
#include "chrome/browser/glic/host/context/glic_screenshot_capturer.h"
#include "chrome/browser/glic/host/context/glic_sharing_manager_impl.h"
#include "chrome/browser/glic/host/glic_ui_embedder.h"
#include "chrome/browser/glic/host/host.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/service/glic_instance_coordinator_impl.h"
#include "chrome/browser/glic/widget/glic_floating_ui.h"
#include "chrome/browser/glic/widget/glic_inactive_side_panel_ui.h"
#include "chrome/browser/glic/widget/glic_side_panel_ui.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "components/tabs/public/tab_interface.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"

namespace glic {

GlicInstanceImpl::EmbedderEntry::EmbedderEntry() = default;
GlicInstanceImpl::EmbedderEntry::~EmbedderEntry() = default;
GlicInstanceImpl::EmbedderEntry::EmbedderEntry(EmbedderEntry&&) = default;
GlicInstanceImpl::EmbedderEntry& GlicInstanceImpl::EmbedderEntry::operator=(
    EmbedderEntry&&) = default;

GlicInstanceImpl::GlicInstanceImpl(
    Profile* profile,
    InstanceId instance_id,
    base::WeakPtr<AttachmentDelegate> attachment_delegate,
    GlicMetrics* metrics)
    : profile_(profile),
      attachment_delegate_(attachment_delegate),
      id_(instance_id),
      host_(profile_, this, this),
      sharing_manager_(
          std::make_unique<GlicEmptyFocusedTabManager>(),
          std::make_unique<GlicEmptyFocusedBrowserManager>(),
          std::make_unique<GlicPinnedTabManager>(profile, this, metrics),
          profile,
          metrics) {}

GlicInstanceImpl::~GlicInstanceImpl() = default;

void GlicInstanceImpl::AttachInstance() {
  if (!attachment_delegate_) {
    return;
  }
  attachment_delegate_->AttachInstance(this);
}

void GlicInstanceImpl::DetachInstance() {
  if (!attachment_delegate_) {
    return;
  }
  attachment_delegate_->DetachInstance(this);
}

bool GlicInstanceImpl::IsShowing() const {
  return active_embedder_key_.has_value();
}

void GlicInstanceImpl::Show(EmbedderType type, tabs::TabInterface* tab) {
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

void GlicInstanceImpl::Close(EmbedderType type, tabs::TabInterface* tab) {
  EmbedderKey key = GetEmbedderKey(type, tab);
  auto* embedder = GetEmbedderForKey(key);
  if (embedder) {
    embedder->Close();
  }
  if (active_embedder_key_.has_value() && active_embedder_key_.value() == key) {
    DeactivateCurrentEmbedder();
  }
}

void GlicInstanceImpl::Toggle(EmbedderType type, tabs::TabInterface* tab) {
  EmbedderKey key = GetEmbedderKey(type, tab);
  if (active_embedder_key_.has_value() && active_embedder_key_.value() == key) {
    Close(type, tab);
  } else {
    Show(type, tab);
  }
}

std::unique_ptr<views::View> GlicInstanceImpl::CreateViewForSidePanel(
    tabs::TabInterface* tab) {
  if (auto* embedder = GetEmbedderForTab(tab)) {
    return embedder->CreateView();
  }
  return nullptr;
}

GlicUiEmbedder* GlicInstanceImpl::GetEmbedderForTab(tabs::TabInterface* tab) {
  return GetEmbedderForKey(EmbedderKey(tab));
}

GlicUiEmbedder* GlicInstanceImpl::GetEmbedderForKey(EmbedderKey key) {
  auto it = embedders_.find(key);
  if (it != embedders_.end()) {
    return it->second.embedder.get();
  }
  return nullptr;
}

GlicSharingManager& GlicInstanceImpl::sharing_manager() {
  return sharing_manager_;
}

void GlicInstanceImpl::CloseInstanceAndShutdown() {
  NOTIMPLEMENTED();
}
void GlicInstanceImpl::CreateTab() {
  NOTIMPLEMENTED();
}
void GlicInstanceImpl::CreateTask() {
  NOTIMPLEMENTED();
}
void GlicInstanceImpl::PerformActions() {
  NOTIMPLEMENTED();
}
void GlicInstanceImpl::StopActorTask() {
  NOTIMPLEMENTED();
}
void GlicInstanceImpl::PauseActorTask() {
  NOTIMPLEMENTED();
}
void GlicInstanceImpl::ResumeActorTask() {
  NOTIMPLEMENTED();
}
void GlicInstanceImpl::GetZeroStateSuggestionsAndSubscribe() {
  NOTIMPLEMENTED();
}
void GlicInstanceImpl::GetZeroStateSuggestionsForFocusedTab() {
  NOTIMPLEMENTED();
}

void GlicInstanceImpl::DisassociateFromTab(tabs::TabInterface* tab) {
  if (active_embedder_key_.has_value() &&
      active_embedder_key_.value() == EmbedderKey(tab)) {
    DeactivateCurrentEmbedder();
  }
  embedders_.erase(EmbedderKey(tab));
}

bool GlicInstanceImpl::IsOrphaned() const {
  // An instance is orphaned if it has no tab associations. The floating
  // embedder does not count.
  for (const auto& [key, entry] : embedders_) {
    if (std::holds_alternative<tabs::TabInterface*>(key)) {
      return false;
    }
  }
  return true;
}

Host& GlicInstanceImpl::host() {
  return host_;
}

const InstanceId& GlicInstanceImpl::id() const {
  return id_;
}

void GlicInstanceImpl::FetchZeroStateSuggestions(
    bool is_first_run,
    std::optional<std::vector<std::string>> supported_tools,
    mojom::WebClientHandler::GetZeroStateSuggestionsForFocusedTabCallback
        callback) {
  // TODO(crbug.com/444463509): Update this when we have per-instance
  // sharing managers set up without auto-focus.
  auto* active_web_contents =
      sharing_manager().GetFocusedTabData().focus()
          ? sharing_manager().GetFocusedTabData().focus()->GetContents()
          : nullptr;

  contextual_cueing::ContextualCueingService* contextual_cueing_service =
      contextual_cueing::ContextualCueingServiceFactory::GetForProfile(
          profile_);

  if (contextual_cueing_service && active_web_contents && IsShowing()) {
    auto suggestions = mojom::ZeroStateSuggestions::New();
    suggestions->tab_id = GetTabId(active_web_contents);
    suggestions->tab_url = active_web_contents->GetLastCommittedURL();
    contextual_cueing_service
        ->GetContextualGlicZeroStateSuggestionsForFocusedTab(
            active_web_contents, is_first_run, supported_tools,
            mojo::WrapCallbackWithDefaultInvokeIfNotRun(
                base::BindOnce(&GlicInstanceImpl::OnZeroStateSuggestionsFetched,
                               weak_ptr_factory_.GetWeakPtr(),
                               std::move(suggestions), std::move(callback)),
                std::vector<std::string>({})));

  } else {
    std::move(callback).Run(nullptr);
  }
}

void GlicInstanceImpl::OnZeroStateSuggestionsFetched(
    mojom::ZeroStateSuggestionsPtr suggestions,
    mojom::WebClientHandler::GetZeroStateSuggestionsForFocusedTabCallback
        callback,
    std::vector<std::string> returned_suggestions) {
  std::vector<mojom::SuggestionContentPtr> output_suggestions;
  for (const std::string& suggestion_string : returned_suggestions) {
    output_suggestions.push_back(
        mojom::SuggestionContent::New(suggestion_string));
  }
  suggestions->suggestions = std::move(output_suggestions);

  std::move(callback).Run(std::move(suggestions));
}

const std::optional<std::string>& GlicInstanceImpl::conversation_id() const {
  return conversation_id_;
}

void GlicInstanceImpl::set_conversation_id(const std::string& conversation_id) {
  CHECK(!conversation_id_);
  conversation_id_ = conversation_id;
}

GlicInstanceImpl::EmbedderKey GlicInstanceImpl::GetEmbedderKey(
    EmbedderType type,
    tabs::TabInterface* tab) {
  if (type == EmbedderType::kSidePanel) {
    CHECK(tab);
    return tab;
  }
  return FloatingEmbedderKey();
}

GlicUiEmbedder* GlicInstanceImpl::GetActiveEmbedder() {
  if (!active_embedder_key_.has_value()) {
    return nullptr;
  }
  auto it = embedders_.find(active_embedder_key_.value());
  if (it != embedders_.end()) {
    return it->second.embedder.get();
  }
  return nullptr;
}

void GlicInstanceImpl::DeactivateCurrentEmbedder() {
  auto* old_embedder = GetActiveEmbedder();
  if (!old_embedder) {
    active_embedder_key_.reset();
    return;
  }

  auto it = embedders_.find(active_embedder_key_.value());
  CHECK(it != embedders_.end());
  it->second.embedder = old_embedder->CreateInactiveEmbedder();
  active_embedder_key_.reset();
  // Avoids use-after-free bugs. This is a temporary fix until swapping
  // delegates is properly supported (crbug.com/446219126).
  host_.Initialize(it->second.embedder.get()->GetHostDelegate());
}

GlicUiEmbedder* GlicInstanceImpl::CreateActiveEmbedderFor(
    const EmbedderKey& key) {
  EmbedderEntry new_entry;
  std::visit(absl::Overload{
                 [&](FloatingEmbedderKey) {
                   new_entry.embedder = std::make_unique<GlicFloatingUi>();
                 },
                 [&](tabs::TabInterface* tab) {
                   new_entry.embedder = std::make_unique<GlicSidePanelUi>(
                       profile_, tab->GetWeakPtr(), *this);
                   auto* helper = GlicInstanceHelper::From(tab);
                   CHECK(helper);
                   new_entry.destruction_subscription =
                       helper->SubscribeToDestruction(base::BindRepeating(
                           &GlicInstanceImpl::OnAssociatedTabDestroyed,
                           weak_ptr_factory_.GetWeakPtr()));
                 },
             },
             key);

  auto* embedder_ptr = new_entry.embedder.get();
  embedders_.insert_or_assign(key, std::move(new_entry));
  return embedder_ptr;
}

void GlicInstanceImpl::MaybeShowHostUi(GlicUiEmbedder* embedder) {
  Host::Delegate* delegate = embedder->GetHostDelegate();
  if (!delegate) {
    return;
  }

  host_.Initialize(delegate);

  // Create the WebContents if it's not already created.
  host_.CreateContents(/*initially_hidden=*/false);
  host_.NotifyWindowIntentToShow();

  // TODO: NotifyPanelStateChanged() here
  // TODO: pass in the correct invocation source
  // TODO: pass in the conversation id
  host_.PanelWillOpen(mojom::InvocationSource::kTopChromeButton, {});
}

void GlicInstanceImpl::OnAssociatedTabDestroyed(tabs::TabInterface* tab,
                                                const InstanceId& instance_id) {
  DisassociateFromTab(tab);
  if (IsOrphaned() && attachment_delegate_) {
    attachment_delegate_->OnInstanceOrphaned(this);
  }
}

void GlicInstanceImpl::SwitchConversation(
    tabs::TabInterface* tab,
    const std::string& conversation_id,
    mojom::WebClientHandler::SwitchConversationCallback callback) {
  if (attachment_delegate_) {
    attachment_delegate_->SwitchConversation(tab, conversation_id,
                                             std::move(callback));
  } else {
    std::move(callback).Run(mojom::SwitchConversationErrorReason::kUnknown);
  }
}

}  // namespace glic
