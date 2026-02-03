// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/public/widget/glic_side_panel_coordinator_android.h"

#include <climits>

#include "base/functional/bind.h"
#include "base/rand_util.h"
#include "chrome/android/features/tab_ui/jni_headers/TabBottomSheetSimpleManager_jni.h"
#include "chrome/browser/android/tab_android.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"

using base::android::AttachCurrentThread;

namespace glic {

// NEEDS_ANDROID_IMPL: This is only stubbed out with a rudimentary fake for now
// to get tests working. TODO(b/473636242): Implement for bottom sheet and side
// panel.

GlicSidePanelCoordinatorAndroid::GlicSidePanelCoordinatorAndroid(
    tabs::TabInterface* tab)
    : GlicSidePanelCoordinator(tab),
      tab_(*tab),
      // Request ID is temporary. Eventually this will get an object like
      // SidePanelEntry from the bottom sheet manager.
      request_id_(base::RandInt(0, INT_MAX)) {
  did_activate_subscription_ = tab_->RegisterDidActivate(
      base::BindRepeating(&GlicSidePanelCoordinatorAndroid::OnTabDidActivate,
                          base::Unretained(this)));
  will_deactivate_subscription_ = tab_->RegisterWillDeactivate(
      base::BindRepeating(&GlicSidePanelCoordinatorAndroid::OnTabWillDeactivate,
                          base::Unretained(this)));
}

GlicSidePanelCoordinatorAndroid::~GlicSidePanelCoordinatorAndroid() = default;

void GlicSidePanelCoordinatorAndroid::Show(bool suppress_animations) {
  if (IsShowing()) {
    return;
  }

  TabAndroid* tab_android = GetTabAndroid();

  if (!tab_->IsActivated()) {
    SetState(State::kBackgrounded);
    return;
  }
  Java_TabBottomSheetSimpleManager_show(
      AttachCurrentThread(), tab_android->GetJavaObject(), request_id_);
  SetState(State::kShown);
}

void GlicSidePanelCoordinatorAndroid::SetWebContents(
    content::WebContents* web_contents) {
  TabAndroid* tab_android = GetTabAndroid();
  Java_TabBottomSheetSimpleManager_setWebContents(
      AttachCurrentThread(), tab_android->GetJavaObject(),
      web_contents ? web_contents->GetJavaWebContents() : nullptr);
}

void GlicSidePanelCoordinatorAndroid::Close(const CloseOptions& options) {
  if (state_ == State::kClosed) {
    return;
  }

  TabAndroid* tab_android = GetTabAndroid();

  Java_TabBottomSheetSimpleManager_close(
      AttachCurrentThread(), tab_android->GetJavaObject(), request_id_);
  SetState(State::kClosed);
}

bool GlicSidePanelCoordinatorAndroid::IsShowing() const {
  return state_ == State::kShown;
}

GlicSidePanelCoordinator::State GlicSidePanelCoordinatorAndroid::state() {
  return state_;
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

  // If we are not closed (e.g. backgrounded), show the panel.
  Show(/*suppress_animations=*/true);
}

void GlicSidePanelCoordinatorAndroid::OnTabWillDeactivate(
    tabs::TabInterface* tab) {
  if (state_ == State::kClosed) {
    return;
  }
  SetState(State::kBackgrounded);
}

TabAndroid* GlicSidePanelCoordinatorAndroid::GetTabAndroid() const {
  return TabAndroid::FromTabHandle(tab_->GetHandle());
}

}  // namespace glic
