// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dictation/session_ui_impl.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/notimplemented.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/dictation/session_ui_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/profile_browser_collection.h"
#include "chrome/browser/ui/tabs/public/tab_dialog_manager.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/views/dictation/dictation_bubble_ui.h"
#include "chrome/browser/ui/views/interaction/browser_elements_views.h"
#include "components/tabs/public/tab_interface.h"

namespace dictation {

namespace {

DictationBubbleUi::State ToBubbleUiState(SessionState state) {
  switch (state) {
    case SessionState::kInactive:
      return DictationBubbleUi::State::kInactive;
    case SessionState::kStreamInitializing:
      return DictationBubbleUi::State::kInitializing;
    case SessionState::kTranscribing:
      return DictationBubbleUi::State::kTranscribing;
    case SessionState::kFinalizing:
      return DictationBubbleUi::State::kFinalizing;
  }
}

}  // namespace

SessionUiImpl::SessionUiImpl(tabs::TabInterface& tab,
                             SessionUiDelegate& delegate)
    : controller_(delegate) {
  BrowserWindowInterface* window = tab.GetBrowserWindowInterface();
  CHECK(window);

  // TODO(b/529143806): This should be anchoring to a Tab/WebContents View.
  views::View* anchor_view =
      BrowserElementsViews::From(window)->GetView(kTopContainerElementId);
  if (!anchor_view) {
    return;
  }

  bubble_ui_ = std::make_unique<DictationBubbleUi>(
      anchor_view,
      base::BindRepeating(&SessionUiImpl::OnDictationBubbleCloseClicked,
                          base::Unretained(this)),
      base::BindRepeating(&SessionUiImpl::OnToggleActiveStreamClicked,
                          base::Unretained(this)));

  session_state_changed_subscription_ =
      delegate.AddSessionStateChangedCallback(base::BindRepeating(
          &SessionUiImpl::OnSessionStateChanged, base::Unretained(this)));

  // TODO(b/510778034): Determine what we need to make this accessibility
  // friendly.

  // TODO(bokan): Handle the case where !tab_dialog_manager()->CanShowDialog().
  // Should we be using a TabDialog? This might be temporary.

  auto params = std::make_unique<tabs::TabDialogManager::Params>();
  params->disable_input = false;
  params->block_new_modal = false;
  params->close_on_detach = false;
  params->get_dialog_bounds = base::BindRepeating(
      &DictationBubbleUi::GetBubbleBounds, base::Unretained(bubble_ui_.get()));

  tab.GetTabFeatures()->tab_dialog_manager()->ShowDialog(
      bubble_ui_->GetWidget(), std::move(params));

  tab_detach_subscription_ = tab.RegisterWillDetach(base::BindRepeating(
      &SessionUiImpl::OnTabWillDetach, base::Unretained(this)));

  tab_insert_subscription_ = tab.RegisterDidInsert(base::BindRepeating(
      &SessionUiImpl::OnTabInserted, base::Unretained(this)));

  tab_will_deactivate_subscription_ =
      tab.RegisterWillDeactivate(base::BindRepeating(
          &SessionUiImpl::OnTabWillDeactivate, base::Unretained(this)));
}

SessionUiImpl::~SessionUiImpl() = default;

void SessionUiImpl::OnSessionStateChanged(SessionState state) {
  bubble_ui_->SetState(ToBubbleUiState(state));
}

void SessionUiImpl::OnDictationBubbleCloseClicked() {
  controller_->UiRequestEndSession();
}

void SessionUiImpl::OnToggleActiveStreamClicked() {
  switch (controller_->GetState()) {
    case SessionState::kStreamInitializing:
    case SessionState::kTranscribing:
      controller_->UiRequestEndActiveStream();
      break;
    case SessionState::kInactive:
      controller_->UiRequestStartStream();
      break;
    case SessionState::kFinalizing:
      // The toggle button should be disabled while finalizing.
      NOTREACHED();
  }
}

void SessionUiImpl::OnTabWillDetach(tabs::TabInterface* tab,
                                    tabs::TabInterface::DetachReason reason) {
  if (reason == tabs::TabInterface::DetachReason::kDelete) {
    controller_->HostTabDidClose();
    // WARNING: Do not add code below, `this` is deleted.
  }
}

void SessionUiImpl::OnTabWillDeactivate(tabs::TabInterface* tab) {
  // Tabs become deactivated briefly while being detached from a window. We
  // don't want to stop the session in that case, only when a new tab in the
  // window is foregrounded. We use PostTask since the detach case always
  // synchronously re-inserts the tab into a new window so we can differentiate
  // these two cases by checking IsActivated asynchronously.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](base::WeakPtr<SessionUiImpl> self,
             base::WeakPtr<tabs::TabInterface> tab_weak) {
            if (self && tab_weak && !tab_weak->IsActivated()) {
              tab_weak->GetTabFeatures()->tab_dialog_manager()->CloseDialog();
              self->controller_->FinalizeAndShutdown();
              // TODO(b/529137727): Show a toast that the dictation
              // was ended.
            }
          },
          weak_ptr_factory_.GetWeakPtr(), tab->GetWeakPtr()));
}

void SessionUiImpl::OnTabInserted(tabs::TabInterface* tab) {
  BrowserWindowInterface* window = tab->GetBrowserWindowInterface();
  if (!window) {
    return;
  }
  // TODO(b/529143806): This would likely be unneeded if the anchor was based on
  // the Tab/WebContents View.
  views::View* new_anchor_view =
      BrowserElementsViews::From(window)->GetView(kTopContainerElementId);
  if (new_anchor_view) {
    bubble_ui_->SetAnchorView(new_anchor_view);
  }
}

}  // namespace dictation
