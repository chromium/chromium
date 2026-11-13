// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/browser_ui/glic_modal_android.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/android/jni_android.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/profiles/profile.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "third_party/jni_zero/jni_zero.h"
#include "ui/android/window_android.h"

// JNI headers must be included after standard headers.
#include "chrome/browser/glic/android/jni_headers/GlicModalCoordinator_jni.h"

namespace glic {

GlicModalAndroid::GlicModalAndroid() = default;

GlicModalAndroid::~GlicModalAndroid() {
  Observe(nullptr);
  tab_will_detach_subscription_ = {};
  tab_will_become_hidden_subscription_ = {};
  on_closed_.Reset();
  if (java_coordinator_) {
    JNIEnv* env = base::android::AttachCurrentThread();
    Java_GlicModalCoordinator_onNativeDestroyed(env, java_coordinator_);
    java_coordinator_.Reset();
  }
  if (modal_web_contents_) {
    modal_web_contents_->SetDelegate(nullptr);
    base::SequencedTaskRunner::GetCurrentDefault()->DeleteSoon(
        FROM_HERE, std::move(modal_web_contents_));
  }
}

bool GlicModalAndroid::ShowInternal(tabs::TabInterface& tab,
                                    Profile* profile,
                                    ui::WindowAndroid* window_android,
                                    Params params) {
  CHECK(!java_coordinator_);
  tab_handle_ = tab.GetHandle();
  min_size_dip_ = params.min_size;
  max_size_dip_ = params.portrait_max_size_dp;
  size_to_content_ = params.size_to_content;
  on_available_size_changed_ = std::move(params.on_available_size_changed);

  modal_web_contents_ =
      content::WebContents::Create(content::WebContents::CreateParams(profile));
  modal_web_contents_->SetDelegate(this);
  Observe(modal_web_contents_.get());

  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jobject> java_obj =
      Java_GlicModalCoordinator_show(
          env, jni_zero::MakeRaw(this), window_android,
          modal_web_contents_.get(), params.portrait_max_size_dp.width(),
          params.portrait_max_size_dp.height(),
          params.landscape_max_size_dp.width(),
          params.landscape_max_size_dp.height(),
          params.show_android_close_button, params.loading_view,
          params.size_to_content);
  if (!java_obj) {
    Observe(nullptr);
    modal_web_contents_->SetDelegate(nullptr);
    modal_web_contents_.reset();
    return false;
  }

  modal_web_contents_->GetController().LoadURLWithParams(
      content::NavigationController::LoadURLParams(params.content_url));

  if (params.on_webui_created) {
    std::move(params.on_webui_created).Run(modal_web_contents_.get());
  }

  on_closed_ = std::move(params.on_closed);
  java_coordinator_.Reset(java_obj);
  tab_will_detach_subscription_ = tab.RegisterWillDetach(base::BindRepeating(
      &GlicModalAndroid::OnTabWillDetach, base::Unretained(this)));
  tab_will_become_hidden_subscription_ =
      tab.RegisterWillBecomeHidden(base::BindRepeating(
          &GlicModalAndroid::OnTabWillBecomeHidden, base::Unretained(this)));
  return true;
}

void GlicModalAndroid::Close() {
  if (!java_coordinator_) {
    return;
  }

  JNIEnv* env = base::android::AttachCurrentThread();
  Java_GlicModalCoordinator_dismiss(env, java_coordinator_);
}

void GlicModalAndroid::Activate() {}

void GlicModalAndroid::ShowContent() {
  if (!java_coordinator_) {
    return;
  }

  JNIEnv* env = base::android::AttachCurrentThread();
  Java_GlicModalCoordinator_showContent(env, java_coordinator_);
}

content::WebContents* GlicModalAndroid::GetWebContents() const {
  return modal_web_contents_.get();
}

void GlicModalAndroid::SimulateDismissingForTesting() {
  if (!java_coordinator_) {
    return;
  }

  JNIEnv* env = base::android::AttachCurrentThread();
  Java_GlicModalCoordinator_simulateDismissingForTesting(  // IN-TEST
      env, java_coordinator_);
}

void GlicModalAndroid::ResizeDueToAutoResize(content::WebContents* source,
                                             const gfx::Size& new_size) {
  if (!size_to_content_ || !java_coordinator_) {
    return;
  }

  JNIEnv* env = base::android::AttachCurrentThread();
  Java_GlicModalCoordinator_onContentSizeChanged(env, java_coordinator_,
                                                 new_size.height());
}

void GlicModalAndroid::CloseContents(content::WebContents* source) {
  Close();
}

content::WebContents* GlicModalAndroid::OpenURLFromTab(
    content::WebContents* source,
    const content::OpenURLParams& params,
    base::OnceCallback<void(content::NavigationHandle&)>
        navigation_handle_callback) {
  tabs::TabInterface* host_tab = tab_handle_.Get();
  if (!host_tab || !host_tab->GetContents()) {
    return nullptr;
  }
  return host_tab->GetContents()->OpenURL(
      params, std::move(navigation_handle_callback));
}

void GlicModalAndroid::RenderFrameCreated(
    content::RenderFrameHost* render_frame_host) {
  UpdateAutoResize(modal_web_contents_
                       ? modal_web_contents_->GetRenderWidgetHostView()
                       : nullptr);
}

void GlicModalAndroid::RenderFrameHostChanged(
    content::RenderFrameHost* old_host,
    content::RenderFrameHost* new_host) {
  UpdateAutoResize(modal_web_contents_
                       ? modal_web_contents_->GetRenderWidgetHostView()
                       : nullptr);
}

void GlicModalAndroid::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  UpdateAutoResize(modal_web_contents_
                       ? modal_web_contents_->GetRenderWidgetHostView()
                       : nullptr);
}

void GlicModalAndroid::OnDismissed() {
  Observe(nullptr);
  tab_will_detach_subscription_ = {};
  tab_will_become_hidden_subscription_ = {};
  java_coordinator_.Reset();
  if (modal_web_contents_) {
    modal_web_contents_->SetDelegate(nullptr);
    base::SequencedTaskRunner::GetCurrentDefault()->DeleteSoon(
        FROM_HERE, std::move(modal_web_contents_));
  }

  if (on_closed_) {
    std::move(on_closed_).Run();
  }
}

void GlicModalAndroid::OnAvailableSizeChanged(JNIEnv* env,
                                              int width_dip,
                                              int height_dip) {
  available_size_dip_ = gfx::Size(width_dip, height_dip);
  UpdateAutoResize(modal_web_contents_
                       ? modal_web_contents_->GetRenderWidgetHostView()
                       : nullptr);
  if (on_available_size_changed_) {
    on_available_size_changed_.Run(available_size_dip_);
  }
}

void GlicModalAndroid::UpdateAutoResize(content::RenderWidgetHostView* rwhv) {
  if (!size_to_content_ || !rwhv) {
    return;
  }
  const gfx::Size max_size =
      available_size_dip_.IsEmpty() ? max_size_dip_ : available_size_dip_;
  if (max_size.IsEmpty()) {
    return;
  }
  const int min_height = std::min(min_size_dip_.height(), max_size.height());
  rwhv->EnableAutoResize(gfx::Size(max_size.width(), std::max(1, min_height)),
                         max_size);
}

void GlicModalAndroid::OnTabWillDetach(
    tabs::TabInterface* tab,
    tabs::TabInterface::DetachReason reason) {
  if (reason == tabs::TabInterface::DetachReason::kDelete) {
    Close();
  }
}

void GlicModalAndroid::OnTabWillBecomeHidden(tabs::TabInterface* tab) {
  Close();
}

// static
std::unique_ptr<GlicModal> GlicModal::Show(tabs::TabInterface& tab,
                                           Params params) {
  content::WebContents* source_contents = tab.GetContents();
  if (!source_contents) {
    return nullptr;
  }

  ui::WindowAndroid* window_android =
      source_contents->GetTopLevelNativeWindow();
  if (!window_android) {
    return nullptr;
  }

  Profile* profile =
      Profile::FromBrowserContext(source_contents->GetBrowserContext());
  if (!profile) {
    return nullptr;
  }

  auto modal = std::make_unique<GlicModalAndroid>();
  if (!modal->ShowInternal(tab, profile, window_android, std::move(params))) {
    return nullptr;
  }
  return modal;
}

}  // namespace glic

DEFINE_JNI(GlicModalCoordinator)
