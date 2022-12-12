// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/customtabs/tab_interaction_recorder_android.h"

#include <memory>

#include "base/android/jni_android.h"
#include "base/bind.h"
#include "base/memory/raw_ptr.h"
#include "chrome/android/chrome_jni_headers/TabInteractionRecorder_jni.h"
#include "chrome/browser/android/customtabs/custom_tab_session_state_tracker.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/core/browser/autofill_manager.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"

namespace customtabs {

using autofill::AutofillManager;
using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;
using content::RenderFrameHost;

namespace {

AutofillManager* GetAutofillManager(RenderFrameHost* render_frame_host) {
  auto* autofill_driver =
      autofill::ContentAutofillDriver::GetForRenderFrameHost(render_frame_host);
  if (!autofill_driver)
    return nullptr;
  return autofill_driver->autofill_manager();
}

}  // namespace

AutofillObserverImpl::AutofillObserverImpl(
    autofill::AutofillManager* autofill_manager,
    OnFormInteractionCallback form_interaction_callback)
    : autofill_manager_(autofill_manager),
      form_interaction_callback_(std::move(form_interaction_callback)) {
  autofill_manager->AddObserver(this);
}

AutofillObserverImpl::~AutofillObserverImpl() {
  Invalidate();
}

void AutofillObserverImpl::OnFormSubmitted() {
  OnFormInteraction();
}

void AutofillObserverImpl::OnSelectControlDidChange() {
  OnFormInteraction();
}

void AutofillObserverImpl::OnTextFieldDidChange() {
  OnFormInteraction();
}

void AutofillObserverImpl::OnTextFieldDidScroll() {
  OnFormInteraction();
}

void AutofillObserverImpl::Invalidate() {
  if (IsInObserverList()) {
    DCHECK(autofill_manager_);
    autofill_manager_->RemoveObserver(this);
    autofill_manager_ = nullptr;
  }
}

void AutofillObserverImpl::OnFormInteraction() {
  Invalidate();
  std::move(form_interaction_callback_).Run();
}

TabInteractionRecorderAndroid::~TabInteractionRecorderAndroid() = default;

TabInteractionRecorderAndroid::TabInteractionRecorderAndroid(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<TabInteractionRecorderAndroid>(
          *web_contents) {}

bool TabInteractionRecorderAndroid::HasNavigatedFromFirstPage() const {
  return web_contents()->GetController().CanGoBack() ||
         web_contents()->GetController().CanGoForward();
}

// content::WebContentsObserver:
void TabInteractionRecorderAndroid::RenderFrameHostStateChanged(
    RenderFrameHost* render_frame_host,
    RenderFrameHost::LifecycleState old_state,
    RenderFrameHost::LifecycleState new_state) {
  if (old_state == RenderFrameHost::LifecycleState::kActive) {
    rfh_observer_map_.erase(render_frame_host->GetGlobalId());
  } else if (new_state == RenderFrameHost::LifecycleState::kActive &&
             !has_form_interactions_) {
    StartObservingFrame(render_frame_host);
  }
}

void TabInteractionRecorderAndroid::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (has_form_interactions_)
    return;
  if (!navigation_handle->IsSameDocument() &&
      navigation_handle->HasCommitted() &&
      navigation_handle->GetRenderFrameHost()->IsActive())
    StartObservingFrame(navigation_handle->GetRenderFrameHost());
}

void TabInteractionRecorderAndroid::DidGetUserInteraction(
    const blink::WebInputEvent& event) {
  // We already reported the interaction for this Tab and the lifetime of
  // this object is the same as the CCT, so we will only report once.
  if (did_get_user_interaction_)
    return;

  did_get_user_interaction_ = true;
  chrome::android::CustomTabSessionStateTracker::GetInstance()
      .OnUserInteraction();
}

void TabInteractionRecorderAndroid::SetHasFormInteractions() {
  has_form_interactions_ = true;
  rfh_observer_map_.clear();
}

void TabInteractionRecorderAndroid::StartObservingFrame(
    RenderFrameHost* render_frame_host) {
  // Do not observe the same frame more than once.
  if (rfh_observer_map_[render_frame_host->GetGlobalId()])
    return;

  AutofillManager* autofill_manager =
      test_autofill_manager_ ? test_autofill_manager_.get()
                             : GetAutofillManager(render_frame_host);
  if (!autofill_manager)
    return;

  rfh_observer_map_[render_frame_host->GetGlobalId()] =
      std::make_unique<AutofillObserverImpl>(
          autofill_manager,
          base::BindOnce(&TabInteractionRecorderAndroid::SetHasFormInteractions,
                         weak_factory_.GetWeakPtr()));
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(TabInteractionRecorderAndroid);

// JNI methods
jboolean TabInteractionRecorderAndroid::DidGetUserInteraction(
    JNIEnv* env) const {
  return did_get_user_interaction_;
}

jboolean TabInteractionRecorderAndroid::HadInteraction(JNIEnv* env) const {
  bool has_interaction = has_form_interactions() || HasNavigatedFromFirstPage();
  return static_cast<jboolean>(has_interaction);
}

void TabInteractionRecorderAndroid::Reset(JNIEnv* env) {
  has_form_interactions_ = false;
}

ScopedJavaLocalRef<jobject> JNI_TabInteractionRecorder_GetFromTab(
    JNIEnv* env,
    const JavaParamRef<jobject>& jtab) {
  TabAndroid* tab = TabAndroid::GetNativeTab(env, jtab);
  if (!tab || !tab->web_contents() || tab->web_contents()->IsBeingDestroyed()) {
    return ScopedJavaLocalRef<jobject>(env, nullptr);
  }

  auto* recorder =
      TabInteractionRecorderAndroid::FromWebContents(tab->web_contents());
  return Java_TabInteractionRecorder_create(
      env, reinterpret_cast<int64_t>(recorder));
}

ScopedJavaLocalRef<jobject> JNI_TabInteractionRecorder_CreateForTab(
    JNIEnv* env,
    const JavaParamRef<jobject>& jtab) {
  TabAndroid* tab = TabAndroid::GetNativeTab(env, jtab);
  if (!tab || !tab->web_contents() || tab->web_contents()->IsBeingDestroyed()) {
    return ScopedJavaLocalRef<jobject>(env, nullptr);
  }

  TabInteractionRecorderAndroid::CreateForWebContents(tab->web_contents());

  auto* recorder =
      TabInteractionRecorderAndroid::FromWebContents(tab->web_contents());
  return Java_TabInteractionRecorder_create(
      env, reinterpret_cast<int64_t>(recorder));
}

}  // namespace customtabs
