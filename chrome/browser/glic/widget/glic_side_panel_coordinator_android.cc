// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/public/widget/glic_side_panel_coordinator_android.h"

#include <climits>

#include "base/functional/bind.h"
#include "base/rand_util.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/context_sharing/tab_bottom_sheet/android/jni_headers/CoBrowseViewFactory_jni.h"
#include "chrome/browser/context_sharing/tab_bottom_sheet/android/jni_headers/TabBottomSheetNativeInterface_jni.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "ui/android/window_android.h"

using base::android::AttachCurrentThread;

namespace glic {

DEFINE_JNI(TabBottomSheetNativeInterface)

GlicSidePanelCoordinatorAndroid::GlicSidePanelCoordinatorAndroid(
    tabs::TabInterface* tab)
    : GlicSidePanelCoordinator(tab), tab_(*tab) {
  did_activate_subscription_ = tab_->RegisterDidActivate(
      base::BindRepeating(&GlicSidePanelCoordinatorAndroid::OnTabDidActivate,
                          base::Unretained(this)));
  will_deactivate_subscription_ = tab_->RegisterWillDeactivate(
      base::BindRepeating(&GlicSidePanelCoordinatorAndroid::OnTabWillDeactivate,
                          base::Unretained(this)));

  JNIEnv* env = AttachCurrentThread();
  java_interface_.Reset(Java_TabBottomSheetNativeInterface_Constructor(
      env, reinterpret_cast<intptr_t>(this), GetTabAndroid()->GetJavaObject()));
}

GlicSidePanelCoordinatorAndroid::~GlicSidePanelCoordinatorAndroid() {
  Java_TabBottomSheetNativeInterface_destroy(AttachCurrentThread(),
                                             java_interface_);
}

void GlicSidePanelCoordinatorAndroid::Show(bool suppress_animations) {
  if (IsShowing()) {
    return;
  }

  if (!tab_->IsActivated()) {
    SetState(State::kBackgrounded);
    return;
  }
  Java_TabBottomSheetNativeInterface_show(AttachCurrentThread(),
                                          java_interface_, co_browse_views_);
  SetState(State::kShown);
}

void GlicSidePanelCoordinatorAndroid::SetWebContents(
    content::WebContents* web_contents) {
  if (!web_contents) {
    last_web_contents_.reset();
    co_browse_views_.Reset();
    return;
  }
  TabAndroid* tab_android = GetTabAndroid();
  if (!tab_android) {
    return;
  }
  ui::WindowAndroid* window_android =
      tab_android->GetContents()->GetTopLevelNativeWindow();
  if (!window_android) {
    return;
  }

  if (last_web_contents_ && last_web_contents_.get() == web_contents) {
    // The web contents didn't change, so we don't need to do anything.
    return;
  }

  last_web_contents_ = web_contents->GetWeakPtr();
  JNIEnv* env = base::android::AttachCurrentThread();
  // Call Factory to get CoBrowseViews and save it
  co_browse_views_.Reset(Java_CoBrowseViewFactory_getCoBrowseViews(
      env, window_android->GetJavaObject(),
      web_contents->GetJavaWebContents()));
}

void GlicSidePanelCoordinatorAndroid::Close(const CloseOptions& options) {
  if (state_ == State::kClosed) {
    return;
  }

  Java_TabBottomSheetNativeInterface_close(AttachCurrentThread(),
                                           java_interface_);
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

void GlicSidePanelCoordinatorAndroid::OnClose(JNIEnv* env) {
  SetState(State::kClosed);
}

TabAndroid* GlicSidePanelCoordinatorAndroid::GetTabAndroid() const {
  return TabAndroid::FromTabHandle(tab_->GetHandle());
}

}  // namespace glic
