// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/public/widget/glic_side_panel_coordinator_desktop_android.h"

#include "base/functional/callback.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/context_sharing/tab_bottom_sheet/android/co_browse_views_bridge.h"
#include "chrome/browser/glic/browser_ui/glic_toast.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_instance.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/public/service/glic_instance_coordinator.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/side_panel/android/side_panel_native_view_android.h"
#include "chrome/browser/ui/side_panel/side_panel_entry.h"
#include "chrome/browser/ui/side_panel/side_panel_enums.h"
#include "chrome/browser/ui/side_panel/side_panel_registry.h"
#include "chrome/browser/ui/side_panel/side_panel_ui.h"
#include "chrome/browser/ui/side_panel/side_panel_ui_provider.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/generated_resources.h"
#include "components/tabs/public/tab_interface.h"

namespace glic {

namespace {

// TODO(crbug.com/515493573): Remove this once Glic transitions to using the
// bottom sheet for narrow windows.
std::unique_ptr<GlicToast> MaybeShowResizeToast(
    tabs::TabInterface* tab,
    GlicKeyedService* glic_service) {
  content::WebContents* web_contents = tab->GetContents();
  if (!web_contents || !glic_service) {
    return nullptr;
  }

  bool is_actuating = false;
  if (auto* instance =
          glic_service->instance_coordinator().GetInstanceForTab(tab)) {
    is_actuating = instance->IsActuating();
  }

  int title_res_id =
      is_actuating ? IDS_GLIC_TASK_PAUSED_TITLE : IDS_GLIC_CHAT_HIDDEN_TITLE;
  int description_res_id = is_actuating ? IDS_GLIC_TASK_PAUSED_DESCRIPTION
                                        : IDS_GLIC_CHAT_HIDDEN_DESCRIPTION;
  return GlicToast::Show(web_contents, title_res_id, description_res_id);
}

}  // namespace

GlicSidePanelCoordinatorDesktopAndroid::GlicSidePanelCoordinatorDesktopAndroid(
    tabs::TabInterface* tab_interface,
    SidePanelRegistry* side_panel_registry,
    Profile* profile)
    : GlicSidePanelCoordinator(tab_interface),
      tab_(tab_interface),
      side_panel_registry_(side_panel_registry),
      glic_service_(GlicKeyedServiceFactory::GetGlicKeyedService(profile)) {
  CHECK(side_panel_registry_);
  if (glic_service_) {
    on_glic_enabled_changed_subscription_ =
        glic_service_->enabling().RegisterAllowedChanged(base::BindRepeating(
            &GlicSidePanelCoordinatorDesktopAndroid::OnGlicEnabledChanged,
            base::Unretained(this)));
    if (glic_service_->enabling().IsAllowed()) {
      CreateAndRegisterEntry();
    }
  }
}

GlicSidePanelCoordinatorDesktopAndroid::
    ~GlicSidePanelCoordinatorDesktopAndroid() {
  if (entry_) {
    entry_->RemoveObserver(this);
  }
}

void GlicSidePanelCoordinatorDesktopAndroid::CreateAndRegisterEntry() {
  if (side_panel_registry_->GetEntryForKey(
          SidePanelEntry::Key(SidePanelEntry::Id::kGlic))) {
    return;
  }

  // Note that Android only supports toolbar-height side panels, so
  // we pass `SidePanelType::kToolbar` without checking the
  // `kGlicUseToolbarHeightSidePanel` flag. The flag controls the rollout of
  // toolbar-height panel on WML, so it may be disabled and not in sync with the
  // GLiC rollout on Android.
  //
  // TODO(crbug.com/515153523): Consider if Android side panels should be
  // WebContents-height.
  auto entry = std::make_unique<SidePanelEntry>(
      SidePanelType::kToolbar, SidePanelEntry::Key(SidePanelEntry::Id::kGlic),
      base::BindRepeating(&GlicSidePanelCoordinatorDesktopAndroid::CreateView,
                          base::Unretained(this)),
      base::BindRepeating(
          &GlicSidePanelCoordinatorDesktopAndroid::GetPreferredWidth,
          base::Unretained(this)));
  entry->set_should_show_header(false);
  entry->set_should_show_ephemerally_in_toolbar(false);
  entry->AddObserver(this);
  entry_ = entry->GetWeakPtr();
  side_panel_registry_->Register(std::move(entry));
}

void GlicSidePanelCoordinatorDesktopAndroid::Show(const ShowOptions& options) {
  auto* window_side_panel_ui = GetWindowSidePanelUI();
  if (!window_side_panel_ui || !entry_) {
    return;
  }
  if (!tab_->IsActivated()) {
    if (entry_) {
      // The tab is in the background, so we just mark it for showing the glic
      // side panel when it becomes the active tab. eg. This flow can be
      // encountered when a background tab is bound via daisy chaining.
      side_panel_registry_->SetActiveEntry(entry_.get());
      SetState(State::kBackgrounded);
    }
    return;
  }
  window_side_panel_ui->Show(entry_->key(), std::nullopt,
                             options.suppress_animations);
}

void GlicSidePanelCoordinatorDesktopAndroid::Close(
    const CloseOptions& options) {
  auto* window_side_panel_ui = GetWindowSidePanelUI();
  if (!window_side_panel_ui || !entry_) {
    return;
  }
  if (state_ == State::kShown) {
    window_side_panel_ui->Close(SidePanelEntryHideReason::kSidePanelClosed,
                                options.suppress_animations);
    return;
  }
  if (state_ == State::kBackgrounded) {
    CHECK(IsGlicSidePanelActive());
    side_panel_registry_->ResetActiveEntry();
    SetState(State::kClosed);
  }
}

bool GlicSidePanelCoordinatorDesktopAndroid::IsShowing() const {
  return state_ == State::kShown;
}

GlicSidePanelCoordinator::State
GlicSidePanelCoordinatorDesktopAndroid::state() {
  return state_;
}

bool GlicSidePanelCoordinatorDesktopAndroid::SupportsPeek() const {
  return false;
}

void GlicSidePanelCoordinatorDesktopAndroid::OnEntryHiddenWithReason(
    SidePanelEntry* entry,
    SidePanelEntryHideReason reason) {
  CHECK_EQ(entry->key().id(), SidePanelEntry::Id::kGlic);
  if (reason == SidePanelEntryHideReason::kBackgrounded ||
      reason == SidePanelEntryHideReason::kWindowResized) {
    SetState(State::kBackgrounded);
    if (reason == SidePanelEntryHideReason::kWindowResized) {
      resize_toast_ = MaybeShowResizeToast(tab_, glic_service_);
    }
  } else {
    SetState(State::kClosed);
  }
}

void GlicSidePanelCoordinatorDesktopAndroid::OnEntryShown(
    SidePanelEntry* entry) {
  CHECK_EQ(entry->key().id(), SidePanelEntry::Id::kGlic);
  resize_toast_.reset();
  SetState(State::kShown);
}

void GlicSidePanelCoordinatorDesktopAndroid::OnGlicEnabledChanged() {
  // Maybe register side panel entry if not yet registered.
  if (glic_service_ && glic_service_->enabling().IsAllowed()) {
    CreateAndRegisterEntry();
  }
}

SidePanelNativeView GlicSidePanelCoordinatorDesktopAndroid::CreateView(
    SidePanelEntryScope& scope) {
  if (!cobrowse_views_bridge_) {
    cobrowse_views_bridge_ =
        std::make_unique<context_sharing::CoBrowseViewsBridge>(
            *tab_, context_sharing::TabBottomSheetClientType::kGlic,
            context_sharing::CoBrowseContainerType::kSidePanel);
    cobrowse_views_bridge_->CreateCoBrowseViews(web_contents_.get());
  }
  auto view = context_sharing::CoBrowseViewsBridge::GetViewFromCoBrowseViews(
      cobrowse_views_bridge_->GetCoBrowseViews());
  if (!view) {
    return nullptr;
  }
  JNIEnv* env = base::android::AttachCurrentThread();
  return std::make_unique<SidePanelNativeViewAndroid>(
      base::android::ScopedJavaGlobalRef<jobject>(env, view));
}

base::CallbackListSubscription
GlicSidePanelCoordinatorDesktopAndroid::AddStateCallback(
    base::RepeatingCallback<void(State state)> callback) {
  return state_changed_callbacks_.Add(std::move(callback));
}

void GlicSidePanelCoordinatorDesktopAndroid::SetWebContents(
    content::WebContents* web_contents) {
  web_contents_ = web_contents;
  if (cobrowse_views_bridge_) {
    cobrowse_views_bridge_->SetWebContents(web_contents,
                                           /*request_focus=*/false);
  }
}

int GlicSidePanelCoordinatorDesktopAndroid::GetPreferredWidth() {
  return features::kGlicSidePanelMinWidth.Get();
}

bool GlicSidePanelCoordinatorDesktopAndroid::IsGlicSidePanelActive() {
  if (!side_panel_registry_) {
    return false;
  }
  auto* glic_side_panel_entry = side_panel_registry_->GetEntryForKey(
      SidePanelEntryKey(SidePanelEntry::Id::kGlic));
  if (!glic_side_panel_entry) {
    return false;
  }
  const auto& active_entry = side_panel_registry_->GetActiveEntry();
  if (!active_entry.has_value() ||
      active_entry.value() != glic_side_panel_entry) {
    return false;
  }
  return true;
}

SidePanelUI* GlicSidePanelCoordinatorDesktopAndroid::GetWindowSidePanelUI()
    const {
  if (auto* window = tab_->GetBrowserWindowInterface()) {
    return SidePanelUIProvider::From(window);
  }
  return nullptr;
}

void GlicSidePanelCoordinatorDesktopAndroid::SetState(State new_state) {
  state_ = new_state;
  state_changed_callbacks_.Notify(state_);
}

}  // namespace glic
