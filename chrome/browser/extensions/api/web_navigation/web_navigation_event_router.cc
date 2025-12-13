// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/web_navigation/web_navigation_event_router.h"

#include "chrome/browser/extensions/api/web_navigation/frame_navigation_state.h"
#include "chrome/browser/extensions/api/web_navigation/web_navigation_api_helpers.h"
#include "chrome/browser/extensions/api/web_navigation/web_navigation_tab_observer.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "components/tabs/public/tab_interface.h"
#include "extensions/browser/extension_api_frame_id_map.h"
#include "extensions/browser/view_type_utils.h"
#include "extensions/common/mojom/view_type.mojom.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/scoped_multi_source_observation.h"
#include "chrome/browser/android/tab_android.h"  // nogncheck
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list_observer.h"
#include "chrome/browser/ui/android/tab_model/tab_model_observer.h"
#else
#include "chrome/browser/ui/browser_tab_strip_tracker.h"
#include "chrome/browser/ui/browser_tab_strip_tracker_delegate.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#endif

namespace extensions {

WebNavigationEventRouter::PendingWebContents::PendingWebContents() = default;
WebNavigationEventRouter::PendingWebContents::~PendingWebContents() = default;

void WebNavigationEventRouter::PendingWebContents::Set(
    int source_tab_id,
    int source_render_process_id,
    int source_extension_frame_id,
    content::WebContents* target_web_contents,
    const GURL& target_url,
    base::OnceCallback<void(content::WebContents*)> on_destroy) {
  Observe(target_web_contents);
  source_tab_id_ = source_tab_id;
  source_render_process_id_ = source_render_process_id;
  source_extension_frame_id_ = source_extension_frame_id;
  target_web_contents_ = target_web_contents;
  target_url_ = target_url;
  on_destroy_ = std::move(on_destroy);
}

void WebNavigationEventRouter::PendingWebContents::WebContentsDestroyed() {
  std::move(on_destroy_).Run(target_web_contents_.get());
  // |this| is deleted!
}

#if BUILDFLAG(IS_ANDROID)
// Android uses TabModel to track tabs.
class WebNavigationEventRouter::TabHelper : public TabModelListObserver,
                                            public TabModelObserver {
 public:
  TabHelper(WebNavigationEventRouter* router, Profile* profile)
      : router_(router), profile_(profile) {}
  TabHelper(const TabHelper&) = delete;
  TabHelper& operator=(const TabHelper&) = delete;
  ~TabHelper() override {
    tab_model_observations_.RemoveAllObservations();
    TabModelList::RemoveObserver(this);
  }

  void Init() {
    // Equivalent to observing for new windows (a TabModel is like a window).
    TabModelList::AddObserver(this);
    // Add models for existing windows.
    for (TabModel* model : TabModelList::models()) {
      OnTabModelAdded(model);
    }
  }

  // TabModelListObserver:
  void OnTabModelAdded(TabModel* tab_model) override {
    // Equivalent to a new window being added. Check if the window has a profile
    // we're tracking.
    if (!ShouldTrackModel(tab_model)) {
      return;
    }
    // Observe for new tabs being created.
    tab_model_observations_.AddObservation(tab_model);
    // Call TabAdded() on existing tabs.
    for (::tabs::TabInterface* tab : tab_model->GetAllTabs()) {
      if (tab && tab->GetContents()) {
        router_->TabAdded(tab->GetContents());
      }
    }
  }

  void OnTabModelRemoved(TabModel* tab_model) override {
    if (tab_model_observations_.IsObservingSource(tab_model)) {
      tab_model_observations_.RemoveObservation(tab_model);
    }
  }

  // TabModelObserver:
  void DidAddTab(TabAndroid* tab, TabModel::TabLaunchType type) override {
    if (tab->GetContents()) {
      router_->TabAdded(tab->GetContents());
    }
  }

  // TODO(crbug.com/371432404): Find a method to deal with "replace" updates
  // to tabs.

 private:
  // Returns true if we should track tabs in this model (window).
  bool ShouldTrackModel(TabModel* model) const {
    return profile_->IsSameOrParent(model->GetProfile());
  }

  raw_ptr<WebNavigationEventRouter> router_;
  raw_ptr<Profile> profile_;
  base::ScopedMultiSourceObservation<TabModel, TabModelObserver>
      tab_model_observations_{this};
};
#else
// Tab helper implementation for Win/Mac/Linux/Chrome OS.
class WebNavigationEventRouter::TabHelper
    : public TabStripModelObserver,
      public BrowserTabStripTrackerDelegate {
 public:
  TabHelper(WebNavigationEventRouter* router, Profile* profile)
      : router_(router),
        profile_(profile),
        browser_tab_strip_tracker_(this, this) {}
  TabHelper(const TabHelper&) = delete;
  TabHelper& operator=(const TabHelper&) = delete;
  ~TabHelper() override = default;

  void Init() { browser_tab_strip_tracker_.Init(); }

  // TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override {
    if (change.type() == TabStripModelChange::kReplaced) {
      auto* replace = change.GetReplace();
      router_->TabReplaced(replace->old_contents, replace->new_contents);
    } else if (change.type() == TabStripModelChange::kInserted) {
      for (auto& tab : change.GetInsert()->contents) {
        router_->TabAdded(tab.contents);
      }
    }
  }

  // BrowserTabStripTrackerDelegate:
  bool ShouldTrackBrowser(BrowserWindowInterface* browser) override {
    return profile_->IsSameOrParent(browser->GetProfile());
  }

  raw_ptr<WebNavigationEventRouter> router_;
  raw_ptr<Profile> profile_;
  BrowserTabStripTracker browser_tab_strip_tracker_;
};
#endif  // BUILDFLAG(IS_ANDROID)

WebNavigationEventRouter::WebNavigationEventRouter(Profile* profile)
    : profile_(profile),
      tab_helper_(std::make_unique<TabHelper>(this, profile_)) {
  tab_helper_->Init();
}

WebNavigationEventRouter::~WebNavigationEventRouter() = default;

void WebNavigationEventRouter::TabReplaced(content::WebContents* old_contents,
                                           content::WebContents* new_contents) {
  WebNavigationTabObserver* tab_observer =
      WebNavigationTabObserver::Get(old_contents);

  if (!tab_observer) {
    // If you hit this DCHECK(), please add reproduction steps to
    // http://crbug.com/109464.
    DCHECK(GetViewType(old_contents) != mojom::ViewType::kTabContents);
    return;
  }
  if (!FrameNavigationState::IsValidUrl(old_contents->GetLastCommittedURL()) ||
      !FrameNavigationState::IsValidUrl(new_contents->GetLastCommittedURL())) {
    return;
  }

  web_navigation_api_helpers::DispatchOnTabReplaced(old_contents, profile_,
                                                    new_contents);
}

void WebNavigationEventRouter::RecordNewWebContents(
    content::WebContents* source_web_contents,
    int source_render_process_id,
    int source_render_frame_id,
    GURL target_url,
    content::WebContents* target_web_contents,
    bool not_yet_in_tabstrip) {
  if (source_render_frame_id == 0) {
    return;
  }
  WebNavigationTabObserver* tab_observer =
      WebNavigationTabObserver::Get(source_web_contents);
  if (!tab_observer) {
    // If you hit this DCHECK(), please add reproduction steps to
    // http://crbug.com/109464.
    DCHECK(GetViewType(source_web_contents) != mojom::ViewType::kTabContents);
    return;
  }

  auto* frame_host = content::RenderFrameHost::FromID(source_render_process_id,
                                                      source_render_frame_id);
  auto* frame_navigation_state =
      FrameNavigationState::GetForCurrentDocument(frame_host);

  if (!frame_navigation_state || !frame_navigation_state->CanSendEvents()) {
    return;
  }

  int source_extension_frame_id =
      ExtensionApiFrameIdMap::GetFrameId(frame_host);
  int source_tab_id = ExtensionTabUtil::GetTabId(source_web_contents);

  // If the WebContents isn't yet inserted into a tab strip, we need to delay
  // the extension event until the WebContents is fully initialized.
  if (not_yet_in_tabstrip) {
    pending_web_contents_[target_web_contents].Set(
        source_tab_id, source_render_process_id, source_extension_frame_id,
        target_web_contents, target_url,
        base::BindOnce(&WebNavigationEventRouter::PendingWebContentsDestroyed,
                       base::Unretained(this)));
  } else {
    web_navigation_api_helpers::DispatchOnCreatedNavigationTarget(
        source_tab_id, source_render_process_id, source_extension_frame_id,
        target_web_contents->GetBrowserContext(), target_web_contents,
        target_url);
  }
}

void WebNavigationEventRouter::TabAdded(content::WebContents* tab) {
  auto iter = pending_web_contents_.find(tab);
  if (iter == pending_web_contents_.end()) {
    return;
  }

  const PendingWebContents& pending_tab = iter->second;
  web_navigation_api_helpers::DispatchOnCreatedNavigationTarget(
      pending_tab.source_tab_id(), pending_tab.source_render_process_id(),
      pending_tab.source_extension_frame_id(),
      pending_tab.target_web_contents()->GetBrowserContext(),
      pending_tab.target_web_contents(), pending_tab.target_url());
  pending_web_contents_.erase(iter);
}

void WebNavigationEventRouter::PendingWebContentsDestroyed(
    content::WebContents* tab) {
  pending_web_contents_.erase(tab);
}

}  // namespace extensions
