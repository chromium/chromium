// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/android/contextual_tasks_panel_host_desktop_android.h"

#include <utility>

#include "base/android/jni_android.h"
#include "base/check.h"
#include "chrome/browser/context_sharing/tab_bottom_sheet/android/co_browse_container_type.h"
#include "chrome/browser/context_sharing/tab_bottom_sheet/android/co_browse_views_bridge.h"
#include "chrome/browser/context_sharing/tab_bottom_sheet/android/tab_bottom_sheet_client_type.h"
#include "chrome/browser/contextual_tasks/android/contextual_tasks_toast.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/side_panel/android/side_panel_native_view_android.h"
#include "chrome/browser/ui/side_panel/side_panel_entry.h"
#include "chrome/browser/ui/side_panel/side_panel_entry_key.h"
#include "chrome/browser/ui/side_panel/side_panel_enums.h"
#include "chrome/browser/ui/side_panel/side_panel_registry.h"
#include "chrome/browser/ui/side_panel/side_panel_ui.h"
#include "chrome/browser/ui/side_panel/side_panel_ui_provider.h"
#include "components/input/native_web_keyboard_event.h"
#include "components/strings/grit/components_strings.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "ui/android/window_android.h"
#include "ui/base/base_window.h"
#include "ui/base/l10n/l10n_util.h"

using base::android::AttachCurrentThread;

namespace {
constexpr int kSidePanelMinWidth = 440;
}  // namespace

namespace contextual_tasks {

ContextualTasksPanelHostDesktopAndroid::ContextualTasksPanelHostDesktopAndroid(
    BrowserWindowInterface* browser_window)
    : browser_window_(browser_window) {
  CHECK(browser_window_);
  MaybeRegisterEntry();
  MaybeCreateBridge();
}

ContextualTasksPanelHostDesktopAndroid::
    ~ContextualTasksPanelHostDesktopAndroid() {
  if (web_contents_) {
    web_contents_->SetDelegate(nullptr);
    web_contents_ = nullptr;
  }
}

void ContextualTasksPanelHostDesktopAndroid::AddObserver(
    ContextualTasksPanelHost::Observer* observer) {
  observers_.AddObserver(observer);
}

void ContextualTasksPanelHostDesktopAndroid::RemoveObserver(
    ContextualTasksPanelHost::Observer* observer) {
  observers_.RemoveObserver(observer);
}

void ContextualTasksPanelHostDesktopAndroid::Show(AnimationStyle animation) {
  MaybeRegisterEntry();

  auto* side_panel_ui = GetSidePanelUI();
  if (!side_panel_ui) {
    return;
  }
  // TODO(crbug.com/526821301): Consider plumbing trigger from call site.
  SidePanelOpenTrigger trigger = (animation == AnimationStyle::kNoAnimation)
                                     ? SidePanelOpenTrigger::kTabChanged
                                     : SidePanelOpenTrigger::kContextualTasks;
  side_panel_ui->Show(
      SidePanelEntry::Key(SidePanelEntry::Id::kContextualTasks), trigger,
      /*suppress_animations=*/animation == AnimationStyle::kNoAnimation);
}

void ContextualTasksPanelHostDesktopAndroid::Close(AnimationStyle animation) {
  resize_toast_.reset();
  auto* side_panel_ui = GetSidePanelUI();
  if (!side_panel_ui || !side_panel_ui->IsSidePanelShowing()) {
    return;
  }
  side_panel_ui->Close(
      SidePanelEntryHideReason::kSidePanelClosed,
      /*suppress_animations=*/animation == AnimationStyle::kNoAnimation);
}

bool ContextualTasksPanelHostDesktopAndroid::IsPanelInitialized() {
  // Bypasses the initialization check to allow SetWebContents to propagate
  // correctly during the initial setup flow.
  return true;
}

bool ContextualTasksPanelHostDesktopAndroid::IsPanelOpenForContextualTask()
    const {
  auto* side_panel_ui = GetSidePanelUI();
  if (!side_panel_ui) {
    return false;
  }
  return side_panel_ui->IsSidePanelEntryShowing(
      SidePanelEntry::Key(SidePanelEntry::Id::kContextualTasks));
}

bool ContextualTasksPanelHostDesktopAndroid::IsPanelSuppressed() const {
  if (suppressed_for_testing_) {
    return true;
  }
  auto* side_panel_ui = GetSidePanelUI();
  if (!side_panel_ui) {
    return false;
  }
  // Contextual Tasks should be suppressed if Glic is currently showing in the
  // side panel. See `contextual_tasks_panel_host_desktop.cc` for the
  // corresponding logic.
  return side_panel_ui->IsSidePanelEntryShowing(
      SidePanelEntry::Key(SidePanelEntry::Id::kGlic));
}

void ContextualTasksPanelHostDesktopAndroid::SetPanelSuppressedForTesting(
    bool suppressed) {
  suppressed_for_testing_ = suppressed;
}

content::WebContents* ContextualTasksPanelHostDesktopAndroid::GetWebContents() {
  return web_contents_;
}

content::WebContents*
ContextualTasksPanelHostDesktopAndroid::GetToolbarWebContents() {
  return GetWebContents();
}

bool ContextualTasksPanelHostDesktopAndroid::MaybeCreateBridge() {
  // Reuse the bridge if it exists and the tab we created it with is still
  // alive.
  if (co_browse_views_bridge_ && tab_ref_) {
    return true;
  }

  tabs::TabInterface* active_tab =
      TabListInterface::From(browser_window_)->GetActiveTab();
  if (!active_tab) {
    return false;
  }

  tab_ref_ = active_tab->GetWeakPtr();
  co_browse_views_bridge_ =
      std::make_unique<context_sharing::CoBrowseViewsBridge>(
          *active_tab,
          context_sharing::TabBottomSheetClientType::kContextualTasks,
          context_sharing::CoBrowseContainerType::kSidePanel,
          /*bottom_sheet_content_provider=*/nullptr,
          /*enable_pinch_to_zoom=*/true);
  return co_browse_views_bridge_ != nullptr;
}

void ContextualTasksPanelHostDesktopAndroid::SetWebContents(
    content::WebContents* web_contents) {
  if (web_contents_ == web_contents) {
    return;
  }

  if (content::WebContents* prev = std::exchange(web_contents_, web_contents)) {
    prev->SetDelegate(nullptr);
  }

  if (web_contents_) {
    web_contents_->SetDelegate(this);
  }

  if (MaybeCreateBridge()) {
    co_browse_views_bridge_->SetWebContents(web_contents,
                                            /*request_focus=*/true);
  }
}

void ContextualTasksPanelHostDesktopAndroid::OnEntryHiddenWithReason(
    SidePanelEntry* entry,
    SidePanelEntryHideReason reason) {
  CHECK_EQ(entry->key().id(), SidePanelEntry::Id::kContextualTasks);
  is_open_ = false;
  NotifySurfaceStateChanged(
      ContextualTasksPanelHost::SurfaceState::kClosed,
      reason == SidePanelEntryHideReason::kReplaced
          ? ContextualTasksPanelHost::StateChangeReason::kSystemAction
          : ContextualTasksPanelHost::StateChangeReason::kUserAction);

  if (reason == SidePanelEntryHideReason::kWindowResized) {
    tabs::TabInterface* active_tab =
        TabListInterface::From(browser_window_)->GetActiveTab();
    if (active_tab && active_tab->GetContents()) {
      resize_toast_ = ContextualTasksToast::Show(
          active_tab->GetContents(), IDS_CONTEXTUAL_TASKS_CHAT_HIDDEN_TITLE,
          IDS_CONTEXTUAL_TASKS_CHAT_HIDDEN_DESCRIPTION);
    }
  }
}

void ContextualTasksPanelHostDesktopAndroid::OnEntryShown(
    SidePanelEntry* entry) {
  CHECK_EQ(entry->key().id(), SidePanelEntry::Id::kContextualTasks);
  is_open_ = true;
  resize_toast_.reset();
  NotifySurfaceStateChanged(
      ContextualTasksPanelHost::SurfaceState::kVisible,
      ContextualTasksPanelHost::StateChangeReason::kUserAction);
}

void ContextualTasksPanelHostDesktopAndroid::MaybeRegisterEntry() {
  auto* registry = SidePanelRegistry::From(browser_window_);
  if (!registry) {
    return;
  }

  if (registry->GetEntryForKey(
          SidePanelEntry::Key(SidePanelEntry::Id::kContextualTasks))) {
    return;
  }

  auto entry = std::make_unique<SidePanelEntry>(
      SidePanelType::kToolbar,
      SidePanelEntry::Key(SidePanelEntry::Id::kContextualTasks),
      base::BindRepeating(&ContextualTasksPanelHostDesktopAndroid::CreateView,
                          base::Unretained(this)),
      base::BindRepeating([]() { return kSidePanelMinWidth; }));
  entry->set_should_show_header(false);
  entry->set_should_show_ephemerally_in_toolbar(false);
  entry->SetProperty(
      kSidePanelTitleKey,
      l10n_util::GetStringUTF16(IDS_CONTEXTUAL_TASKS_AI_MODE_TITLE));
  side_panel_entry_observation_.Observe(entry.get());
  registry->Register(std::move(entry));
}

SidePanelUI* ContextualTasksPanelHostDesktopAndroid::GetSidePanelUI() const {
  return SidePanelUIProvider::From(browser_window_);
}

SidePanelNativeView ContextualTasksPanelHostDesktopAndroid::CreateView(
    SidePanelEntryScope& scope) {
  if (!co_browse_views_bridge_ || !tab_ref_) {
    if (!MaybeCreateBridge()) {
      return nullptr;
    }
    co_browse_views_bridge_->CreateCoBrowseViews(web_contents_,
                                                 /*request_focus=*/true);
  }

  auto view = context_sharing::CoBrowseViewsBridge::GetViewFromCoBrowseViews(
      co_browse_views_bridge_->GetCoBrowseViews());
  if (!view) {
    return nullptr;
  }
  return std::make_unique<SidePanelNativeViewAndroid>(
      base::android::ScopedJavaGlobalRef<jobject>(
          base::android::AttachCurrentThread(), view));
}

void ContextualTasksPanelHostDesktopAndroid::NotifySurfaceStateChanged(
    ContextualTasksPanelHost::SurfaceState state,
    ContextualTasksPanelHost::StateChangeReason reason) {
  observers_.Notify(&ContextualTasksPanelHost::Observer::OnSurfaceStateChanged,
                    state, reason);
}

content::WebContents* ContextualTasksPanelHostDesktopAndroid::OpenURLFromTab(
    content::WebContents* source,
    const content::OpenURLParams& params,
    base::OnceCallback<void(content::NavigationHandle&)>
        navigation_handle_callback) {
  if (browser_window_) {
    return browser_window_->OpenURL(params,
                                    std::move(navigation_handle_callback));
  }
  return nullptr;
}

bool ContextualTasksPanelHostDesktopAndroid::HandleKeyboardEvent(
    content::WebContents* source,
    const input::NativeWebKeyboardEvent& event) {
  tabs::TabInterface* active_tab =
      TabListInterface::From(browser_window_)->GetActiveTab();
  if (active_tab && active_tab->GetContents() &&
      active_tab->GetContents()->GetDelegate()) {
    return active_tab->GetContents()->GetDelegate()->HandleKeyboardEvent(source,
                                                                         event);
  }
  return false;
}

}  // namespace contextual_tasks
