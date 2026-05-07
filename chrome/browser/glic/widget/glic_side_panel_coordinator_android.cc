// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/public/widget/glic_side_panel_coordinator_android.h"

#include <climits>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/rand_util.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/context_sharing/tab_bottom_sheet/android/tab_bottom_sheet_bridge.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"

namespace glic {

GlicSidePanelCoordinatorAndroid::GlicSidePanelCoordinatorAndroid(
    tabs::TabInterface* tab)
    : GlicSidePanelCoordinator(tab), tab_(*tab) {
  did_activate_subscription_ = tab_->RegisterDidActivate(
      base::BindRepeating(&GlicSidePanelCoordinatorAndroid::OnTabDidActivate,
                          base::Unretained(this)));
  will_deactivate_subscription_ = tab_->RegisterWillDeactivate(
      base::BindRepeating(&GlicSidePanelCoordinatorAndroid::OnTabWillDeactivate,
                          base::Unretained(this)));
  will_detach_subscription_ = tab_->RegisterWillDetach(
      base::BindRepeating(&GlicSidePanelCoordinatorAndroid::OnTabWillDetach,
                          base::Unretained(this)));

  bridge_ = std::make_unique<context_sharing::TabBottomSheetBridge>(
      this, tab, context_sharing::TabBottomSheetClientType::kGlic);
}

GlicSidePanelCoordinatorAndroid::~GlicSidePanelCoordinatorAndroid() = default;

void GlicSidePanelCoordinatorAndroid::Show(const ShowOptions& options) {
  if (state_ == State::kShown) {
    return;
  }

  if (!tab_->IsActivated()) {
    SetState(State::kBackgrounded);
    return;
  }

  bool shown = bridge_->Show(
      /*animate=*/!options.suppress_animations,
      /*starts_expanded=*/options.initial_state ==
          ShowOptions::InitialState::kExpanded);
  if (shown) {
    if (options.initial_state == ShowOptions::InitialState::kExpanded) {
      SetState(State::kShown);
    } else {
      SetState(State::kPeek);
    }
  } else {
    // If the sheet failed to show (e.g. due to being suppressed by a
    // TokenHolder, or placed in a queue behind a higher priority sheet), the
    // Java layer will NOT fire the onBottomSheetClosed callback. We must
    // immediately transition to closed so we don't leak state and deadlock
    // future Close() calls.
    SetState(State::kClosed);
  }
}

void GlicSidePanelCoordinatorAndroid::SetWebContents(
    content::WebContents* web_contents) {
  if (web_contents) {
    web_contents_ = web_contents->GetWeakPtr();
  } else {
    web_contents_.reset();
  }
  bridge_->SetWebContents(web_contents);
}

void GlicSidePanelCoordinatorAndroid::Close(const CloseOptions& options) {
  if (state_ == State::kClosed) {
    return;
  }

  if (state_ == State::kBackgrounded) {
    SetState(State::kClosed);
    return;
  }

  bridge_->Close(/* animate= */ !options.suppress_animations);
}

bool GlicSidePanelCoordinatorAndroid::IsShowing() const {
  return state_ == State::kShown;
}

GlicSidePanelCoordinator::State GlicSidePanelCoordinatorAndroid::state() {
  return state_;
}

bool GlicSidePanelCoordinatorAndroid::SupportsPeek() const {
  return true;
}

base::CallbackListSubscription
GlicSidePanelCoordinatorAndroid::AddStateCallback(
    base::RepeatingCallback<void(State state)> callback) {
  return state_callbacks_.Add(std::move(callback));
}

int GlicSidePanelCoordinatorAndroid::GetPreferredWidth() {
  return 0;
}

bool GlicSidePanelCoordinatorAndroid::IsGlicSidePanelActive() {
  return state_ != State::kClosed;
}

void GlicSidePanelCoordinatorAndroid::SetState(State state) {
  if (state_ == state) {
    return;
  }
  state_ = state;
  state_callbacks_.Notify(state_);
}

void GlicSidePanelCoordinatorAndroid::OnTabDidActivate(
    tabs::TabInterface* tab) {
  if (state_ == State::kClosed) {
    return;
  }

  ShowOptions options;
  options.suppress_animations = true;
  options.initial_state = ShowOptions::InitialState::kPeeked;
  Show(options);
}

void GlicSidePanelCoordinatorAndroid::OnTabWillDeactivate(
    tabs::TabInterface* tab) {
  if (state_ == State::kClosed) {
    return;
  }
  SetState(State::kBackgrounded);

  bridge_->Close(/* animate= */ false);
}

void GlicSidePanelCoordinatorAndroid::OnTabWillDetach(
    tabs::TabInterface* tab,
    tabs::TabInterface::DetachReason detach_reason) {
  // If the tab was deleted, set the state to backgrounded in case the
  // deletion is undone.
  // This can happen if the user closes the tab in the tab switcher, causing the
  // bottom sheet to appear for the next active tab.
  if (detach_reason == tabs::TabInterface::DetachReason::kDelete) {
    if (state_ != State::kClosed) {
      SetState(State::kBackgrounded);
      bridge_->Close(/* animate= */ false);
    }
  }
}

void GlicSidePanelCoordinatorAndroid::OnClosed() {
  if (state_ == State::kBackgrounded) {
    return;
  }
  SetState(State::kClosed);
}

void GlicSidePanelCoordinatorAndroid::OnSuppressed() {}

void GlicSidePanelCoordinatorAndroid::OnOpened(bool is_expanded) {
  SetState(is_expanded ? State::kShown : State::kPeek);
}

}  // namespace glic
