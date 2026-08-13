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
#include "chrome/browser/context_sharing/tab_bottom_sheet/android/co_browse_container_type.h"
#include "chrome/browser/context_sharing/tab_bottom_sheet/android/co_browse_views_bridge.h"
#include "chrome/browser/context_sharing/tab_bottom_sheet/android/tab_bottom_sheet_bridge.h"
#include "chrome/browser/context_sharing/tab_bottom_sheet/android/tab_bottom_sheet_client_type.h"
#include "chrome/browser/glic/android/jni_headers/GlicBottomSheetComponentProvider_jni.h"
#include "chrome/browser/glic/experimental_opt_in/glic_experimental_opt_in_ui_host_android.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#include "chrome/common/webui_url_constants.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"

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

class BottomSheetSession
    : public context_sharing::TabBottomSheetBridge::Observer {
 public:
  BottomSheetSession(Profile* profile,
                     tabs::TabInterface* tab,
                     base::OnceCallback<void(bool)> on_closed_callback)
      : on_closed_callback_(std::move(on_closed_callback)) {
    JNIEnv* env = base::android::AttachCurrentThread();
    base::android::ScopedJavaLocalRef<jobject> bottom_sheet_content_provider =
        Java_GlicBottomSheetComponentProvider_createProvider(
            env, profile->GetJavaObject());

    bottom_sheet_bridge_ =
        std::make_unique<context_sharing::TabBottomSheetBridge>(this, tab);

    views_bridge_ = std::make_unique<context_sharing::CoBrowseViewsBridge>(
        *tab, context_sharing::TabBottomSheetClientType::kGlic,
        context_sharing::CoBrowseContainerType::kBottomSheet,
        bottom_sheet_content_provider,
        /*enable_pinch_to_zoom=*/false);

    content::WebContents::CreateParams params(profile);
    bottom_sheet_web_contents_ = content::WebContents::Create(params);
    bottom_sheet_web_contents_->GetController().LoadURLWithParams(
        content::NavigationController::LoadURLParams(
            GURL(chrome::kChromeUIGlicExperimentalOptInURL)));

    views_bridge_->SetWebContents(bottom_sheet_web_contents_.get(),
                                  /*request_focus=*/false);
  }

  ~BottomSheetSession() override = default;

  bool Show() {
    // TODO(crbug.com/540987436): Wait for the manager to be ready if it isn't
    // initialized yet. Since this can happen at startup, it might not be ready.
    if (bottom_sheet_bridge_->IsManagerReady()) {
      return bottom_sheet_bridge_->Show(views_bridge_->GetCoBrowseViews(),
                                        /*animate=*/true,
                                        /*starts_expanded=*/true);
    }
    return false;
  }

  void Close(bool accepted) {
    current_accepted_ = accepted;
    if (bottom_sheet_bridge_) {
      bottom_sheet_bridge_->Close(/*animate=*/true);
    }
  }

  // context_sharing::TabBottomSheetBridge::Observer implementation:
  void OnClosed() override {
    if (on_closed_callback_) {
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE,
          base::BindOnce(&BottomSheetSession::RunClosedCallback,
                         weak_ptr_factory_.GetWeakPtr(), current_accepted_));
    }
  }

  void OnOpened(bool is_expanded) override {}
  void OnSuppressed() override {}

 private:
  void RunClosedCallback(bool accepted) {
    if (on_closed_callback_) {
      std::move(on_closed_callback_).Run(accepted);
    }
  }

  std::unique_ptr<content::WebContents> bottom_sheet_web_contents_;
  std::unique_ptr<context_sharing::TabBottomSheetBridge> bottom_sheet_bridge_;
  std::unique_ptr<context_sharing::CoBrowseViewsBridge> views_bridge_;

  base::OnceCallback<void(bool)> on_closed_callback_;
  bool current_accepted_ = false;
  base::WeakPtrFactory<BottomSheetSession> weak_ptr_factory_{this};
};

GlicExperimentalOptInUIHostAndroid::GlicExperimentalOptInUIHostAndroid(
    Profile* profile,
    Delegate* delegate)
    : profile_(profile), delegate_(delegate) {}

GlicExperimentalOptInUIHostAndroid::~GlicExperimentalOptInUIHostAndroid() =
    default;

void GlicExperimentalOptInUIHostAndroid::Show(
    content::WebContents* web_contents) {
  if (session_) {
    return;
  }

  // TODO(crbug.com/540987436): Confirm with product if it's fine to show on
  // the current active tab (which shares context with Glic), or if we should
  // open a new dedicated tab for consent flow.
  tabs::TabInterface* tab = web_contents
                                ? TabAndroid::FromWebContents(web_contents)
                                : GetActiveTab(profile_);
  if (!tab) {
    if (delegate_) {
      delegate_->OnUIClosed(/*accepted=*/false);
    }
    return;
  }

  session_ = std::make_unique<BottomSheetSession>(
      profile_, tab,
      base::BindOnce(&GlicExperimentalOptInUIHostAndroid::OnSessionClosed,
                     weak_ptr_factory_.GetWeakPtr()));

  if (!session_->Show()) {
    session_.reset();
    if (delegate_) {
      delegate_->OnUIClosed(/*accepted=*/false);
    }
  }
}

void GlicExperimentalOptInUIHostAndroid::Close(bool accepted) {
  if (session_) {
    session_->Close(accepted);
  }
}

content::WebContents*
GlicExperimentalOptInUIHostAndroid::GetOrCreateSuitableWebContents() {
  // TODO(crbug.com/540987436): Update once we have UX requirements.
  return nullptr;
}

void GlicExperimentalOptInUIHostAndroid::
    SimulateClosingBottomSheetForTesting() {
  if (session_) {
    session_->OnClosed();
  }
}

void GlicExperimentalOptInUIHostAndroid::OnSessionClosed(bool accepted) {
  // We are executing from a posted task, so the Android bridge has fully
  // finished closing. It is safe to synchronously delete the session.
  session_.reset();

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
