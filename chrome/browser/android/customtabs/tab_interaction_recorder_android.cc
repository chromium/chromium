// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/customtabs/tab_interaction_recorder_android.h"

#include <memory>
#include <string>

#include "base/android/jni_android.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/android/customtabs/custom_tab_session_state_tracker.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/core/browser/autofill_manager.h"
#include "components/autofill/core/common/unique_ids.h"
#include "content/public/browser/document_user_data.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/TabInteractionRecorder_jni.h"

namespace customtabs {

using autofill::AutofillManager;
using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;
using content::GlobalRenderFrameHostId;
using content::RenderFrameHost;

namespace {

AutofillManager* GetAutofillManager(RenderFrameHost* render_frame_host) {
  auto* autofill_driver =
      autofill::ContentAutofillDriver::GetForRenderFrameHost(render_frame_host);
  if (!autofill_driver)
    return nullptr;
  return &autofill_driver->GetAutofillManager();
}
}  // namespace

AutofillObserverImpl::AutofillObserverImpl(
    GlobalRenderFrameHostId id,
    autofill::AutofillManager* autofill_manager,
    OnFormInteractionCallback form_interaction_callback)
    : global_id_(id),
      autofill_manager_(autofill_manager),
      form_interaction_callback_(std::move(form_interaction_callback)) {
  autofill_manager->AddObserver(this);
}

AutofillObserverImpl::~AutofillObserverImpl() {
  Invalidate();
}

void AutofillObserverImpl::OnFormSubmitted(autofill::AutofillManager&,
                                           const autofill::FormData&) {
  OnFormInteraction();
}

void AutofillObserverImpl::OnAfterSelectControlDidChange(
    autofill::AutofillManager&,
    autofill::FormGlobalId,
    autofill::FieldGlobalId) {
  OnFormInteraction();
}

void AutofillObserverImpl::OnAfterTextFieldDidChange(autofill::AutofillManager&,
                                                     autofill::FormGlobalId,
                                                     autofill::FieldGlobalId,
                                                     const std::u16string&) {
  OnFormInteraction();
}

void AutofillObserverImpl::OnAfterTextFieldDidScroll(autofill::AutofillManager&,
                                                     autofill::FormGlobalId,
                                                     autofill::FieldGlobalId) {
  OnFormInteraction();
}

void AutofillObserverImpl::OnAfterFormsSeen(
    AutofillManager& manager,
    base::span<const autofill::FormGlobalId> updated_forms,
    base::span<const autofill::FormGlobalId> removed_forms) {
  RenderFrameHost* rfh = RenderFrameHost::FromID(global_id_);
  // Only mark has form associated data for the RFH observed. This is to
  // metigate the fact that AutofillManager will dispatch |OnAfterFormsSeen| to
  // *all* observers regardless where the RFH the observer lives in has a form.
  if (!rfh) {
    return;
  }

  // Check whether the form seen lives in the observed RFH.
  bool forms_in_rfh = false;
  for (auto form_id : updated_forms) {
    if (form_id.frame_token ==
        autofill::LocalFrameToken(rfh->GetFrameToken().value())) {
      forms_in_rfh = true;
      break;
    }
  }
  if (!forms_in_rfh) {
    return;
  }

  // Set the had form data associated value in the page's
  // BackForwardCacheMetrics. Note that this means that if |rfh| is a subframe
  // it's possible that the page will still be marked as having a form data
  // associated with it even though |rfh| had navigated to another document and
  // there is no longer any document with forms in the page.
  content::BackForwardCache::SetHadFormDataAssociated(rfh->GetPage());

  // Create the form interaction user data meaning the page has seen a form
  // attached.
  FormInteractionData::GetOrCreateForCurrentDocument(
      rfh->GetOutermostMainFrame());
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
  std::move(form_interaction_callback_).Run(global_id_);
}

DOCUMENT_USER_DATA_KEY_IMPL(FormInteractionData);

FormInteractionData::~FormInteractionData() = default;

FormInteractionData::FormInteractionData(RenderFrameHost* rfh)
    : DocumentUserData<FormInteractionData>(rfh) {}

void FormInteractionData::SetHasFormInteractionData() {
  had_form_interaction_data_ = true;
}

bool FormInteractionData::GetHasFormInteractionData() {
  return had_form_interaction_data_;
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

bool TabInteractionRecorderAndroid::HasActiveFormInteraction() const {
  bool interaction = false;
  web_contents()->GetPrimaryMainFrame()->ForEachRenderFrameHostWithAction(
      [&interaction](RenderFrameHost* rfh) {
        if (!FormInteractionData::GetForCurrentDocument(rfh)) {
          return RenderFrameHost::FrameIterationAction::kContinue;
        }
        if (FormInteractionData::GetForCurrentDocument(rfh)
                ->GetHasFormInteractionData()) {
          interaction = true;
          return RenderFrameHost::FrameIterationAction::kStop;
        }
        return RenderFrameHost::FrameIterationAction::kContinue;
      });
  return interaction;
}

void TabInteractionRecorderAndroid::ResetImpl() {
  has_form_interactions_in_session_ = false;
  did_get_user_interaction_ = false;
  web_contents()->GetPrimaryMainFrame()->ForEachRenderFrameHost(
      [](RenderFrameHost* rfh) {
        if (FormInteractionData::GetForCurrentDocument(rfh)) {
          FormInteractionData::DeleteForCurrentDocument(rfh);
        }
      });
}

// content::WebContentsObserver:
void TabInteractionRecorderAndroid::RenderFrameHostStateChanged(
    RenderFrameHost* render_frame_host,
    RenderFrameHost::LifecycleState old_state,
    RenderFrameHost::LifecycleState new_state) {
  if (old_state == RenderFrameHost::LifecycleState::kActive) {
    rfh_observer_map_.erase(render_frame_host->GetGlobalId());
  } else if (new_state == RenderFrameHost::LifecycleState::kActive) {
    StartObservingFrame(render_frame_host);
  }
}

void TabInteractionRecorderAndroid::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
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

void TabInteractionRecorderAndroid::SetHasFormInteractions(
    GlobalRenderFrameHostId id) {
  if (RenderFrameHost::FromID(id)) {
    FormInteractionData::GetOrCreateForCurrentDocument(
        RenderFrameHost::FromID(id))
        ->SetHasFormInteractionData();
  }

  has_form_interactions_in_session_ = true;
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
          render_frame_host->GetGlobalId(), autofill_manager,
          base::BindOnce(&TabInteractionRecorderAndroid::SetHasFormInteractions,
                         weak_factory_.GetWeakPtr()));
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(TabInteractionRecorderAndroid);

// JNI methods
jboolean TabInteractionRecorderAndroid::DidGetUserInteraction(
    JNIEnv* env) const {
  return did_get_user_interaction_;
}

jboolean TabInteractionRecorderAndroid::HadFormInteractionInSession(
    JNIEnv* env) const {
  return has_form_interactions_in_session();
}

jboolean TabInteractionRecorderAndroid::HadNavigationInteraction(
    JNIEnv* env) const {
  return did_get_user_interaction_ && HasNavigatedFromFirstPage();
}

jboolean TabInteractionRecorderAndroid::HadFormInteractionInActivePage(
    JNIEnv* env) const {
  return HasActiveFormInteraction();
}

void TabInteractionRecorderAndroid::Reset(JNIEnv* env) {
  ResetImpl();
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
