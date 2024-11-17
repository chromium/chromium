// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/bottombar/overlay_panel_content.h"

#include <memory>
#include <set>

#include "base/android/jni_string.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "cc/input/browser_controls_offset_tags_info.h"
#include "cc/input/browser_controls_state.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "components/embedder_support/android/delegate/web_contents_delegate_android.h"
#include "components/navigation_interception/intercept_navigation_delegate.h"
#include "components/variations/variations_associated_data.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "ui/android/view_android.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/OverlayPanelContent_jni.h"

using base::android::JavaParamRef;


// This class manages the native behavior of the panel.
// Instances of this class are owned by the Java OverlayPanelContentl.
OverlayPanelContent::OverlayPanelContent(JNIEnv* env, jobject obj) {
  java_manager_.Reset(env, obj);
}

OverlayPanelContent::~OverlayPanelContent() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_OverlayPanelContent_clearNativePanelContentPtr(env, java_manager_);
}

void OverlayPanelContent::Destroy(JNIEnv* env) {
  delete this;
}

void OverlayPanelContent::OnPhysicalBackingSizeChanged(
    JNIEnv* env,
    content::WebContents* web_contents,
    jint width,
    jint height) {
  gfx::Size size(width, height);
  web_contents->GetNativeView()->OnPhysicalBackingSizeChanged(size);
  web_contents->GetNativeView()->OnSizeChanged(width, height);
}

void OverlayPanelContent::SetWebContents(
    JNIEnv* env,
    content::WebContents* web_contents,
    const JavaParamRef<jobject>& jweb_contents_delegate) {
  // NOTE(pedrosimonetti): Takes ownership of the WebContents. This is to make
  // sure that the WebContens and the Compositor are in the same process.
  // TODO(pedrosimonetti): Confirm with dtrainor@ if the comment above
  // is accurate.
  web_contents_.reset(web_contents);
  web_contents_->SetOwnerLocationForDebug(FROM_HERE);

  web_contents_->SetIsOverlayContent(true);
  // TODO(pedrosimonetti): confirm if we need this after promoting it
  // to a real tab.
  TabAndroid::AttachTabHelpers(web_contents_.get());
  web_contents_delegate_ = std::make_unique<
      web_contents_delegate_android::WebContentsDelegateAndroid>(
      env, jweb_contents_delegate);
  web_contents_->SetDelegate(web_contents_delegate_.get());
}

void OverlayPanelContent::DestroyWebContents(JNIEnv* env) {
  DCHECK(web_contents_.get());
  // At the time this is called we may be deeply nested in a callback from
  // WebContents. WebContents does not support being deleted from a callback
  // (crashes). To avoid this problem DeleteSoon() is used. See
  // https://crbug.com/1262098.
  base::SingleThreadTaskRunner::GetCurrentDefault()->DeleteSoon(
      FROM_HERE, web_contents_.release());
  // |web_contents_delegate_| may already be NULL at this point.
  web_contents_delegate_.reset();
}

void OverlayPanelContent::SetInterceptNavigationDelegate(
    JNIEnv* env,
    const JavaParamRef<jobject>& delegate,
    content::WebContents* web_contents) {
  navigation_interception::InterceptNavigationDelegate::Associate(
      web_contents,
      std::make_unique<navigation_interception::InterceptNavigationDelegate>(
          env, delegate));
}

void OverlayPanelContent::UpdateBrowserControlsState(
    JNIEnv* env,
    jboolean are_controls_hidden) {
  if (!web_contents_)
    return;

  cc::BrowserControlsState state = cc::BrowserControlsState::kShown;
  if (are_controls_hidden)
    state = cc::BrowserControlsState::kHidden;

  web_contents_->UpdateBrowserControlsState(
      state, cc::BrowserControlsState::kBoth, false, std::nullopt);
}

jlong JNI_OverlayPanelContent_Init(JNIEnv* env,
                                   const JavaParamRef<jobject>& obj) {
  OverlayPanelContent* content = new OverlayPanelContent(env, obj);
  return reinterpret_cast<intptr_t>(content);
}
