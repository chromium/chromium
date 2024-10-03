// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sessions/session_restore.h"

#include <stddef.h>

#include <algorithm>
#include <iterator>
#include <list>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/containers/flat_map.h"
#include "base/debug/alias.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_macros.h"
#include "base/observer_list.h"
#include "base/ranges/algorithm.h"
#include "base/run_loop.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/buildflags.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/profiles/keep_alive/profile_keep_alive_types.h"
#include "chrome/browser/profiles/keep_alive/scoped_profile_keep_alive.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/sessions/app_session_service.h"
#include "chrome/browser/sessions/app_session_service_factory.h"
#include "chrome/browser/sessions/session_restore_delegate.h"
#include "chrome/browser/sessions/session_service.h"
#include "chrome/browser/sessions/session_service_factory.h"
#include "chrome/browser/sessions/session_service_log.h"
#include "chrome/browser/sessions/session_service_lookup.h"
#include "chrome/browser/sessions/session_service_utils.h"
#include "chrome/browser/sessions/sessions_features.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_tabrestore.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/startup/startup_browser_creator.h"
#include "chrome/browser/ui/startup/startup_tab.h"
#include "chrome/browser/ui/startup/startup_types.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_keyed_service.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_service_factory.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_utils.h"
#include "chrome/browser/ui/tabs/tab_group.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_user_gesture_details.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/webui/whats_new/whats_new_util.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/common/extensions/extension_metrics.h"
#include "chrome/common/url_constants.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#include "components/saved_tab_groups/public/features.h"
#include "components/saved_tab_groups/public/saved_tab_group.h"
#include "components/saved_tab_groups/public/tab_group_sync_service.h"
#include "components/saved_tab_groups/public/types.h"
#include "components/sessions/core/session_types.h"
#include "components/tab_groups/tab_group_id.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/dom_storage_context.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/session_storage_namespace.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension_set.h"
#include "ui/base/mojom/window_show_state.mojom.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
#include "chrome/browser/ui/webui/whats_new/whats_new_fetcher.h"
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_features.h"
#include "ash/metrics/login_unlock_throughput_recorder.h"
#include "ash/shell.h"
#include "chrome/browser/ash/boot_times_recorder/boot_times_recorder.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "components/app_restore/window_properties.h"
#include "ui/aura/window_occlusion_tracker.h"
#include "ui/compositor/layer.h"
#include "ui/wm/core/scoped_animation_disabler.h"
#endif

using content::NavigationController;
using content::RenderWidgetHost;
using content::WebContents;
using RestoredTab = SessionRestoreDelegate::RestoredTab;

namespace {

bool HasSingleNewTabPage(Browser* browser) {
  if (browser->tab_strip_model()->count() != 1) {
    return false;
  }
  content::WebContents* active_tab =
      browser->tab_strip_model()->GetWebContentsAt(0);
  return active_tab->GetURL() == chrome::kChromeUINewTabURL ||
         search::IsInstantNTP(active_tab);
}

// Pointers to SessionRestoreImpls which are currently restoring the session.
std::set<SessionRestoreImpl*>* active_session_restorers = nullptr;

#if BUILDFLAG(IS_CHROMEOS_ASH)
void ReportRestoredWindowCreated(aura::Window* window) {
  // Ash is not always initialized in unit tests.
  if (!ash::Shell::HasInstance()) {
    return;
  }

  const int32_t restore_window_id =
      window->GetProperty(app_restore::kRestoreWindowIdKey);

  // Restored window IDs are always non-zero.
  if (restore_window_id == 0) {
    return;
  }

  ash::LoginUnlockThroughputRecorder* throughput_recorder =
      ash::Shell::Get()->login_unlock_throughput_recorder();
  throughput_recorder->OnRestoredWindowCreated(restore_window_id);
  aura::Window* root_window = window->GetRootWindow();
  if (root_window) {
    ui::Compositor* compositor = root_window->layer()->GetCompositor();
    throughput_recorder->OnBeforeRestoredWindowShown(restore_window_id,
                                                     compositor);
  }
}
#endif

}  // namespace

// SessionRestoreImpl ---------------------------------------------------------

// SessionRestoreImpl is responsible for fetching the set of tabs to create
// from SessionService. SessionRestoreImpl deletes itself when done.

class SessionRestoreImpl : public BrowserListObserver {
 public:
  SessionRestoreImpl(Profile* profile,
                     Browser* browser,
                     bool synchronous,
                     bool clobber_existing_tab,
                     bool always_create_tabbed_browser,
                     bool restore_apps,
                     bool restore_browser,
                     bool log_event,
                     const StartupTabs& startup_tabs)
      : profile_(profile),
        browser_(browser),
        synchronous_(synchronous),
        clobber_existing_tab_(clobber_existing_tab),
        always_create_tabbed_browser_(always_create_tabbed_browser),
        log_event_(log_event),
        restore_apps_(restore_apps),
        restore_browser_(restore_browser),
        startup_tabs_(startup_tabs),
        active_window_id_(SessionID::InvalidValue()),
        restore_started_(base::TimeTicks::Now()) {
    DCHECK(restore_browser_ || restore_apps_);

    if (active_session_restorers == nullptr) {
      active_session_restorers = new std::set<SessionRestoreImpl*>();
    }

    // Only one SessionRestoreImpl should be operating on the profile at the
    // same time.
    std::set<SessionRestoreImpl*>::iterator it;
    for (it = active_session_restorers->begin();
         it != active_session_restorers->end(); ++it) {
      if ((*it)->profile_ == profile) {
        break;
      }
    }
    DCHECK(it == active_session_restorers->end());

    active_session_restorers->insert(this);

    keep_alive_ = std::make_unique<ScopedKeepAlive>(
        KeepAliveOrigin::SESSION_RESTORE, KeepAliveRestartOption::DISABLED);
    profile_keep_alive_ = std::make_unique<ScopedProfileKeepAlive>(
        profile, ProfileKeepAliveOrigin::kBrowserWindow);
  }

  bool synchronous() const { return synchronous_; }

  Browser* Restore() {
    if (restore_browser_) {
      SessionServiceBase* service =
          SessionServiceFactory::GetForProfileForSessionRestore(profile_);
      CHECK(service);
      service->GetLastSession(base::BindOnce(&SessionRestoreImpl::OnGotSession,
                                             weak_factory_.GetWeakPtr(),
                                             /* for_apps */ false));
    }

    if (restore_apps_) {
      SessionServiceBase* app_service =
          AppSessionServiceFactory::GetForProfileForSessionRestore(profile_);
      CHECK(app_service);
      app_service->GetLastSession(base::BindOnce(
          &SessionRestoreImpl::OnGotSession, weak_factory_.GetWeakPtr(),
          /* for_apps */ true));

      // Ensure the registry is ready so that when we reopen apps they work
      // properly. If we don't wait, it's possible that apps are restored in
      // an incoherent state.
      web_app::WebAppProvider* provider =
          web_app::WebAppProvider::GetForLocalAppsUnchecked(profile_);
      DCHECK(provider);

      provider->on_registry_ready().Post(
          FROM_HERE, base::BindOnce(&SessionRestoreImpl::WebAppRegistryReady,
                                    weak_factory_.GetWeakPtr()));
    }

    if (synchronous_) {
      {
        base::RunLoop loop(base::RunLoop::Type::kNestableTasksAllowed);
        quit_closure_for_sync_restore_ = loop.QuitClosure();
        loop.Run();
        quit_closure_for_sync_restore_ = base::OnceClosure();
      }
      Browser* browser =
          ProcessSessionWindowsAndNotify(&windows_, active_window_id_);
      delete this;
      return browser;
    }

    if (browser_) {
      BrowserList::AddObserver(this);
    }

    return browser_;
  }

  // Restore window(s) from a foreign session. Returns newly created Browsers.
  std::vector<Browser*> RestoreForeignSession(
      std::vector<const sessions::SessionWindow*>::const_iterator begin,
      std::vector<const sessions::SessionWindow*>::const_iterator end) {
    std::vector<Browser*> browsers;
    std::vector<RestoredTab> restored_tabs;
    // Create a browser instance to put the restored tabs in.
    for (auto i = begin; i != end; ++i) {
      Browser* browser = CreateRestoredBrowser(
          BrowserTypeForWindowType((*i)->type), (*i)->bounds, (*i)->workspace,
          (*i)->visible_on_all_workspaces, (*i)->show_state, (*i)->app_name,
          (*i)->user_title, (*i)->extra_data, (*i)->window_id.id());
      browsers.push_back(browser);

      // A foreign session window will not contain tab groups, however an
      // instance is still required for RestoreTabsToBrowser.
      base::flat_map<tab_groups::TabGroupId, tab_groups::TabGroupId>
          new_group_ids;

      // Restore and show the browser.
      const int initial_tab_count = 0;
      bool did_show_browser = false;
      RestoreTabsToBrowser(*(*i), browser, /*is_active_browser=*/false,
                           initial_tab_count, restored_tabs, &new_group_ids,
                           did_show_browser);
      NotifySessionServiceOfRestoredTabs(browser, initial_tab_count);
    }

    // Always create in a new window.
    FinishedTabCreation(true, true, restored_tabs);

    SessionRestore::on_session_restored_callbacks()->Notify(
        profile_, static_cast<int>(restored_tabs.size()));

    return browsers;
  }

  // Restore a single tab from a foreign session.
  // Opens in the tab in the last active browser, unless disposition is
  // NEW_WINDOW, in which case the tab will be opened in a new browser. Returns
  // the WebContents of the restored tab.
  WebContents* RestoreForeignTab(const sessions::SessionTab& tab,
                                 WindowOpenDisposition disposition) {
    DCHECK(!tab.navigations.empty());
    int selected_index = tab.current_navigation_index;
    selected_index = std::max(
        0,
        std::min(selected_index, static_cast<int>(tab.navigations.size() - 1)));

    bool use_new_window = disposition == WindowOpenDisposition::NEW_WINDOW;

    Browser* browser =
        use_new_window ? Browser::Create(Browser::CreateParams(profile_, true))
                       : browser_.get();

    RecordAppLaunchForTab(browser, tab, selected_index);

    WebContents* web_contents;
    if (disposition == WindowOpenDisposition::CURRENT_TAB) {
      DCHECK(!use_new_window);
      web_contents = chrome::ReplaceRestoredTab(
          browser, tab.navigations, selected_index, tab.extension_app_id,
          nullptr, tab.user_agent_override, tab.extra_data,
          true /* from_session_restore */);
    } else {
      int tab_index =
          use_new_window ? 0 : browser->tab_strip_model()->active_index() + 1;
      web_contents = chrome::AddRestoredTab(
          browser, tab.navigations, tab_index, selected_index,
          tab.extension_app_id, std::nullopt,
          disposition == WindowOpenDisposition::NEW_FOREGROUND_TAB,  // selected
          tab.pinned, base::TimeTicks(), base::Time(), nullptr,
          tab.user_agent_override, tab.extra_data,
          /*from_session_restore=*/true,
          /*is_active_browser=*/std::nullopt);
      // Start loading the tab immediately.
      web_contents->GetController().LoadIfNecessary();
    }

    if (use_new_window) {
      browser->tab_strip_model()->ActivateTabAt(
          0, TabStripUserGestureDetails(
                 TabStripUserGestureDetails::GestureType::kOther));
      browser->window()->Show();
    }
    NotifySessionServiceOfRestoredTabs(browser,
                                       browser->tab_strip_model()->count());

    // Since FinishedTabCreation() is not called here, |this| will leak if we
    // are not in sychronous mode.
    DCHECK(synchronous_);

    SessionRestore::on_session_restored_callbacks()->Notify(profile_, 1);

    return web_contents;
  }

  SessionRestoreImpl(const SessionRestoreImpl&) = delete;
  SessionRestoreImpl& operator=(const SessionRestoreImpl&) = delete;

  ~SessionRestoreImpl() override {
    BrowserList::RemoveObserver(this);
    active_session_restorers->erase(this);
    if (active_session_restorers->empty()) {
      delete active_session_restorers;
      active_session_restorers = nullptr;
    }
  }

  // BrowserListObserver:
  void OnBrowserRemoved(Browser* browser) override {
    if (browser == browser_) {
      if (log_event_) {
        LogSessionServiceRestoreCanceledEvent(profile_);
      }
      delete this;
    }
  }

  Profile* profile() { return profile_; }

 private:
  // Invoked when done with creating all the tabs/browsers.
  //
  // |created_tabbed_browser| indicates whether a tabbed browser was created,
  // or we used an existing tabbed browser.
  //
  // If successful, this begins loading tabs and deletes itself when all tabs
  // have been loaded.
  //
  // Returns the Browser that was created, if any.
  Browser* FinishedTabCreation(bool succeeded,
                               bool created_tabbed_browser,
                               std::vector<RestoredTab>& restored_tabs) {
    Browser* browser = nullptr;
    if (!created_tabbed_browser && always_create_tabbed_browser_) {
      browser = Browser::Create(Browser::CreateParams(profile_, false));
      if (startup_tabs_.empty() ||
          (startup_tabs_.size() == 1 && whats_new::IsEnabled() &&
           startup_tabs_[0].url == whats_new::GetWebUIStartupURL())) {
        // No tab browsers were created and no URLs were supplied on the command
        // line, or only the What's New page is specified at startup and may or
        // may not add a tab. Open the new tab page.
        startup_tabs_.emplace_back(GURL(chrome::kChromeUINewTabURL));
      }
      AppendURLsToBrowser(browser, startup_tabs_);
      browser->window()->Show();
    }

    // Remove any tabs that have been deleted.
    std::erase_if(restored_tabs,
                  [](RestoredTab& tab) { return tab.contents() == nullptr; });

    if (succeeded) {
      // Sort the tabs in the order they should be restored, and start loading
      // them.
      std::stable_sort(restored_tabs.begin(), restored_tabs.end());
      SessionRestoreDelegate::RestoreTabs(restored_tabs, restore_started_);
    }

    if (!synchronous_) {
      // If we're not synchronous we need to delete ourself.
      // NOTE: we must use DeleteLater here as most likely we're in a callback
      // from the history service which doesn't deal well with deleting the
      // object it is notifying.
      base::SingleThreadTaskRunner::GetCurrentDefault()->DeleteSoon(FROM_HERE,
                                                                    this);

      // The delete may take a while and at this point we no longer care about
      // if the browser is deleted. Don't listen to anything. This avoid a
      // possible double delete too (if browser is closed before DeleteSoon() is
      // processed).
      BrowserList::RemoveObserver(this);
    }

#if BUILDFLAG(IS_CHROMEOS_ASH)
    ash::BootTimesRecorder::Get()->AddLoginTimeMarker("SessionRestore-End",
                                                      false);
#endif
    return browser;
  }

  // We typically want to restore windows in order of creation.
  void SortWindowsByWindowId() {
    std::sort(windows_.begin(), windows_.end(),
              [](const std::unique_ptr<sessions::SessionWindow>& lhs,
                 const std::unique_ptr<sessions::SessionWindow>& rhs) {
                return lhs->window_id < rhs->window_id;
              });
  }

  void OnGotSession(
      bool for_apps,
      std::vector<std::unique_ptr<sessions::SessionWindow>> windows,
      SessionID active_window_id,
      bool read_error) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    ash::BootTimesRecorder::Get()->AddLoginTimeMarker(
        "SessionRestore-GotSession", false);
#endif

    // This function could be called twice from both SessionService and
    // AppSessionService. If one of them returns error, then |read_error_| is
    // true. So check whether |read_error_| has been set as true to prevent the
    // result is overwritten.
    if (!read_error_) {
      read_error_ = read_error;
    }

    // Copy windows into windows_ so that we can combine both app and browser
    // windows together before doing a one-pass restore.
    base::ranges::move(windows, std::back_inserter(windows_));
    SessionRestore::OnGotSession(profile(), for_apps, windows.size());
    windows.clear();

    // Since we could now be possibly waiting for two |GetSession|s, we need
    // to track if we're completely finished or not. While we're at it,
    // store the windows for later merging and restoring.
    if (for_apps) {
      got_app_windows_ = true;
    } else {
      got_browser_windows_ = true;
    }

    // Don't let app restores set the active_window_id, or else it will
    // always bring the app to the forefront.
    if (!for_apps) {
      active_window_id_ = active_window_id;
    }

    ProcessSessionWindowsIfReady();
  }

  void WebAppRegistryReady() {
    DCHECK_EQ(web_app_registry_ready_, false);
    DCHECK(restore_apps_);
    web_app_registry_ready_ = true;

    ProcessSessionWindowsIfReady();
  }

  // This is called by callback handlers and may trigger session restore if
  // all the required conditions have been met. It also handles the differences
  // between async/sync restores.
  void ProcessSessionWindowsIfReady() {
    bool got_all_sessions = IsReadyToProcessSessionWindows();

    // For async restores, we need to early exit here if we aren't ready to
    // start restoring windows.
    if (!got_all_sessions) {
      return;
    }

    SortWindowsByWindowId();

    if (synchronous_) {
      CHECK(!quit_closure_for_sync_restore_.is_null());
      // now we know we have all the windows we need merge them into windows_
      // for processing.
      std::move(quit_closure_for_sync_restore_).Run();
      return;
    }

    ProcessSessionWindowsAndNotify(&windows_, active_window_id_);
  }

  // This helper, based on the restore_apps_ state tells us if we're ready to
  // begin restoring windows. There's two ways we are ready to process:
  // 1. we aren't restoring apps, and have browser_windows.
  // 2. we are restoring apps, we have both browser and app windows and
  //   WebAppRegistryReady() has been called.
  bool IsReadyToProcessSessionWindows() const {
    if (!restore_apps_) {
      return got_browser_windows_;
    }

    if (!restore_browser_) {
      return got_app_windows_ && web_app_registry_ready_;
    }

    return (got_app_windows_ && got_browser_windows_ &&
            web_app_registry_ready_);
  }

  Browser* ProcessSessionWindowsAndNotify(
      std::vector<std::unique_ptr<sessions::SessionWindow>>* windows,
      SessionID active_window_id) {
    int window_count = 0;
    int tab_count = 0;
    std::vector<RestoredTab> restored_tabs;
    Browser* result = ProcessSessionWindows(
        windows, active_window_id, restored_tabs, &window_count, &tab_count);
    if (log_event_) {
      LogSessionServiceRestoreEvent(profile_, window_count, tab_count,
                                    read_error_);
    }
    SessionRestore::on_session_restored_callbacks()->Notify(
        profile_, static_cast<int>(restored_tabs.size()));
    return result;
  }

  void PruneWindows(
      std::vector<std::unique_ptr<sessions::SessionWindow>>* windows) {
#if BUILDFLAG(IS_CHROMEOS)
    web_app::WebAppProvider* provider =
        web_app::WebAppProvider::GetForWebApps(profile_);
    if (!provider) {
      return;
    }

    windows->erase(
        base::ranges::remove_if(
            *windows,
            [provider](const std::unique_ptr<sessions::SessionWindow>& window)
                -> bool {
              // Windows that are auto-started and prevented from closing are
              // exempted from session restore.
              webapps::AppId app_id =
                  web_app::GetAppIdFromApplicationName(window->app_name);
              // Checking for close prevention does not require an `AppLock`
              // and therefore `registrar_unsafe()` is safe to use.
              return provider->registrar_unsafe().IsPreventCloseEnabled(app_id);
            }),
        windows->end());
#endif  // BUIDLFLAG(IS_CHROMEOS)
  }

  // Creates browsers for `windows` and returns the last tabbed browser or the
  // one needs to be activated. If there is no tabbed browser (e.g. only PWAs
  // open in the last session, see b/40275406) returns the last app browesr in
  // `windows`.
  Browser* ProcessSessionWindows(
      std::vector<std::unique_ptr<sessions::SessionWindow>>* windows,
      SessionID active_window_id,
      std::vector<RestoredTab>& restored_tabs,
      int* window_count,
      int* tab_count) {
    DVLOG(1) << "ProcessSessionWindows " << windows->size();

    PruneWindows(windows);

    if (windows->empty()) {
      // Restore was unsuccessful. The DOM storage system can also delete its
      // data, since no session restore will happen at a later point in time.
      profile_->GetDefaultStoragePartition()
          ->GetDOMStorageContext()
          ->StartScavengingUnusedSessionStorage();
      return FinishedTabCreation(false, false, restored_tabs);
    }

#if BUILDFLAG(IS_CHROMEOS_ASH)
    ash::BootTimesRecorder::Get()->AddLoginTimeMarker(
        "SessionRestore-CreatingTabs-Start", false);
#endif

    // After the for loop this contains the last TYPE_NORMAL browser, or nullptr
    // if no TYPE_NORMAL browser exists.
    Browser* last_normal_browser = nullptr;
    bool has_normal_browser = false;

    // After the for loop this contains the last TYPE_APP browser, or nullptr
    // if no TYPE_APP browser exists.
    Browser* last_app_browser = nullptr;

    // After the for loop, this contains the browser to activate, if one of the
    // windows has the same id as specified in active_window_id.
    Browser* browser_to_activate = nullptr;

    // Determine if there is a visible window, or if the active window exists.
    // Even if all windows are ui::mojom::WindowShowState::kMinimized, if one of
    // them is the active window it will be made visible by the call to
    // browser_to_activate->window()->Activate() later on in this method.
    bool has_visible_browser = false;
    for (const auto& window : *windows) {
      if (window->show_state != ui::mojom::WindowShowState::kMinimized ||
          window->window_id == active_window_id) {
        has_visible_browser = true;
      }
    }

#if BUILDFLAG(IS_CHROMEOS_ASH)
    // Pause occlusion tracking until all browser windows are created so that
    // their final occlusion state is used to trigger tab loading.
    // This is ash only because the final occlusion state is calculated
    // synchronously on ash.
    std::optional<aura::WindowOcclusionTracker::ScopedPause>
        pause_occlusion_tracking;

    // Disables window animations for created browser windows. Otherwise,
    // because occlusion states are not calculated for animating windows, all
    // restored browser windows are considered visible and triggers tab load.
    std::optional<
        std::map<aura::Window*, std::optional<wm::ScopedAnimationDisabler>>>
        window_animation_disablers;

    if (base::FeatureList::IsEnabled(
            ash::features::kAshSessionRestoreDeferOccludedActiveTabLoad)) {
      pause_occlusion_tracking.emplace();
      window_animation_disablers.emplace();
    }
#endif  //  BUILDFLAG(IS_CHROMEOS_ASH)

    for (const std::unique_ptr<sessions::SessionWindow>& window : *windows) {
      ++(*window_count);
      // 1. Choose between restoring tabs in an existing browser or in a newly
      //    created browser.
      Browser* browser = nullptr;
      const bool is_first_window = window.get() == windows->begin()->get();
      const bool is_normal_window =
          window->type == sessions::SessionWindow::TYPE_NORMAL;

      if (is_first_window && is_normal_window &&
          ShouldRestoreToExistingBrowser()) {
        // The first set of tabs is added to the existing browser.
        browser = browser_;
      } else {
#if BUILDFLAG(IS_CHROMEOS_ASH)
        ash::BootTimesRecorder::Get()->AddLoginTimeMarker(
            "SessionRestore-CreateRestoredBrowser-Start", false);
#endif
        // Change the initial show state of the created browser to
        // WindowShowState::kNormal if there are no visible browsers.
        ui::mojom::WindowShowState show_state = window->show_state;
        if (!has_visible_browser) {
          show_state = ui::mojom::WindowShowState::kNormal;
          has_visible_browser = true;
        }
        browser = CreateRestoredBrowser(
            BrowserTypeForWindowType(window->type), window->bounds,
            window->workspace, window->visible_on_all_workspaces, show_state,
            window->app_name, window->user_title, window->extra_data,
            window->window_id.id());

#if BUILDFLAG(IS_CHROMEOS_ASH)
        aura::Window* browser_window = browser->window()->GetNativeWindow();
        if (window_animation_disablers) {
          (*window_animation_disablers)[browser_window].emplace(browser_window);
        }

        ash::BootTimesRecorder::Get()->AddLoginTimeMarker(
            "SessionRestore-CreateRestoredBrowser-End", false);
        ReportRestoredWindowCreated(browser_window);
#endif
      }

      // 2. Track TYPE_NORMAL browsers.
      if (is_normal_window) {
        has_normal_browser = true;
        last_normal_browser = browser;
        browser->SetWindowUserTitle(window->user_title);
      }

      // Track TYPE_APP browsers.
      if (window->type == sessions::SessionWindow::TYPE_APP) {
        last_app_browser = browser;
      }

      // 3. Determine whether the currently active tab should be closed.
      WebContents* active_tab =
          browser->tab_strip_model()->GetActiveWebContents();
      int initial_tab_count = browser->tab_strip_model()->count();
      bool close_active_tab = clobber_existing_tab_ && is_first_window &&
                              is_normal_window && active_tab &&
                              browser == browser_ && !window->tabs.empty();
      if (close_active_tab) {
        --initial_tab_count;
      }

      if (window->window_id == active_window_id) {
        browser_to_activate = browser;
      }

      // 5. Restore tabs in |browser|. This will also call Show() on |browser|
      //    if its initial show state is not mimimized.
      // For the cases that users have more than one desk, a window is restored
      // to its parent desk, which can be non-active desk, and left invisible
      // but unminimized.
      base::flat_map<tab_groups::TabGroupId, tab_groups::TabGroupId>
          new_group_ids;
      bool did_show_browser = false;
      RestoreTabsToBrowser(*window, browser, initial_tab_count,
                           browser == browser_to_activate, restored_tabs,
                           &new_group_ids, did_show_browser);
      // Newly created browsers should be shown by RestoreTabsToBrowser. If they
      // aren't shown, they are likely to be never shown.
      if (browser != browser_) {
        DCHECK(did_show_browser);
      }

      (*tab_count) += (browser->tab_strip_model()->count() - initial_tab_count);

      // 6. Tabs will be grouped appropriately in RestoreTabsToBrowser. Now
      //    restore the groups' visual data.
      //    Note, this may delete some of the WebContents created earlier.
      RestoreTabGroupMetadata(browser, new_group_ids, window->tab_groups);

      // 7. Notify SessionService of restored tabs, so they can be saved to the
      //    current session.
      // TODO(fdoray): This seems redundant with the call to
      // SessionService::TabRestored() at the end of chrome::AddRestoredTab().
      // Consider removing it.
      NotifySessionServiceOfRestoredTabs(browser, initial_tab_count);

      // 8. Close the tab that was active in the window prior to session
      //    restore, if needed.
      if (close_active_tab) {
        chrome::CloseWebContents(browser, active_tab, true);
      }

      // Sanity check: A restored browser should have an active tab.
      // TODO(crbug.com/40662817): Change to DCHECK once we understand
      // why some browsers don't have an active tab on startup.
      CHECK(browser->tab_strip_model()->GetActiveWebContents());
    }

    if (browser_to_activate && browser_to_activate->is_type_normal()) {
      last_normal_browser = browser_to_activate;
    }

    if (last_normal_browser && !startup_tabs_.empty()) {
      browser_to_activate = OpenStartupUrls(last_normal_browser, startup_tabs_);
    }

#if BUILDFLAG(IS_CHROMEOS_ASH)
    ash::BootTimesRecorder::Get()->AddLoginTimeMarker(
        "SessionRestore-CreatingTabs-End", false);
#endif
    if (browser_to_activate) {
      browser_to_activate->window()->Activate();
    }

    // If last_normal_browser is NULL and startup_tabs_ is non-empty,
    // FinishedTabCreation will create a new TabbedBrowser and add the urls to
    // it.
    Browser* const finished_browser =
        FinishedTabCreation(true, has_normal_browser, restored_tabs);
    if (finished_browser) {
      last_normal_browser = finished_browser;
    }

    // sessionStorages needed for the session restore have now been recreated
    // by RestoreTab. Now it's safe for the DOM storage system to start
    // deleting leftover data.
    profile_->GetDefaultStoragePartition()
        ->GetDOMStorageContext()
        ->StartScavengingUnusedSessionStorage();

    // Cookies needed for session restore have been loaded and their last
    // accessed time has been updated. Now it's safe for the CookieManager to
    // delete stale session cookies not used in the past 7 days.
    // See crbug.com/40285083 for more info.
    if (base::FeatureList::IsEnabled(kDeleteStaleSessionCookiesOnStartup)) {
      profile_->GetDefaultStoragePartition()
          ->DeleteStaleSessionOnlyCookiesAfterDelay();
    }

    return last_normal_browser ? last_normal_browser : last_app_browser;
  }

  // Record an app launch event (if appropriate) for a tab which is about to
  // be restored. Callers should ensure that selected_index is within the
  // bounds of tab.navigations before calling.
  void RecordAppLaunchForTab(Browser* browser,
                             const sessions::SessionTab& tab,
                             int selected_index) {
    DCHECK(selected_index >= 0 &&
           selected_index < static_cast<int>(tab.navigations.size()));
    GURL url = tab.navigations[selected_index].virtual_url();
    const extensions::Extension* extension =
        extensions::ExtensionRegistry::Get(profile())
            ->enabled_extensions()
            .GetAppByURL(url);
    if (extension) {
      extensions::RecordAppLaunchType(
          extension_misc::APP_LAUNCH_SESSION_RESTORE, extension->GetType());
    }
  }

  // Adds the tabs from |window| to |browser|. Normal tabs go after the existing
  // tabs but pinned tabs will be pushed in front.
  // If there are no existing tabs, the tab at |window.selected_tab_index| will
  // be selected. Otherwise, the tab selection will remain untouched.
  void RestoreTabsToBrowser(
      const sessions::SessionWindow& window,
      Browser* browser,
      int initial_tab_count,
      bool is_active_browser,
      std::vector<RestoredTab>& restored_tabs,
      base::flat_map<tab_groups::TabGroupId, tab_groups::TabGroupId>*
          new_group_ids,
      bool& did_show_browser) {
    DVLOG(1) << "RestoreTabsToBrowser " << window.tabs.size();
    // TODO(crbug.com/40662817): Change to DCHECK once we understand
    // why some browsers don't have an active tab on startup.
    CHECK(!window.tabs.empty());

    // TODO(crbug.com/41440719): Check that tab groups are contiguous in
    // |window| to ensure tabs will not be reordered when restoring. This is not
    // possible yet due the ordering of TabStripModelObserver notifications in
    // an edge case.

    const int selected_tab_index = std::clamp(
        window.selected_tab_index, 0, static_cast<int>(window.tabs.size() - 1));

    const base::Time epoch_time = base::Time::UnixEpoch();
    const base::TimeTicks epoch_time_ticks = base::TimeTicks::UnixEpoch();
    for (int i = 0; i < static_cast<int>(window.tabs.size()); ++i) {
      const sessions::SessionTab& tab = *(window.tabs[i]);

      // Loads are scheduled for each restored tab unless the tab is going to
      // be selected as ShowBrowser() will load the selected tab.
      bool is_selected_tab =
          (initial_tab_count == 0) && (i == selected_tab_index);

      // Convert the last active time because WebContents needs a TimeTicks.
      const base::TimeDelta delta = tab.last_active_time - epoch_time;
      const base::TimeTicks last_active_time_ticks = epoch_time_ticks + delta;

      // If the browser already has tabs, we want to restore the new ones after
      // the existing ones. E.g. this happens in Win8 Metro where we merge
      // windows or when launching a hosted app from the app launcher.
      int tab_index = i + initial_tab_count;
      RestoreTab(tab, browser, is_active_browser, restored_tabs, new_group_ids,
                 tab_index, is_selected_tab, last_active_time_ticks,
                 tab.last_active_time, did_show_browser);
    }
  }

  // |tab_index| is ignored for pinned tabs which will always be pushed behind
  // the last existing pinned tab. If |is_selected_tab| is true the tab will be
  // shown after loading, otherwise it's loaded hidden. |last_active_time| is
  // the value to use to set the last time the WebContents was made active.
  void RestoreTab(const sessions::SessionTab& tab,
                  Browser* browser,
                  bool is_active_browser,
                  std::vector<RestoredTab>& restored_tabs,
                  base::flat_map<tab_groups::TabGroupId,
                                 tab_groups::TabGroupId>* new_group_ids,
                  const int tab_index,
                  bool is_selected_tab,
                  base::TimeTicks last_active_time_ticks,
                  base::Time last_active_time,
                  bool& did_show_browser) {
    // It's possible (particularly for foreign sessions) to receive a tab
    // without valid navigations. In that case, just skip it.
    // See crbug.com/154129.
    if (tab.navigations.empty()) {
      return;
    }

    SessionRestore::NotifySessionRestoreStartedLoadingTabs();
    int selected_index = GetNavigationIndexToSelect(tab);

    RecordAppLaunchForTab(browser, tab, selected_index);

    // Associate sessionStorage (if any) to the restored tab.
    scoped_refptr<content::SessionStorageNamespace> session_storage_namespace;
    if (!tab.session_storage_persistent_id.empty()) {
      session_storage_namespace =
          profile_->GetDefaultStoragePartition()
              ->GetDOMStorageContext()
              ->RecreateSessionStorage(tab.session_storage_persistent_id);
    }

    // Relabel group IDs to prevent duplicating groups. See crbug.com/1202102.
    std::optional<tab_groups::TabGroupId> new_group;
    if (tab.group) {
      auto it = new_group_ids->find(*tab.group);
      if (it == new_group_ids->end()) {
        it = new_group_ids
                 ->emplace(*tab.group, tab_groups::TabGroupId::GenerateNew())
                 .first;
      }
      new_group = it->second;
    }

    // Apply the stored group.
    WebContents* web_contents = chrome::AddRestoredTab(
        browser, tab.navigations, tab_index, selected_index,
        tab.extension_app_id, new_group, is_selected_tab, tab.pinned,
        last_active_time_ticks, last_active_time,
        session_storage_namespace.get(), tab.user_agent_override,
        tab.extra_data, true /* from_session_restore */, is_active_browser);
    DCHECK(web_contents);

    RestoredTab restored_tab(web_contents, is_selected_tab,
                             tab.extension_app_id.empty(), tab.pinned,
                             new_group);
    restored_tabs.push_back(restored_tab);

    // If this isn't the selected tab, there's nothing else to do.
    if (!is_selected_tab) {
      return;
    }

    if (browser != browser_) {
      did_show_browser = true;
    }
    ShowBrowser(browser, browser->tab_strip_model()->GetIndexOfWebContents(
                             web_contents));
  }

  void RestoreTabGroupMetadata(
      const Browser* browser,
      const base::flat_map<tab_groups::TabGroupId, tab_groups::TabGroupId>&
          new_group_ids,
      const std::vector<std::unique_ptr<sessions::SessionTabGroup>>&
          tab_groups) {
    if (!browser->tab_strip_model()->SupportsTabGroups()) {
      return;
    }

    for (const std::unique_ptr<sessions::SessionTabGroup>& session_tab_group :
         tab_groups) {
      TabGroup* model_tab_group =
          browser->tab_strip_model()->group_model()->GetTabGroup(
              new_group_ids.at(session_tab_group->id));
      CHECK(model_tab_group);
      model_tab_group->SetVisualData(session_tab_group->visual_data);

      ProcessSavedGroup(browser->profile(),
                        new_group_ids.at(session_tab_group->id),
                        session_tab_group->saved_guid);
    }
  }

  void ProcessSavedGroup(Profile* profile,
                         const tab_groups::LocalTabGroupID& local_id,
                         std::optional<std::string> sync_id) {
    tab_groups::TabGroupSyncService* service =
        tab_groups::SavedTabGroupUtils::GetServiceForProfile(profile);
    CHECK(service);

    if (sync_id) {
      const base::Uuid& sync_guid = base::Uuid::ParseLowercase(sync_id.value());
      service->ConnectLocalTabGroup(
          sync_guid, local_id,
          tab_groups::OpeningSource::kConnectOnSessionRestore);
    } else if (tab_groups::IsTabGroupsSaveV2Enabled()) {
      // Default save any groups that are not saved yet. This happens when
      // a user goes from V1 of SavedTabGroups to V2 through an update.
      service->SaveGroup(
          tab_groups::SavedTabGroupUtils::CreateSavedTabGroupFromLocalId(
              local_id));
    }
  }

  Browser* CreateRestoredBrowser(
      Browser::Type type,
      gfx::Rect bounds,
      const std::string& workspace,
      bool visible_on_all_workspaces,
      ui::mojom::WindowShowState show_state,
      const std::string& app_name,
      const std::string& user_title,
      const std::map<std::string, std::string>& extra_data,
      int32_t restore_id) {
    Browser::CreateParams params(type, profile_, false);
    params.initial_bounds = bounds;
    params.user_title = user_title;

    // We only store trusted app windows, so we also create them as trusted.
    if (type == Browser::Type::TYPE_APP) {
      params = Browser::CreateParams::CreateForApp(
          app_name, /*trusted_source=*/true, bounds, profile_,
          /*user_gesture=*/false);
    } else if (type == Browser::Type::TYPE_APP_POPUP) {
      params = Browser::CreateParams::CreateForAppPopup(
          app_name, /*trusted_source=*/true, bounds, profile_,
          /*user_gesture=*/false);
    }

#if BUILDFLAG(IS_CHROMEOS)
    params.restore_id = restore_id;
#endif

    params.initial_show_state = show_state;
    params.initial_workspace = workspace;
    params.initial_visible_on_all_workspaces_state = visible_on_all_workspaces;
    params.creation_source = Browser::CreationSource::kSessionRestore;
    Browser* browser = Browser::Create(params);

    return browser;
  }

  void ShowBrowser(Browser* browser, int selected_tab_index) {
    DCHECK(browser);
    DCHECK(browser->tab_strip_model()->count());
    browser->tab_strip_model()->ActivateTabAt(
        selected_tab_index,
        TabStripUserGestureDetails(
            TabStripUserGestureDetails::GestureType::kOther));

    if (browser_ == browser) {
      return;
    }

    browser->window()->Show();
    browser->set_is_session_restore(false);
  }

  // Appends the urls in |startup_tabs| to |browser|.
  void AppendURLsToBrowser(Browser* browser, const StartupTabs& startup_tabs) {
    bool is_first_tab = true;
    for (const auto& startup_tab : startup_tabs) {
      const GURL& url = startup_tab.url;
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
      if (url == whats_new::GetWebUIStartupURL()) {
        whats_new::StartWhatsNewFetch(browser);
        continue;
      }
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
      int add_types = AddTabTypes::ADD_FORCE_INDEX;
      if (is_first_tab) {
        add_types |= AddTabTypes::ADD_ACTIVE;
      }
      NavigateParams params(browser, url, ui::PAGE_TRANSITION_AUTO_TOPLEVEL);
      params.disposition = is_first_tab
                               ? WindowOpenDisposition::NEW_FOREGROUND_TAB
                               : WindowOpenDisposition::NEW_BACKGROUND_TAB;
      params.tabstrip_add_types = add_types;
      params.suggested_system_entropy = blink::mojom::SystemEntropy::kHigh;
      is_first_tab = false;
      Navigate(&params);
    }
  }

  // Normally opens |startup_tabs| in |last_normal_browser|, but if there are
  // urls set from LAST_AND_URLS startup pref, those are opened in a new
  // browser. Returns the browser to activate.
  Browser* OpenStartupUrls(Browser* last_normal_browser,
                           const StartupTabs& startup_tabs) {
    Browser* browser_to_activate = last_normal_browser;
    StartupTabs normal_startup_tabs, startup_tabs_from_last_and_urls_pref;
    for (const StartupTab& startup_tab : startup_tabs) {
      if (startup_tab.type == StartupTab::Type::kFromLastAndUrlsStartupPref) {
        startup_tabs_from_last_and_urls_pref.push_back(startup_tab);
      } else {
        normal_startup_tabs.push_back(startup_tab);
      }
    }
    if (last_normal_browser && !normal_startup_tabs.empty()) {
      AppendURLsToBrowser(last_normal_browser, normal_startup_tabs);
    }
    if (!startup_tabs_from_last_and_urls_pref.empty()) {
      Browser::CreateParams params =
          Browser::CreateParams(profile_, /*user_gesture*/ false);
      params.creation_source = Browser::CreationSource::kLastAndUrlsStartupPref;
      Browser* new_browser = Browser::Create(params);
      AppendURLsToBrowser(new_browser, startup_tabs_from_last_and_urls_pref);
      new_browser->window()->Show();
      browser_to_activate = new_browser;
    }
    return browser_to_activate;
  }

  // Invokes TabRestored on the SessionService for all tabs in browser after
  // initial_count.
  void NotifySessionServiceOfRestoredTabs(Browser* browser, int initial_count) {
    SessionServiceBase* service =
        GetAppropriateSessionServiceForProfile(browser);
    if (!service) {
      return;
    }
    TabStripModel* tab_strip = browser->tab_strip_model();
    for (int i = initial_count; i < tab_strip->count(); ++i) {
      service->TabRestored(tab_strip->GetWebContentsAt(i),
                           tab_strip->IsTabPinned(i));
    }
  }

  // Returns true if the first set of tabs should be restored the Browser
  // supplied to the constructor.
  bool ShouldRestoreToExistingBrowser() const {
    // Assume that if the window is not-visible the browser is about to
    // be deleted. This is necessitated by browser destruction first hiding
    // the window, and then asynchronously deleting it.
    return browser_ && browser_->is_type_normal() &&
           !browser_->profile()->IsOffTheRecord() &&
           browser_->window()->IsVisible();
  }

  // The profile to create the sessions for.
  raw_ptr<Profile> profile_;

  // The first browser to restore to, may be null.
  raw_ptr<Browser, DanglingUntriaged> browser_;

  // Whether or not restore is synchronous.
  const bool synchronous_;

  // The quit-closure to terminate the nested message-loop started for
  // synchronous session-restore.
  base::OnceClosure quit_closure_for_sync_restore_;

  // See description of CLOBBER_CURRENT_TAB.
  const bool clobber_existing_tab_;

  // If true and there is an error or there are no windows to restore, we
  // create a tabbed browser anyway. This is used on startup to make sure at
  // at least one window is created.
  const bool always_create_tabbed_browser_;

  // If true, LogSessionServiceRestoreEvent() is called after restore.
  const bool log_event_;

  // If true, restores apps.
  const bool restore_apps_;

  // If true, restores the normal browser.
  bool restore_browser_ = true;

  // App restores depend on web_app::WebAppProvider on_registry_ready(). This
  // bool will track that and hold up restores until that's ready too if apps
  // are being restored.
  bool web_app_registry_ready_ = false;

  // During app restores, we make two GetLastSession calls to
  // [App]SessionService, we need to wait till they both return before
  // processing the windows together in one pass.
  bool got_app_windows_ = false;
  bool got_browser_windows_ = false;

  // Set of URLs to open in addition to those restored from the session.
  StartupTabs startup_tabs_;

  // When synchronous we run a nested run loop. To avoid creating windows
  // from the nested run loop (which can make exiting the nested message
  // loop take a while) we cache the SessionWindows here and create the actual
  // windows when the nested run loop exits.
  std::vector<std::unique_ptr<sessions::SessionWindow>> windows_;
  SessionID active_window_id_;

  // When asynchronous it's possible for there to be no windows. To make sure
  // Chrome doesn't prematurely exit we register a KeepAlive for the lifetime
  // of this object.
  std::unique_ptr<ScopedKeepAlive> keep_alive_;

  // Same as |keep_alive_|, but also prevent |profile_| from getting deleted
  // (when DestroyProfileOnBrowserClose is enabled).
  std::unique_ptr<ScopedProfileKeepAlive> profile_keep_alive_;

  // The time we started the restore.
  base::TimeTicks restore_started_;

  // Set to true if reading the last commands encountered an error.
  bool read_error_ = false;

  base::WeakPtrFactory<SessionRestoreImpl> weak_factory_{this};
};

// SessionRestore -------------------------------------------------------------

// static
Browser* SessionRestore::RestoreSession(
    Profile* profile,
    Browser* browser,
    SessionRestore::BehaviorBitmask behavior,
    const StartupTabs& startup_tabs) {
#if DCHECK_IS_ON()
  // Profiles that are locked because they require signin should not be
  // restored.
  if (g_browser_process->profile_manager()) {
    ProfileAttributesEntry* entry =
        g_browser_process->profile_manager()
            ->GetProfileAttributesStorage()
            .GetProfileAttributesWithPath(profile->GetPath());
    DCHECK(!entry || !entry->IsSigninRequired());
  }
#endif
#if BUILDFLAG(IS_CHROMEOS_ASH)
  ash::BootTimesRecorder::Get()->AddLoginTimeMarker("SessionRestore-Start",
                                                    false);
#endif
  DCHECK(profile);
  DCHECK(SessionServiceFactory::GetForProfile(profile));
  profile->set_restored_last_session(true);

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  if (!profile->IsMainProfile()) {
    behavior &= ~RESTORE_APPS;
  }
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

  LogSessionServiceRestoreInitiatedEvent(profile, (behavior & SYNCHRONOUS) != 0,
                                         (behavior & RESTORE_BROWSER) != 0);

  // SessionRestoreImpl takes care of deleting itself when done.
  SessionRestoreImpl* restorer = new SessionRestoreImpl(
      profile, browser, (behavior & SYNCHRONOUS) != 0,
      (behavior & CLOBBER_CURRENT_TAB) != 0,
      (behavior & ALWAYS_CREATE_TABBED_BROWSER) != 0,
      (behavior & RESTORE_APPS) != 0, (behavior & RESTORE_BROWSER) != 0,
      /* log_event */ true, startup_tabs);
  return restorer->Restore();
}

// static
void SessionRestore::RestoreSessionAfterCrash(Browser* browser) {
  auto* profile = browser->profile();

// While this behavior is enabled for ash, it is explicitly disabled for lacros.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Desks restore a window to the right desk, so we should not reuse any
  // browser window. Otherwise, the conflict of the parent desk arises because
  // tabs created in this |browser| should remain in the current active desk,
  // but the first restored window should be restored to its saved parent desk
  // before a crash. This also avoids users' confusion of the current window
  // disappearing from the current desk after pressing a restore button.
  browser = nullptr;
#endif

  SessionRestore::BehaviorBitmask behavior =
      SessionRestore::RESTORE_BROWSER |
      (browser && HasSingleNewTabPage(browser)
           ? SessionRestore::CLOBBER_CURRENT_TAB
           : 0);

#if !BUILDFLAG(IS_CHROMEOS_ASH)
  // Apps should always be restored on crash restore except on Chrome OS. In
  // Chrome OS, apps are restored by full restore only. This function is called
  // when the chrome browser is launched after crash, so only browser restored,
  // apps are not restored in Chrome OS.
  behavior |= SessionRestore::RESTORE_APPS;
#endif
  SessionRestore::RestoreSession(profile, browser, behavior, StartupTabs());
}

// static
void SessionRestore::OpenStartupPagesAfterCrash(Browser* browser) {
  WebContents* tab_to_clobber = nullptr;
  if (HasSingleNewTabPage(browser)) {
    tab_to_clobber = browser->tab_strip_model()->GetActiveWebContents();
  }

  StartupBrowserCreator::OpenStartupPages(
      browser, chrome::startup::IsProcessStartup::kYes);
  if (tab_to_clobber && browser->tab_strip_model()->count() > 1) {
    chrome::CloseWebContents(browser, tab_to_clobber, true);
  }
}

// static
std::vector<Browser*> SessionRestore::RestoreForeignSessionWindows(
    Profile* profile,
    std::vector<const sessions::SessionWindow*>::const_iterator begin,
    std::vector<const sessions::SessionWindow*>::const_iterator end) {
  StartupTabs startup_tabs;
  SessionRestoreImpl restorer(
      profile, static_cast<Browser*>(nullptr), true, false, true,
      /* restore_apps */ false, /* restore_browser */ true,
      /* log_event */ false, startup_tabs);
  return restorer.RestoreForeignSession(begin, end);
}

// static
WebContents* SessionRestore::RestoreForeignSessionTab(
    content::WebContents* source_web_contents,
    const sessions::SessionTab& tab,
    WindowOpenDisposition disposition,
    bool skip_renderer_creation) {
  Browser* browser = chrome::FindBrowserWithTab(source_web_contents);
  Profile* profile = browser->profile();
  StartupTabs startup_tabs;
  SessionRestoreImpl restorer(profile, browser, true, false, false,
                              /* restore_apps */ false,
                              /* restore_browser */ true,
                              /* log_event */ false, startup_tabs);
  return restorer.RestoreForeignTab(tab, disposition);
}

// static
bool SessionRestore::IsRestoring(const Profile* profile) {
  if (active_session_restorers == nullptr) {
    return false;
  }
  for (SessionRestoreImpl* const active_session_restorer :
       *active_session_restorers) {
    if (active_session_restorer->profile() == profile) {
      return true;
    }
  }
  return false;
}

// static
bool SessionRestore::IsRestoringSynchronously() {
  if (!active_session_restorers) {
    return false;
  }
  for (const SessionRestoreImpl* const active_session_restorer :
       *active_session_restorers) {
    if (active_session_restorer->synchronous()) {
      return true;
    }
  }
  return false;
}

// static
base::CallbackListSubscription
SessionRestore::RegisterOnSessionRestoredCallback(
    const RestoredCallback& callback) {
  return on_session_restored_callbacks()->Add(callback);
}

// static
void SessionRestore::AddObserver(SessionRestoreObserver* observer) {
  observers()->AddObserver(observer);
}

// static
void SessionRestore::RemoveObserver(SessionRestoreObserver* observer) {
  observers()->RemoveObserver(observer);
}

// static
void SessionRestore::OnTabLoaderFinishedLoadingTabs() {
  if (!session_restore_started_) {
    return;
  }

  session_restore_started_ = false;
  for (auto& observer : *observers()) {
    observer.OnSessionRestoreFinishedLoadingTabs();
  }
}

// static
void SessionRestore::NotifySessionRestoreStartedLoadingTabs() {
  if (session_restore_started_) {
    return;
  }

  session_restore_started_ = true;
  for (auto& observer : *observers()) {
    observer.OnSessionRestoreStartedLoadingTabs();
  }
}

// static
void SessionRestore::OnWillRestoreTab(content::WebContents* web_contents) {
  for (auto& observer : *observers()) {
    observer.OnWillRestoreTab(web_contents);
  }
}

// static
void SessionRestore::OnGotSession(Profile* profile,
                                  bool for_apps,
                                  int window_count) {
  for (auto& observer : *observers()) {
    observer.OnGotSession(profile, for_apps, window_count);
  }
}

// static
SessionRestore::CallbackList* SessionRestore::on_session_restored_callbacks_ =
    nullptr;

// static
base::ObserverList<SessionRestoreObserver>::Unchecked*
    SessionRestore::observers_ = nullptr;

// static
bool SessionRestore::session_restore_started_ = false;
