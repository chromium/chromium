// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/widget/glic_co_browse_views.h"

#include <utility>

#include "base/android/jni_android.h"
#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/context_sharing/tab_bottom_sheet/android/co_browse_views_bridge.h"
#include "chrome/browser/glic/android/jni_headers/GlicBottomSheetComponentProvider_jni.h"
#include "chrome/browser/glic/android/jni_headers/GlicSidePanelComponentProvider_jni.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "third_party/jni_zero/jni_zero.h"

namespace glic {

GlicCoBrowseViews::GlicCoBrowseViews(base::WeakPtr<tabs::TabInterface> tab,
                                     Profile* profile,
                                     bool is_bottom_sheet)
    : tab_(tab),
      profile_(CHECK_DEREF(profile)),
      is_bottom_sheet_(is_bottom_sheet) {
  if (tab_) {
    will_detach_subscription_ = tab_->RegisterWillDetach(base::BindRepeating(
        &GlicCoBrowseViews::OnTabWillDetach, weak_ptr_factory_.GetWeakPtr()));
  }
}

GlicCoBrowseViews::~GlicCoBrowseViews() {
  views_bridge_.reset();
  component_provider_.Reset();
}

base::ScopedClosureRunner GlicCoBrowseViews::Configure(Params params) {
  const uint64_t generation = ++generation_;
  close_callback_ = std::move(params.close_callback);
  on_native_fre_accepted_ = std::move(params.native_fre_accepted_callback);
  SetIsLoading(params.is_loading);
  SetWebContents(params.web_contents);
  return base::ScopedClosureRunner(
      base::BindOnce(&GlicCoBrowseViews::ResetIfGenerationMatches,
                     weak_ptr_factory_.GetWeakPtr(), generation));
}

base::android::ScopedJavaLocalRef<jobject>
GlicCoBrowseViews::GetOrCreateJavaCoBrowseViews() {
  EnsureBridgeCreated();
  if (!views_bridge_) {
    return nullptr;
  }
  views_bridge_->SetWebContents(web_contents_.get(), /*request_focus=*/false);
  UpdateJavaNativeFre();
  return views_bridge_->GetCoBrowseViews();
}

void GlicCoBrowseViews::RecreateJavaCoBrowseViews() {
  if (views_bridge_) {
    views_bridge_->CreateCoBrowseViews(web_contents_.get(),
                                       /*request_focus=*/false);
  }
}

void GlicCoBrowseViews::OnPanelClosed() {
  ResetNativeFre();
}

void GlicCoBrowseViews::SetWebContents(content::WebContents* web_contents) {
  web_contents_ = web_contents ? web_contents->GetWeakPtr() : nullptr;
  if (views_bridge_) {
    views_bridge_->SetWebContents(web_contents_.get(), /*request_focus=*/false);
  }
}

void GlicCoBrowseViews::SetIsLoading(bool is_loading) {
  is_loading_ = is_loading;
  UpdateJavaIsLoading();
}

void GlicCoBrowseViews::PrepareForShow(bool show_native_fre) {
  if (is_bottom_sheet_ && show_native_fre) {
    showing_native_fre_ = true;
  }
  UpdateJavaNativeFre();
}

void GlicCoBrowseViews::OnNativeFreAccepted() {
  // Java owns the FRE view teardown after accept, so only clear the state.
  showing_native_fre_ = false;
  UpdateJavaIsLoading();
  if (on_native_fre_accepted_) {
    on_native_fre_accepted_.Run();
  }
}

bool GlicCoBrowseViews::HasJavaWebContentsForTesting() const {
  return views_bridge_ &&
         views_bridge_->HasJavaWebContentsForTesting();  // IN-TEST
}

bool GlicCoBrowseViews::IsShowingNativeFreForTesting() const {
  return ShouldShowNativeFre();
}

bool GlicCoBrowseViews::ShouldShowLoadingPlaceholderForTesting() const {
  return ShouldShowLoadingPlaceholder();
}

void GlicCoBrowseViews::RequestCloseForTesting() {
  OnCloseRequested();
}

void GlicCoBrowseViews::EnsureBridgeCreated() {
  if (views_bridge_) {
    return;
  }
  if (!tab_) {
    return;
  }
  TabAndroid* tab_android = TabAndroid::FromTabInterface(tab_.get());
  if (!tab_android) {
    return;
  }
  JNIEnv* env = base::android::AttachCurrentThread();
  if (!component_provider_) {
    if (is_bottom_sheet_) {
      component_provider_ =
          Java_GlicBottomSheetComponentProvider_createProvider(
              env, profile_->GetJavaObject(), jni_zero::MakeRaw(this));
    } else {
      component_provider_ =
          Java_GlicSidePanelComponentProvider_createProvider(env);
    }
  }
  const auto container_type =
      is_bottom_sheet_ ? context_sharing::CoBrowseContainerType::kBottomSheet
                       : context_sharing::CoBrowseContainerType::kSidePanel;
  views_bridge_ = std::make_unique<context_sharing::CoBrowseViewsBridge>(
      *tab_, context_sharing::TabBottomSheetClientType::kGlic, container_type,
      base::android::ScopedJavaLocalRef<jobject>(env, component_provider_),
      /*enable_pinch_to_zoom=*/false, kColorGlicBackground,
      kColorGlicPlaceholderElem,
      base::BindRepeating(&GlicCoBrowseViews::OnCloseRequested,
                          weak_ptr_factory_.GetWeakPtr()));

  UpdateJavaIsLoading();
  UpdateJavaNativeFre();
}

void GlicCoBrowseViews::ResetIfGenerationMatches(uint64_t generation) {
  if (generation_ != generation) {
    return;
  }
  close_callback_.Reset();
  on_native_fre_accepted_.Reset();
  SetIsLoading(false);
  SetWebContents(nullptr);
}

void GlicCoBrowseViews::OnCloseRequested() {
  if (close_callback_) {
    close_callback_.Run();
  }
}

void GlicCoBrowseViews::OnTabWillDetach(
    tabs::TabInterface* tab,
    tabs::TabInterface::DetachReason detach_reason) {
  if (detach_reason == tabs::TabInterface::DetachReason::kDelete) {
    views_bridge_.reset();
    component_provider_.Reset();
  }
}

bool GlicCoBrowseViews::ShouldShowNativeFre() const {
  return showing_native_fre_ &&
         !GlicEnabling::HasConsentedForProfile(&profile_.get());
}

void GlicCoBrowseViews::UpdateJavaNativeFre() {
  // TODO(b/571546684): Observe FRE completion so an accept in another window
  // hides this FRE.
  if (!ShouldShowNativeFre()) {
    ResetNativeFre();
    return;
  }
  UpdateJavaIsLoading();
  if (!component_provider_ || !is_bottom_sheet_) {
    return;
  }
  Java_GlicBottomSheetComponentProvider_configureNativeFre(
      base::android::AttachCurrentThread(), component_provider_,
      /*showNativeFre=*/true);
}

void GlicCoBrowseViews::ResetNativeFre() {
  if (std::exchange(showing_native_fre_, false) && component_provider_ &&
      is_bottom_sheet_) {
    Java_GlicBottomSheetComponentProvider_configureNativeFre(
        base::android::AttachCurrentThread(), component_provider_,
        /*showNativeFre=*/false);
  }
  UpdateJavaIsLoading();
}

bool GlicCoBrowseViews::ShouldShowLoadingPlaceholder() const {
  // The native FRE takes precedence over the loading placeholder. Once the FRE
  // is accepted or dismissed, the placeholder shows if Glic is still loading.
  return is_loading_ && !ShouldShowNativeFre();
}

void GlicCoBrowseViews::UpdateJavaIsLoading() {
  if (views_bridge_) {
    views_bridge_->SetIsLoading(ShouldShowLoadingPlaceholder());
  }
}

DEFINE_JNI(GlicBottomSheetComponentProvider)

}  // namespace glic
