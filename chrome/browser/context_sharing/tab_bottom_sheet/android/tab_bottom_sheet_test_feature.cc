// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/context_sharing/tab_bottom_sheet/android/tab_bottom_sheet_test_feature.h"

#include "base/android/jni_android.h"
#include "chrome/browser/context_sharing/tab_bottom_sheet/android/co_browse_container_type.h"
#include "chrome/browser/context_sharing/tab_bottom_sheet/android/co_browse_views_bridge.h"
#include "chrome/browser/context_sharing/tab_bottom_sheet/android/tab_bottom_sheet_bridge.h"
#include "chrome/browser/context_sharing/tab_bottom_sheet/android/tab_bottom_sheet_client_type.h"
#include "chrome/browser/context_sharing/tab_bottom_sheet/android/test_jni_headers/TestTabBottomSheetComponentProvider_jni.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"

namespace context_sharing {

TabBottomSheetTestFeature::TabBottomSheetTestFeature(tabs::TabInterface* tab)
    : tab_(*tab) {
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jobject> provider =
      Java_TestTabBottomSheetComponentProvider_Constructor(env);
  views_bridge_ = std::make_unique<CoBrowseViewsBridge>(
      *tab, TabBottomSheetClientType::kUnknown,
      CoBrowseContainerType::kBottomSheet, provider);
  tab_bottom_sheet_bridge_ = std::make_unique<TabBottomSheetBridge>(this, tab);
}

TabBottomSheetTestFeature::~TabBottomSheetTestFeature() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

bool TabBottomSheetTestFeature::Show(bool animate, bool starts_expanded) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return tab_bottom_sheet_bridge_->Show(views_bridge_->GetCoBrowseViews(),
                                        animate, starts_expanded);
}

void TabBottomSheetTestFeature::Close(bool animate) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  tab_bottom_sheet_bridge_->Close(animate);
}

void TabBottomSheetTestFeature::SetWebContents(
    content::WebContents* web_contents) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  views_bridge_->SetWebContents(web_contents, /*request_focus=*/true);
}

void TabBottomSheetTestFeature::OnClosed() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  was_closed_ = true;
  if (on_closed_callback_) {
    on_closed_callback_.Run();
  }
}

void TabBottomSheetTestFeature::OnSuppressed() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  was_suppressed_ = true;
}

void TabBottomSheetTestFeature::OnOpened(bool is_expanded) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  was_opened_ = true;
  is_expanded_ = is_expanded;
  if (on_opened_callback_) {
    on_opened_callback_.Run();
  }
}

}  // namespace context_sharing
