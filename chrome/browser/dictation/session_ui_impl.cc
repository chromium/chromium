// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dictation/session_ui_impl.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/notimplemented.h"
#include "chrome/browser/dictation/session_ui_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/profile_browser_collection.h"
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
  bubble_ui_->Show();
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

}  // namespace dictation
