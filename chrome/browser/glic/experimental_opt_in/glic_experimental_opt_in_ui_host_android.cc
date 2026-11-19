// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/experimental_opt_in/glic_experimental_opt_in_ui_host.h"

#include <memory>
#include <utility>

#include "base/android/jni_android.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/flags/android/chrome_session_state.h"
#include "chrome/browser/glic/browser_ui/glic_modal.h"
#include "chrome/browser/glic/common/glic_navigation.h"
#include "chrome/browser/glic/experimental_opt_in/glic_experimental_opt_in_ui_host_android.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#include "chrome/browser/ui/navigator/browser_navigator_params.h"
#include "chrome/common/webui_url_constants.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "ui/android/window_android.h"
#include "ui/base/mojom/window_open_disposition.mojom.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"

// JNI headers must be included after standard headers.
#include "chrome/browser/glic/android/jni_headers/GlicUiCoordinator_jni.h"

namespace glic {

namespace {

constexpr int kPortraitMaxWidthDp = 380;
constexpr int kPortraitScrollableMaxHeightDp = 567;
constexpr int kPortraitNonScrollableMaxHeightDp = 710;
constexpr int kLandscapeMaxWidthDp = 600;
constexpr int kLandscapeMaxHeightDp = 310;

// Returns the tab model backing the window the user is currently looking at,
// for `profile`.
TabModel* GetActiveTabModel(Profile* profile) {
  TabModel* active_model = nullptr;
  TabModelList::ForEach(profile, [&](TabModel* model) {
    if (model->IsActiveModel()) {
      active_model = model;
      return false;
    }
    return true;
  });
  return active_model;
}

// Like GetActiveTabModel(), but restricted to models it is meaningful to open
// a new tab in. Custom Tabs, Trusted Web Activities and web apps host a single
// tab with no tab strip, so a tab inserted into them would be unreachable.
// TabMatcherAndroid filters these out for the same reason.
TabModel* GetActiveTabbedModel(Profile* profile) {
  TabModel* model = GetActiveTabModel(profile);
  if (!model ||
      model->activity_type() != chrome::android::ActivityType::kTabbed) {
    return nullptr;
  }
  return model;
}

tabs::TabInterface* GetActiveTab(Profile* profile) {
  TabModel* model = GetActiveTabModel(profile);
  return model ? model->GetActiveTab() : nullptr;
}

bool IsNewTabPageUrl(const GURL& url) {
  return search::IsNewTabOrigin(url) || search::IsNTPURL(url);
}

}  // namespace

GlicExperimentalOptInUIHostAndroid::GlicExperimentalOptInUIHostAndroid(
    Profile* profile,
    Delegate* delegate)
    : profile_(profile), delegate_(delegate) {}

GlicExperimentalOptInUIHostAndroid::~GlicExperimentalOptInUIHostAndroid() =
    default;

void GlicExperimentalOptInUIHostAndroid::Show(
    content::WebContents* web_contents) {
  if (modal_) {
    return;
  }
  is_accepted_ = false;

  tabs::TabInterface* tab =
      web_contents ? tabs::TabInterface::MaybeGetFromContents(web_contents)
                   : GetActiveTab(profile_);
  if (!tab) {
    OnModalClosed();
    return;
  }

  modal_host_tab_ = tab->GetHandle();

  GlicModal::Params params;
  params.content_url = GURL(chrome::kChromeUIGlicExperimentalOptInURL);
  const bool non_scrollable = base::FeatureList::IsEnabled(
      features::kGlicExperimentalOptInDialogNonScrollable);
  params.portrait_max_size_dp = gfx::Size(
      kPortraitMaxWidthDp, non_scrollable ? kPortraitNonScrollableMaxHeightDp
                                          : kPortraitScrollableMaxHeightDp);
  params.landscape_max_size_dp =
      gfx::Size(kLandscapeMaxWidthDp, kLandscapeMaxHeightDp);
  params.show_android_close_button = true;
  // Keep the dialog up when another tab is foregrounded behind it (e.g. by an
  // external trigger intent), so concurrent opt-in requests coalesce onto the
  // showing dialog instead of declining it. See
  // GetOrCreateSuitableWebContents().
  params.close_on_android_tab_hidden = false;
  // TODO(b/586590010): Remove once the <600dp experimental opt-in -> Glic
  // bottom-sheet transition is finalized with UX.
  params.deprecated_always_use_modal_dialog = !base::FeatureList::IsEnabled(
      features::kGlicExperimentalOptInBottomSheet);
  params.on_closed =
      base::BindOnce(&GlicExperimentalOptInUIHostAndroid::OnModalClosed,
                     weak_ptr_factory_.GetWeakPtr());

  modal_ = GlicModal::Show(*tab, std::move(params));
  if (!modal_) {
    OnModalClosed();
    return;
  }

  if (ui::WindowAndroid* window_android = GetWindowAndroid()) {
    JNIEnv* env = base::android::AttachCurrentThread();
    Java_GlicUiCoordinator_onPendingActorTaskTrigger(env, window_android);
  }

  OnDialogOpened();
}

void GlicExperimentalOptInUIHostAndroid::Close(bool accepted) {
  is_accepted_ = accepted;
  if (accepted) {
    if (ui::WindowAndroid* window_android = GetWindowAndroid()) {
      JNIEnv* env = base::android::AttachCurrentThread();
      Java_GlicUiCoordinator_onPendingActorTaskTrigger(env, window_android);
    }
  }
  if (!modal_) {
    OnModalClosed();
    return;
  }

  modal_->Close();
}

void GlicExperimentalOptInUIHostAndroid::OnModalClosed() {
  RecordShowDurationOnClose();
  const bool accepted = std::exchange(is_accepted_, false);
  if (!accepted) {
    if (ui::WindowAndroid* window_android = GetWindowAndroid()) {
      JNIEnv* env = base::android::AttachCurrentThread();
      Java_GlicUiCoordinator_onPendingActorTaskFailed(env, window_android);
    }
  }
  modal_.reset();
  modal_host_tab_ = tabs::TabHandle();
  dialog_tab_ = tabs::TabHandle();
  NotifyDelegateClosed(accepted);
}

ui::WindowAndroid* GlicExperimentalOptInUIHostAndroid::GetWindowAndroid()
    const {
  tabs::TabInterface* tab = modal_host_tab_.Get();
  if (!tab || !tab->GetContents()) {
    return nullptr;
  }
  return tab->GetContents()->GetTopLevelNativeWindow();
}

void GlicExperimentalOptInUIHostAndroid::OpenLinkInNewTab(const GURL& url) {
  if (!url.SchemeIsHTTPOrHTTPS()) {
    return;
  }
  std::unique_ptr<NavigateParams> params =
      std::make_unique<NavigateParams>(profile_, url, ui::PAGE_TRANSITION_LINK);
  params->disposition = ui::mojom::WindowOpenDisposition::kNewPopup;
  params->opened_by_another_window = true;
  glic::NavigateAsync(std::move(params), base::DoNothing());
}

content::WebContents*
GlicExperimentalOptInUIHostAndroid::GetOrCreateSuitableWebContents() {
  // A dialog is already showing over a tab this host selected. Hand that same
  // tab back instead of opening another one: Show() ignores the contents while
  // a dialog is up, so a second tab would be left stranded behind the dialog
  // after having pulled the user away from what they were looking at.
  if (modal_) {
    if (tabs::TabInterface* dialog_tab = dialog_tab_.Get()) {
      return dialog_tab->GetContents();
    }
    return nullptr;
  }

  TabModel* model = GetActiveTabbedModel(profile_);
  if (!model) {
    return nullptr;
  }

  // Show the dialog over an ordinary new tab page rather than over whatever
  // the user is currently looking at. This deliberately differs from the
  // desktop host, which anchors the dialog to a tab already showing the Gemini
  // surface: on Clank a plain NTP is the intended backdrop. If the active tab
  // is already an NTP (for example, opened by the external trigger intent
  // fallback), reuse it instead of opening a duplicate tab.
  tabs::TabInterface* target_tab = model->GetActiveTab();
  if (!target_tab || !IsNewTabPageUrl(target_tab->GetURL())) {
    target_tab =
        model->OpenTab(GURL(chrome::kChromeUINewTabURL), model->GetTabCount(),
                       /*foreground=*/true);
  }
  if (!target_tab) {
    return nullptr;
  }
  dialog_tab_ = target_tab->GetHandle();
  return target_tab->GetContents();
}

void GlicExperimentalOptInUIHostAndroid::SimulateDismissingForTesting() {
  if (!modal_) {
    OnModalClosed();
    return;
  }
  modal_->SimulateDismissingForTesting();  // IN-TEST
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
