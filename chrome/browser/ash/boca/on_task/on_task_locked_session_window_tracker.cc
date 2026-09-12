// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/boca/on_task/on_task_locked_session_window_tracker.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

#include "ash/boca/on_task/on_task_pod_controller.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/notifier_catalogs.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"
#include "ash/webui/boca_ui/url_constants.h"
#include "ash/wm/screen_pinning_controller.h"
#include "ash/wm/window_state.h"
#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/ash/boca/on_task/on_task_locked_controller.h"
#include "chrome/browser/ash/boca/on_task/on_task_pod_controller_impl.h"
#include "chrome/browser/ui/ash/system_web_apps/system_web_app_ui_utils.h"
#include "chrome/browser/ui/immersive/immersive_mode_controller.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chromeos/ash/components/boca/boca_metrics_util.h"
#include "chromeos/ash/components/boca/boca_role_util.h"
#include "chromeos/ash/components/boca/boca_window_observer.h"
#include "chromeos/ash/components/boca/on_task/notification_constants.h"
#include "chromeos/ash/components/boca/on_task/on_task_notifications_manager.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "chromeos/ash/components/browser_delegate/browser_controller.h"
#include "chromeos/ash/components/browser_delegate/browser_delegate.h"
#include "chromeos/ash/components/system_web_apps/system_web_app_type.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "chromeos/ui/base/window_properties.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/sessions/core/session_id.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/page.h"
#include "content/public/browser/webid/identity_credential_source.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tracker.h"
#include "ui/base/l10n/l10n_util.h"

LockedSessionWindowTracker::LockedSessionWindowTracker(
    std::unique_ptr<OnTaskBlocklist> on_task_blocklist,
    content::BrowserContext* context)
    : on_task_blocklist_(std::move(on_task_blocklist)),
      is_consumer_profile_(ash::boca_util::IsConsumer(
          ash::BrowserContextHelper::Get()->GetUserByBrowserContext(context))),
      notifications_manager_(ash::boca::OnTaskNotificationsManager::Create()) {
  // Set up window tracker to observe app instances only on consumer devices.
  // This will enable us to filter out unmanaged app instances.
  if (is_consumer_profile_) {
    browser_controller_observation_.Observe(
        ash::BrowserController::GetInstance());
  }
}

LockedSessionWindowTracker::~LockedSessionWindowTracker() {
  if (is_consumer_profile_) {
    browser_controller_observation_.Reset();
  }
  CleanupWindowTracker();
}

void LockedSessionWindowTracker::AddObserver(
    ash::boca::BocaWindowObserver* observer) {
  if (!observers_.HasObserver(observer)) {
    observers_.AddObserver(observer);
  }
}

void LockedSessionWindowTracker::RemoveObserver(
    ash::boca::BocaWindowObserver* observer) {
  observers_.RemoveObserver(observer);
}

void LockedSessionWindowTracker::InitializeBrowserInfoForTracking(
    ash::BrowserDelegate* browser) {
  if (browser_ && browser_ != browser) {
    CleanupWindowTracker();
  }
  if (!browser || browser == browser_) {
    return;
  }
  browser_ = browser;
  tab_observation_.Observe(ash::BrowserController::GetInstance());
  active_tab_observer_.Observe(browser_->GetActiveWebContents());

  if (ash::features::IsBocaOnTaskPodEnabled()) {
    on_task_pod_controller_ =
        std::make_unique<ash::OnTaskPodControllerImpl>(browser_);
  }
}

void LockedSessionWindowTracker::RefreshUrlBlocklist() {
  if (!browser_ || !browser_->GetActiveWebContents() ||
      !browser_->GetActiveWebContents()->GetLastCommittedURL().is_valid()) {
    return;
  }

  on_task_blocklist_->RefreshForUrlBlocklist(browser_->GetActiveWebContents());
}

void LockedSessionWindowTracker::set_oauth_in_progress(
    bool in_progress,
    ash::BrowserDelegate* browser) {
  if (in_progress && !oauth_in_progress_) {
    ash::boca::RecordOnTaskOAuthTriggered();
  }
  oauth_in_progress_ = in_progress;
  if (in_progress && browser &&
      browser->GetType() == ash::BrowserType::kAppPopup) {
    authorized_oauth_browser_ = browser;
  }
}

void LockedSessionWindowTracker::MaybeCloseBrowser(
    ash::BrowserDelegate* browser) {
  CHECK(browser);
  pending_close_tasks_.erase(browser);

  // The browser window needs to be closed if:
  // 1. It is a duplicate instance of the Boca SWA outside the one being
  //    tracked.
  // 2. It is an unmanaged instance of the Boca SWA spawned through
  //    non-conventional means.
  // 3. It is not a Boca app instance and the tracking window happens to be in
  //    locked fullscreen mode.
  // 4. It is an oauth popup and the oauth operation has completed.
  //
  // The inverse checks below ensure we do not attempt to close the window if
  // they do not fall under any of the scenarios outlined above.
  if (browser == browser_) {
    // Same instance as the one being tracked. Skip close.
    return;
  }
  if (!browser_ &&
      ash::boca::OnTaskLockedController::From(&browser->GetBrowser())
          ->is_locked_for_on_task()) {
    // New instance that has been prepared for OnTask but is not being tracked
    // yet. Skip close because it is a managed instance.
    return;
  }
  if (browser->GetType() == ash::BrowserType::kAppPopup && oauth_in_progress_ &&
      browser == authorized_oauth_browser_) {
    // Authorized Oauth popup and oauth is still in progress. Skip close.
    return;
  }

  bool is_boca_app_instance =
      ash::IsBrowserForSystemWebApp(*browser, ash::SystemWebAppType::BOCA);

  if (browser_ &&
      !browser_->IsOnTaskState(ash::BrowserDelegate::OnTaskState::kLocked) &&
      !is_boca_app_instance) {
    // New instance that is not a Boca SWA instance and was spawned when the
    // Boca SWA instance being tracked is not in locked fullscreen mode. Skip
    // close.
    return;
  }
  if (!browser_ && !is_boca_app_instance) {
    // New instance that is not a Boca SWA instance and is spawned when there is
    // no Boca SWA instance being tracked. Skip close for now.
    return;
  }
  browser->Close();
}

void LockedSessionWindowTracker::MaybeCloseWebContents(
    base::WeakPtr<content::WebContents> weak_tab_ptr) {
  content::WebContents* const tab = weak_tab_ptr.get();
  if (!tab || tab->GetLastCommittedURL().is_valid() ||
      on_task_blocklist()->IsParentTab(tab)) {
    return;
  }
  if (browser_->GetWebContentsCount() > 1) {
    std::optional<size_t> index = browser_->GetIndexOfWebContents(tab);
    if (!index) {
      return;
    }
    on_task_blocklist()->RemoveChildFilter(tab);
    browser_->CloseWebContentsAt(*index,
                                 ash::BrowserDelegate::UserGesture::kNo);
  }
}

void LockedSessionWindowTracker::ObserveWebContents(
    content::WebContents* web_content) {
  Observe(web_content);
}

void LockedSessionWindowTracker::OnPauseModeChanged(bool paused) {
  DCHECK(browser_);
  if (on_task_pod_controller_) {
    on_task_pod_controller_->OnPauseModeChanged(paused);
  }

  // Immersive mode is disabled when in pause mode to ensure users cannot switch
  // tabs. We keep the window property always set to restore the window to its
  // previously intended state.
  browser_->GetNativeWindow()->SetProperty(
      chromeos::kUseImmersiveInTrustedPinned, !paused);
  BrowserView::GetBrowserViewForBrowser(&browser_->GetBrowser())
      ->FullscreenStateChanged();
}

void LockedSessionWindowTracker::set_can_start_navigation_throttle(
    bool is_ready) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  can_start_navigation_throttle_ = is_ready;
}

OnTaskBlocklist* LockedSessionWindowTracker::on_task_blocklist() {
  return on_task_blocklist_.get();
}

ash::BrowserDelegate* LockedSessionWindowTracker::browser() {
  return browser_;
}

bool LockedSessionWindowTracker::CanOpenNewPopup() {
  return can_open_new_popup_;
}

void LockedSessionWindowTracker::CleanupWindowTracker() {
  tab_observation_.Reset();
  active_tab_observer_.Observe(nullptr);
  if (on_task_blocklist_) {
    on_task_blocklist_->CleanupBlocklist();
  }
  on_task_pod_controller_.reset();

  browser_ = nullptr;
  can_open_new_popup_ = true;
  oauth_in_progress_ = false;
  authorized_oauth_browser_ = nullptr;
  identity_credential_source_for_testing_ = nullptr;

  for (auto& observer : observers_) {
    observer.OnWindowTrackerCleanedup();
    RemoveObserver(&observer);
  }

  if (ash::Shell::HasInstance()) {
    ash::Shell::Get()
        ->screen_pinning_controller()
        ->SetAllowWindowStackingWithPinnedWindow(false);
  }
}

void LockedSessionWindowTracker::NotifyActiveTabChanged(
    const std::u16string& title) {
  for (auto& observer : observers_) {
    observer.OnActiveTabChanged(title);
  }
}

void LockedSessionWindowTracker::ShowURLBlockedToast() {
  ash::boca::OnTaskNotificationsManager::ToastCreateParams toast_create_params(
      ash::boca::kOnTaskUrlBlockedToastId,
      ash::ToastCatalogName::kOnTaskUrlBlocked,
      /*text_description_callback=*/
      base::BindRepeating([](base::TimeDelta countdown_period) {
        return l10n_util::GetStringUTF16(
            IDS_ON_TASK_URL_BLOCKED_NOTIFICATION_MESSAGE);
      }));
  notifications_manager_->CreateToast(std::move(toast_create_params));
}

// LockedSessionWindowTracker::ActiveTabWebContentsObserver Implementation
LockedSessionWindowTracker::ActiveTabWebContentsObserver::
    ActiveTabWebContentsObserver(LockedSessionWindowTracker* tracker)
    : tracker_(tracker) {}

LockedSessionWindowTracker::ActiveTabWebContentsObserver::
    ~ActiveTabWebContentsObserver() = default;

void LockedSessionWindowTracker::ActiveTabWebContentsObserver::
    DidFinishNavigation(content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInPrimaryMainFrame() ||
      !navigation_handle->HasCommitted()) {
    return;
  }
  tracker_->RefreshUrlBlocklist();
  if (tracker_->on_task_pod_controller_) {
    tracker_->on_task_pod_controller_->OnPageNavigationContextChanged();
  }
  tracker_->NotifyActiveTabChanged(web_contents()->GetTitle());
}

void LockedSessionWindowTracker::ActiveTabWebContentsObserver::TitleWasSet(
    content::NavigationEntry* entry) {
  if (web_contents() && entry &&
      entry == web_contents()->GetController().GetLastCommittedEntry()) {
    tracker_->NotifyActiveTabChanged(web_contents()->GetTitle());
  }
}

ash::OnTaskPodController* LockedSessionWindowTracker::on_task_pod_controller() {
  if (!on_task_pod_controller_) {
    return nullptr;
  }
  return on_task_pod_controller_.get();
}

void LockedSessionWindowTracker::SetNotificationManagerForTesting(
    std::unique_ptr<ash::boca::OnTaskNotificationsManager>
        notifications_manager) {
  notifications_manager_ = std::move(notifications_manager);
}

void LockedSessionWindowTracker::SetIdentityCredentialSourceForTesting(
    content::webid::IdentityCredentialSource* source) {
  identity_credential_source_for_testing_ = source;
}

void LockedSessionWindowTracker::TriggerFedCmFederatedLoginCompletionForTesting(
    bool success) {
  OnFedCmFederatedLogin(success);
}

void LockedSessionWindowTracker::OnTabInserted(ash::BrowserDelegate* browser,
                                               content::WebContents* contents) {
  if (browser != browser_) {
    return;
  }
  const SessionID tab_id = sessions::SessionTabHelper::IdForTab(contents);
  const GURL url = contents->GetVisibleURL();
  SessionID parent_tab_id = SessionID::InvalidValue();
  content::WebContents* const opener =
      contents->GetFirstWebContentsInLiveOriginalOpenerChain();
  if (opener) {
    parent_tab_id = sessions::SessionTabHelper::IdForTab(opener);
  } else {
    content::WebContents* const active_contents =
        active_tab_observer_.web_contents();
    // When new tabs are added, if there is no active tab or it is the boca app
    // homepage, then we set `parent_tab_id` to be invalid.
    if (active_contents && (active_contents->GetVisibleURL() !=
                            GURL(ash::boca::kChromeBocaAppUntrustedIndexURL))) {
      parent_tab_id = sessions::SessionTabHelper::IdForTab(active_contents);
    }
  }
  for (auto& observer : observers_) {
    observer.OnTabAdded(parent_tab_id, tab_id, url);
  }
}

void LockedSessionWindowTracker::OnTabRemoved(ash::BrowserDelegate* browser,
                                              content::WebContents* contents,
                                              bool will_delete) {
  if (browser != browser_) {
    return;
  }
  if (contents == active_tab_observer_.web_contents()) {
    active_tab_observer_.Observe(nullptr);
  }
  on_task_blocklist()->RemoveParentFilter(contents);
  on_task_blocklist()->RemoveChildFilter(contents);
  const SessionID tab_id = sessions::SessionTabHelper::IdForTab(contents);
  for (auto& observer : observers_) {
    observer.OnTabRemoved(tab_id);
  }
}

void LockedSessionWindowTracker::OnActiveWebContentsChanged(
    ash::BrowserDelegate* browser,
    content::WebContents* old_contents,
    content::WebContents* new_contents) {
  if (browser != browser_) {
    return;
  }
  active_tab_observer_.Observe(new_contents);
  RefreshUrlBlocklist();
  if (on_task_pod_controller_) {
    on_task_pod_controller_->OnPageNavigationContextChanged();
  }
  NotifyActiveTabChanged(new_contents->GetTitle());
}

// ash::BrowserController::Observer Implementation
void LockedSessionWindowTracker::OnBrowserClosed(
    ash::BrowserDelegate* browser) {
  pending_close_tasks_.erase(browser);
  if (browser == browser_) {
    // Notify not in workbook when boca closed.
    NotifyActiveTabChanged(l10n_util::GetStringUTF16(IDS_NOT_IN_CLASS_TOOLS));
    CleanupWindowTracker();  // Will reset `browser_`.
  }
  if (browser->GetType() == ash::BrowserType::kAppPopup) {
    ash::Shell::Get()
        ->screen_pinning_controller()
        ->SetAllowWindowStackingWithPinnedWindow(false);
    can_open_new_popup_ = true;
    oauth_in_progress_ = false;
    authorized_oauth_browser_ = nullptr;
  }
}

void LockedSessionWindowTracker::OnBrowserCreated(
    ash::BrowserDelegate* browser) {
  if (browser->GetType() == ash::BrowserType::kAppPopup) {
    ash::Shell::Get()
        ->screen_pinning_controller()
        ->SetAllowWindowStackingWithPinnedWindow(true);
    // Since this is called after the window is created, but before we set the
    // pinning controller to allow the popup window to be on top of the
    // pinned window, we need to explicitly move this `browser` to be on top.
    // Otherwise, the popup window would still be beneath the pinned window.
    aura::Window* const top_container =
        ash::Shell::GetContainer(ash::Shell::GetPrimaryRootWindow(),
                                 ash::kShellWindowId_AlwaysOnTopContainer);
    top_container->StackChildAtTop(browser->GetNativeWindow());
    can_open_new_popup_ = false;
  } else {
    EnsureMaybeCloseBrowserTaskPosted(browser);
  }
}

void LockedSessionWindowTracker::OnBrowserActivated(
    ash::BrowserDelegate* browser) {
  if (!browser || !browser_) {
    return;
  }

  if (browser != browser_) {
    if (browser->GetType() == ash::BrowserType::kNormal &&
        browser != authorized_oauth_browser_ &&
        browser_->IsOnTaskState(ash::BrowserDelegate::OnTaskState::kLocked)) {
      aura::Window* const window = browser->GetNativeWindow();
      if (window) {
        std::unique_ptr<aura::WindowTracker> tracker =
            std::make_unique<aura::WindowTracker>();
        tracker->Add(window);
        base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE, base::BindOnce(
                           [](std::unique_ptr<aura::WindowTracker> tracker) {
                             if (!tracker->windows().empty()) {
                               aura::Window* w = tracker->windows()[0];
                               auto* window_state = ash::WindowState::Get(w);
                               if (window_state) {
                                 window_state->Minimize();
                               }
                             }
                           },
                           std::move(tracker)));
      }
    }
    NotifyActiveTabChanged(l10n_util::GetStringUTF16(IDS_NOT_IN_CLASS_TOOLS));
    return;
  }
  if (!browser_->GetActiveWebContents()) {
    return;
  }
  NotifyActiveTabChanged(browser_->GetActiveWebContents()->GetTitle());
}

// content::WebContentsObserver Impl
void LockedSessionWindowTracker::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  ash::BrowserDelegate* const browser =
      ash::BrowserController::GetInstance()->GetBrowserForTab(
          navigation_handle->GetWebContents());
  if (!browser || !browser_) {
    return;
  }
  if (browser == browser_) {
    content::WebContents* const tab = navigation_handle->GetWebContents();
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&LockedSessionWindowTracker::MaybeCloseWebContents,
                       weak_pointer_factory_.GetWeakPtr(), tab->GetWeakPtr()));
  }
}

void LockedSessionWindowTracker::DidFinishLoad(
    content::RenderFrameHost* render_frame_host,
    const GURL& validated_url) {
  if (!render_frame_host->IsInPrimaryMainFrame()) {
    return;
  }
  ash::BrowserDelegate* const browser =
      ash::BrowserController::GetInstance()->GetBrowserForTab(
          content::WebContents::FromRenderFrameHost(render_frame_host));
  if (!browser || !browser_) {
    return;
  }
  if (browser != browser_) {
    if (browser->GetType() == ash::BrowserType::kAppPopup) {
      // Verify if there are pending FedCM oauth requests for tracking purposes.
      content::webid::IdentityCredentialSource* const source =
          GetIdentityCredentialSource(render_frame_host->GetPage());
      if (source && source->HasPendingRequest()) {
        set_oauth_in_progress(true, browser);
      }
    }
    EnsureMaybeCloseBrowserTaskPosted(browser);
  }
}

void LockedSessionWindowTracker::OnFedCmFederatedLogin(bool success) {
  set_oauth_in_progress(false, nullptr);
  if (web_contents()) {
    ash::BrowserDelegate* const browser =
        ash::BrowserController::GetInstance()->GetBrowserForTab(web_contents());
    if (browser && browser != browser_) {
      // Attempt to close the oauth popup window now that the oauth flow has
      // completed.
      EnsureMaybeCloseBrowserTaskPosted(browser);
    }
  }
}

void LockedSessionWindowTracker::EnsureMaybeCloseBrowserTaskPosted(
    ash::BrowserDelegate* browser) {
  if (pending_close_tasks_.contains(browser)) {
    return;
  }
  auto task = std::make_unique<base::CancelableOnceClosure>(
      base::BindOnce(&LockedSessionWindowTracker::MaybeCloseBrowser,
                     weak_pointer_factory_.GetWeakPtr(), browser));
  pending_close_tasks_.emplace(browser, std::move(task));
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, pending_close_tasks_[browser]->callback());
}

content::webid::IdentityCredentialSource*
LockedSessionWindowTracker::GetIdentityCredentialSource(content::Page& page) {
  if (identity_credential_source_for_testing_) {
    return identity_credential_source_for_testing_.get();
  }
  return content::webid::IdentityCredentialSource::FromPage(page);
}
