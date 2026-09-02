// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dictation/session_ui_impl.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/notimplemented.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/dictation/features.h"
#include "chrome/browser/dictation/session_ui_delegate.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/profile_browser_collection.h"
#include "chrome/browser/ui/tabs/public/tab_dialog_manager.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/toasts/api/toast_id.h"
#include "chrome/browser/ui/toasts/toast_controller.h"
#include "chrome/browser/ui/views/dictation/dictation_bubble_ui.h"
#include "chrome/browser/ui/views/dictation/dictation_overlay_view.h"
#include "chrome/browser/ui/views/dictation/ui_state.h"
#include "chrome/browser/ui/views/interaction/browser_elements_views.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"

namespace dictation {

namespace {

UiState ToUiState(SessionState state) {
  switch (state) {
    case SessionState::kInactive:
      return UiState::kInactive;
    case SessionState::kStreamInitializing:
      return UiState::kInitializing;
    case SessionState::kTranscribing:
      return UiState::kTranscribing;
    case SessionState::kFinalizing:
      return UiState::kFinalizing;
  }
}

ToastId GetToastId(StreamErrorReason reason) {
  switch (reason) {
    case StreamErrorReason::kNoMicrophone:
      return ToastId::kDictationNoMicrophoneError;
    case StreamErrorReason::kNone:
    case StreamErrorReason::kUnknown:
      return ToastId::kDictationError;
  }
}

}  // namespace

void SessionUiImpl::CreateBubbleUi() {
  BrowserWindowInterface* window = tab_->GetBrowserWindowInterface();
  if (!window) {
    return;
  }

  // TODO(b/529143806): This should be anchoring to a Tab/WebContents View.
  auto* browser_elements = BrowserElementsViews::From(window);
  views::View* anchor_view =
      browser_elements ? browser_elements->GetView(kTopContainerElementId)
                       : nullptr;
  if (!anchor_view) {
    return;
  }

  bubble_ui_ = std::make_unique<DictationBubbleUi>(
      anchor_view,
      base::BindRepeating(&SessionUiImpl::OnDictationBubbleCloseClicked,
                          base::Unretained(this)),
      base::BindRepeating(&SessionUiImpl::OnToggleActiveStreamClicked,
                          base::Unretained(this)));
  bubble_ui_->SetState(ToUiState(controller_->GetState()));

  // TODO(b/510778034): Determine what we need to make this accessibility
  // friendly.

  // TODO(bokan): Handle the case where !tab_dialog_manager()->CanShowDialog().
  // Should we be using a TabDialog? This might be temporary.

  auto params = std::make_unique<tabs::TabDialogManager::Params>();
  params->disable_input = false;
  params->block_new_modal = false;
  params->get_dialog_bounds = base::BindRepeating(
      &DictationBubbleUi::GetBubbleBounds, base::Unretained(bubble_ui_.get()));

  tab_->GetTabFeatures()->tab_dialog_manager()->ShowDialog(
      bubble_ui_->GetWidget(), std::move(params));
}

SessionUiImpl::SessionUiImpl(tabs::TabInterface& tab,
                             SessionUiDelegate& delegate)
    : tab_(tab), controller_(delegate) {
  CreateBubbleUi();

  session_state_changed_subscription_ =
      delegate.AddSessionStateChangedCallback(base::BindRepeating(
          &SessionUiImpl::OnSessionStateChanged, base::Unretained(this)));

  tab_detach_subscription_ = tab.RegisterWillDetach(base::BindRepeating(
      &SessionUiImpl::OnTabWillDetach, base::Unretained(this)));

  tab_insert_subscription_ = tab.RegisterDidInsert(base::BindRepeating(
      &SessionUiImpl::OnTabInserted, base::Unretained(this)));

  tab_will_deactivate_subscription_ =
      tab.RegisterWillDeactivate(base::BindRepeating(
          &SessionUiImpl::OnTabWillDeactivate, base::Unretained(this)));
}

SessionUiImpl::~SessionUiImpl() = default;

void SessionUiImpl::OnError(StreamType stream_type, StreamErrorReason reason) {
  BrowserWindowInterface* const window = tab_->GetBrowserWindowInterface();
  if (window) {
    ToastController* const toast_controller = ToastController::From(window);
    if (toast_controller) {
      toast_controller->MaybeShowToast(ToastParams(GetToastId(reason)));
    }
  }

  if (stream_type == StreamType::kAttached) {
    // If the attached stream failed, we still want to let any finalizing
    // streams finish before ending the session.
    controller_->FinalizeAndShutdown();
  }
}

void SessionUiImpl::OnStopped() {
  BrowserWindowInterface* const window = tab_->GetBrowserWindowInterface();
  if (window) {
    ToastController* const toast_controller = ToastController::From(window);
    if (toast_controller) {
      toast_controller->MaybeShowToast(ToastParams(ToastId::kDictationStopped));
    }
  }
}

void SessionUiImpl::UpdateAudioLevel(float audio_level) {
  bubble_ui_->UpdateAudioLevel(audio_level);
  if (overlay_view_) {
    overlay_view_->UpdateAudioLevel(audio_level);
  }
}

void SessionUiImpl::OnStartedStream(content::GlobalDOMNodeId target_id) {
  content::RenderFrameHost* target_rfh =
      target_id.document.AsRenderFrameHostIfValid();
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(target_rfh);
  if (!target_rfh || web_contents != tab_->GetContents()) {
    return;
  }

  if (!overlay_view_) {
    gfx::NativeView parent_view = platform_util::GetViewForWindow(
        web_contents->GetTopLevelNativeWindow());
    overlay_view_ = std::make_unique<DictationOverlayView>(
        parent_view,
        base::BindRepeating(&SessionUiImpl::OnToggleActiveStreamClicked,
                            base::Unretained(this)));
    overlay_view_->SetState(ToUiState(controller_->GetState()));
  }

  overlay_view_->OnStartedStream(target_id);
}

void SessionUiImpl::OnSessionStateChanged(SessionState state) {
  UiState ui_state = ToUiState(state);
  bubble_ui_->SetState(ui_state);
  if (overlay_view_) {
    overlay_view_->SetState(ui_state);
  }
}

void SessionUiImpl::OnDictationBubbleCloseClicked() {
  controller_->UiRequestEndSession();
}

void SessionUiImpl::OnToggleActiveStreamClicked() {
  if (kSessionEndsOnStreamEnd.Get()) {
    // This configuration does not start new streams within a session. We just
    // end the session.
    controller_->FinalizeAndShutdown();
    return;
  }

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
    return;
  }

  // Close the UI elements (toast and overlay) without ending the session.
  tab->GetTabFeatures()->tab_dialog_manager()->CloseDialog();
  bubble_ui_.reset();
  overlay_view_.reset();
}

void SessionUiImpl::OnTabInserted(tabs::TabInterface* tab) {
  // Recreate the UI elements for the ongoing session in the new window.
  CreateBubbleUi();
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
              self->overlay_view_.reset();
              self->controller_->FinalizeAndShutdown();
              BrowserWindowInterface* const window =
                  tab_weak->GetBrowserWindowInterface();
              CHECK(window);
              ToastController* const toast_controller =
                  ToastController::From(window);
              CHECK(toast_controller);
              toast_controller->MaybeShowToast(
                  ToastParams(ToastId::kDictationStopped));
            }
          },
          weak_ptr_factory_.GetWeakPtr(), tab->GetWeakPtr()));
}

}  // namespace dictation
