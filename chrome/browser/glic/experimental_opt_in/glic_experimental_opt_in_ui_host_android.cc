// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/experimental_opt_in/glic_experimental_opt_in_ui_host.h"

#include <memory>

#include "base/android/jni_android.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/glic/experimental_opt_in/glic_experimental_opt_in_ui_host_android.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#include "chrome/common/webui_url_constants.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"
#include "ui/android/window_android.h"

// JNI headers must be included after standard headers.
#include "chrome/browser/glic/android/jni_headers/GlicExperimentalOptInUiCoordinator_jni.h"

namespace glic {

namespace {

tabs::TabInterface* GetActiveTab(Profile* profile) {
  for (TabModel* model : TabModelList::models()) {
    if (model->GetProfile() == profile && model->IsActiveModel()) {
      return model->GetActiveTab();
    }
  }
  return nullptr;
}

}  // namespace

void JNI_GlicExperimentalOptInUiCoordinator_OnDismissed(
    JNIEnv* env,
    int64_t nativeGlicExperimentalOptInUIHostAndroid) {
  auto* host = reinterpret_cast<GlicExperimentalOptInUIHostAndroid*>(
      nativeGlicExperimentalOptInUIHostAndroid);
  if (host) {
    host->OnDismissed();
  }
}

GlicExperimentalOptInUIHostAndroid::GlicExperimentalOptInUIHostAndroid(
    Profile* profile,
    Delegate* delegate)
    : profile_(profile), delegate_(delegate) {}

GlicExperimentalOptInUIHostAndroid::~GlicExperimentalOptInUIHostAndroid() {
  if (java_dialog_) {
    JNIEnv* env = base::android::AttachCurrentThread();
    Java_GlicExperimentalOptInUiCoordinator_onNativeDestroyed(env,
                                                              java_dialog_);
  }
}

void GlicExperimentalOptInUIHostAndroid::Show(
    content::WebContents* web_contents) {
  if (java_dialog_) {
    return;
  }
  is_accepted_ = false;

  content::WebContents* source_contents = web_contents;
  if (!source_contents) {
    tabs::TabInterface* tab = GetActiveTab(profile_);
    if (tab) {
      source_contents = tab->GetContents();
    }
  }

  if (!source_contents) {
    OnDismissed();
    return;
  }

  ui::WindowAndroid* window_android =
      source_contents->GetTopLevelNativeWindow();
  if (!window_android) {
    // There is no window to parent the dialog to, so nothing will ever be
    // shown. Report the dismissal now, otherwise the caller's callback is
    // never run.
    OnDismissed();
    return;
  }

  content::WebContents::CreateParams params(profile_);
  opt_in_web_contents_ = content::WebContents::Create(params);
  opt_in_web_contents_->GetController().LoadURLWithParams(
      content::NavigationController::LoadURLParams(
          GURL(chrome::kChromeUIGlicExperimentalOptInURL)));

  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jobject> java_obj =
      Java_GlicExperimentalOptInUiCoordinator_show(
          env, reinterpret_cast<intptr_t>(this), window_android,
          opt_in_web_contents_.get());

  if (!java_obj) {
    OnDismissed();
    return;
  }

  java_dialog_.Reset(java_obj);
}

void GlicExperimentalOptInUIHostAndroid::Close(bool accepted) {
  is_accepted_ = accepted;
  if (!java_dialog_) {
    OnDismissed();
    return;
  }

  JNIEnv* env = base::android::AttachCurrentThread();
  Java_GlicExperimentalOptInUiCoordinator_dismiss(env, java_dialog_);
}

void GlicExperimentalOptInUIHostAndroid::OnDismissed() {
  java_dialog_.Reset();
  if (opt_in_web_contents_) {
    base::SequencedTaskRunner::GetCurrentDefault()->DeleteSoon(
        FROM_HERE, std::move(opt_in_web_contents_));
  }

  NotifyDelegateClosed(is_accepted_);
}

content::WebContents*
GlicExperimentalOptInUIHostAndroid::GetOrCreateSuitableWebContents() {
  return nullptr;
}

void GlicExperimentalOptInUIHostAndroid::SimulateDismissingForTesting() {
  if (!java_dialog_) {
    OnDismissed();
    return;
  }
  // Dismiss through Java rather than calling OnDismissed() directly. Otherwise
  // the dialog stays on screen holding a pointer to `this`, and calls back into
  // the destroyed host when the activity is later torn down.
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_GlicExperimentalOptInUiCoordinator_simulateDismissingForTesting(  // IN-TEST
      env, java_dialog_);
}

void GlicExperimentalOptInUIHostAndroid::NotifyDelegateClosed(bool accepted) {
  if (delegate_) {
    delegate_->OnUIClosed(accepted);
  }
}

// static
std::unique_ptr<GlicExperimentalOptInUIHost>
GlicExperimentalOptInUIHost::Create(Profile* profile, Delegate* delegate) {
  return std::make_unique<GlicExperimentalOptInUIHostAndroid>(profile,
                                                              delegate);
}

}  // namespace glic

DEFINE_JNI(GlicExperimentalOptInUiCoordinator)
