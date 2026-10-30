// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/active_task_context_provider_impl.h"

#include <algorithm>
#include <map>

#include "base/feature_list.h"
#include "base/memory/raw_ref.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/contextual_search/contextual_search_web_contents_helper.h"
#include "chrome/browser/contextual_tasks/active_task_context_provider.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_panel_controller.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_web_contents_user_data.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/tab_list/tab_list_interface_observer.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/common/webui_url_constants.h"
#include "components/contextual_search/contextual_search_session_handle.h"
#include "components/contextual_search/contextual_search_types.h"
#include "components/contextual_tasks/public/context_decoration_params.h"
#include "components/contextual_tasks/public/contextual_task.h"
#include "components/contextual_tasks/public/contextual_task_context.h"
#include "components/contextual_tasks/public/features.h"
#include "components/omnibox/common/composebox_features.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/sessions/core/session_id.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/page.h"
#include "content/public/common/url_constants.h"

namespace contextual_tasks {

namespace {

std::set<tabs::TabHandle> GetTabsFromContext(
    const ContextualTaskContext& context,
    BrowserWindowInterface* browser_window,
    contextual_search::ContextualSearchSessionHandle* session_handle) {
  std::set<tabs::TabHandle> tabs;

  if (session_handle &&
      session_handle->smart_tab_sharing_toggled_since_last_turn()) {
    return tabs;
  }

  // Map SessionID to its GURL and title.
  std::map<SessionID, std::pair<GURL, std::u16string>>
      context_session_ids_url_map;
  // Returns true if the session still tracks `id` as context. GetTokenForTab()
  // covers uploaded (attached but not yet submitted), submitted and persisted
  // tabs, and returns an empty token for tabs whose context was superceded
  // (e.g. cleared by a manual smart tab sharing toggle).
  const bool check_session_tokens =
      session_handle &&
      (base::FeatureList::IsEnabled(omnibox::kContextManagementInComposebox) ||
       IsContextualTasksRearchitectureEnabled() ||
       IsContextualTasksSidePanelRearchitectureEnabled());
  auto is_tracked_by_session = [session_handle,
                                check_session_tokens](SessionID id) {
    if (!check_session_tokens) {
      return true;
    }
    return !session_handle->GetTokenForTab(id).is_empty() ||
           session_handle->GetTabContextState().ContainsTab(id);
  };

  // Add the tabs from context if they exist in the current browser window.
  for (const auto& attachment : context.GetUrlAttachments()) {
    SessionID id = attachment.GetTabSessionId();
    if (!id.is_valid() || !is_tracked_by_session(id)) {
      continue;
    }
    context_session_ids_url_map[id] =
        std::make_pair(attachment.GetURL(), attachment.GetTitle());
  }

  // Also add the tabs the session itself still holds as submitted or persisted
  // context. This is the same set the side panel uses to render context chips.
  // The task context above is populated from URL resources echoed back by the
  // server; if that echo lacks tab ids (or has not arrived yet), the underline
  // should still reflect what the session is sharing rather than disappear.
  if (session_handle) {
    for (const contextual_search::FileInfo& file_info :
         session_handle->GetSubmittedContextFileInfos()) {
      if (!file_info.tab_session_id.has_value() ||
          !file_info.tab_session_id->is_valid() ||
          !file_info.tab_url.has_value() || file_info.is_superceded ||
          !is_tracked_by_session(*file_info.tab_session_id)) {
        continue;
      }
      // Prefer the attachment from the task context when present; it carries
      // the server-reconciled URL and title.
      context_session_ids_url_map.try_emplace(
          *file_info.tab_session_id, *file_info.tab_url,
          base::UTF8ToUTF16(file_info.tab_title.value_or("")));
    }
  }

  if (session_handle && (IsContextualTasksRearchitectureEnabled() ||
                         IsContextualTasksSidePanelRearchitectureEnabled())) {
    const auto uploaded_tokens = session_handle->GetUploadedContextTokens();
    for (const auto& attached_tab :
         session_handle->GetTabContextState().attached) {
      if (!attached_tab.tab_id.has_value()) {
        continue;
      }
      const bool is_active =
          attached_tab.submitted ||
          std::ranges::contains(uploaded_tokens, attached_tab.context_token);
      if (!is_active) {
        continue;
      }
      SessionID id = SessionID::FromSerializedValue(*attached_tab.tab_id);
      if (id.is_valid()) {
        context_session_ids_url_map.try_emplace(
            id, std::make_pair(attached_tab.url,
                               base::UTF8ToUTF16(attached_tab.title)));
      }
    }
    for (const auto& restored_tab :
         session_handle->GetTabContextState().restored) {
      if (!restored_tab.tab_id.has_value()) {
        continue;
      }
      SessionID id = SessionID::FromSerializedValue(*restored_tab.tab_id);
      if (id.is_valid()) {
        context_session_ids_url_map.try_emplace(
            id, std::make_pair(restored_tab.url,
                               base::UTF8ToUTF16(restored_tab.title)));
      }
    }
  }

  if (context_session_ids_url_map.empty()) {
    return tabs;
  }

  TabListInterface* tab_list = TabListInterface::From(browser_window);
  if (!tab_list) {
    return tabs;
  }

  for (int i = 0; i < tab_list->GetTabCount(); ++i) {
    tabs::TabInterface* tab = tab_list->GetTab(i);
    content::WebContents* web_contents = tab ? tab->GetContents() : nullptr;
    if (!web_contents) {
      continue;
    }
    SessionID tab_id = sessions::SessionTabHelper::IdForTab(web_contents);
    auto matching_attachment = context_session_ids_url_map.find(tab_id);
    if (matching_attachment != context_session_ids_url_map.end()) {
      // Verify if the tab's current URL is equivalent to the context URL.
      // If not, the tab has navigated away and is removed from context.
      const bool check_url_equivalence =
          omnibox::IsTabDeselectionInComposeboxEnabled() ||
          ((IsContextualTasksRearchitectureEnabled() ||
            IsContextualTasksSidePanelRearchitectureEnabled()) &&
           !matching_attachment->second.first.is_empty() &&
           !web_contents->GetLastCommittedURL().is_empty());
      if (check_url_equivalence) {
        bool is_equivalent =
            session_handle
                ? session_handle->AreUrlsEquivalent(
                      matching_attachment->second.first,
                      base::UTF16ToUTF8(matching_attachment->second.second),
                      web_contents->GetLastCommittedURL(),
                      base::UTF16ToUTF8(tab->GetTitle()))
                : (matching_attachment->second.first ==
                   web_contents->GetLastCommittedURL());
        if (!is_equivalent) {
          continue;
        }
      }

      if (session_handle && session_handle->IsTabDeselected(
                                tab_id, web_contents->GetLastCommittedURL(),
                                base::UTF16ToUTF8(tab->GetTitle()))) {
        continue;
      }
      tabs.insert(tab->GetHandle());
    }
  }

  return tabs;
}

}  // namespace

// static
ActiveTaskContextProvider* ActiveTaskContextProvider::From(
    BrowserWindowInterface* window) {
  return Get(window->GetUnownedUserDataHost());
}

DEFINE_USER_DATA(ActiveTaskContextProvider);

// Observes committed primary main-frame cross-document navigations on a single
// browser tab so `ActiveTaskContextProviderImpl` can clear stale local tab
// underlines and notify open Contextual Tasks sessions when an attached tab
// navigates away. Owned in `tab_navigation_observers_` (keyed by
// `tabs::TabInterface*`); created when a tab is added to the window's
// `TabListInterface`, retargeted when its `WebContents` is discarded/replaced,
// and destroyed when the tab is removed or `ActiveTaskContextProviderImpl` is
// torn down.
class ActiveTaskContextProviderImpl::TabNavigationObserver
    : public content::WebContentsObserver {
 public:
  TabNavigationObserver(ActiveTaskContextProviderImpl& owner,
                        tabs::TabInterface* tab)
      : content::WebContentsObserver(tab ? tab->GetContents() : nullptr),
        owner_(owner),
        tab_(tab),
        last_committed_url_(
            web_contents() ? web_contents()->GetLastCommittedURL() : GURL()) {}
  ~TabNavigationObserver() override = default;

  void Reobserve(content::WebContents* contents) {
    Observe(contents);
    last_committed_url_ =
        web_contents() ? web_contents()->GetLastCommittedURL() : GURL();
  }

  void UpdateLastCommittedUrl() {
    if (web_contents()) {
      last_committed_url_ = web_contents()->GetLastCommittedURL();
    }
  }

  // content::WebContentsObserver:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override {
    if (!navigation_handle->IsInPrimaryMainFrame() ||
        !navigation_handle->HasCommitted() ||
        navigation_handle->IsSameDocument()) {
      return;
    }
    GURL previous_url = last_committed_url_;
    last_committed_url_ = navigation_handle->GetURL();
    owner_->OnTabNavigated(tab_, previous_url);
  }

 private:
  const raw_ref<ActiveTaskContextProviderImpl> owner_;
  raw_ptr<tabs::TabInterface> tab_;
  GURL last_committed_url_;
};

ActiveTaskContextProviderImpl::ActiveTaskContextProviderImpl(
    BrowserWindowInterface* browser_window,
    ContextualTasksService* contextual_tasks_service)
    : browser_window_(browser_window),
      contextual_tasks_service_(contextual_tasks_service),
      scoped_unowned_user_data_(browser_window->GetUnownedUserDataHost(),
                                *this) {
  CHECK(contextual_tasks_service_);
}

void ActiveTaskContextProviderImpl::SetContextualTasksPanelController(
    ContextualTasksPanelController* contextual_tasks_panel_controller) {
  contextual_tasks_panel_controller_ = contextual_tasks_panel_controller;

  if (contextual_tasks_panel_controller_) {
    // Start observing the tab strip and ContextualTasksService.
    contextual_tasks_service_observation_.Observe(contextual_tasks_service_);
    auto* tab_list_interface = TabListInterface::From(browser_window_);
    if (tab_list_interface) {
      tab_list_interface->AddTabListInterfaceObserver(this);
      for (int i = 0; i < tab_list_interface->GetTabCount(); ++i) {
        ObserveTab(tab_list_interface->GetTab(i));
      }
      // Observe the active tab's WebContents on startup.
      OnActiveTabChanged(*tab_list_interface,
                         tab_list_interface->GetActiveTab());
    }
  }
}

// Local tab underlines are manual tab strip underlines that are explicitly
// requested by clients (e.g., NTP realbox or omnibox popup searchbox handlers)
// before a backend task has been created or is active.
void ActiveTaskContextProviderImpl::AddLocalTabUnderline(
    tabs::TabHandle tab_handle) {
  auto* tab_list = TabListInterface::From(browser_window_);
  if (tabs::TabInterface* tab = tab_handle.Get()) {
    ObserveTab(tab);
    if (auto it = tab_navigation_observers_.find(tab);
        it != tab_navigation_observers_.end()) {
      it->second->UpdateLastCommittedUrl();
    }
  }
  auto* active_tab = tab_list ? tab_list->GetActiveTab() : nullptr;
  tabs::TabHandle owner_handle =
      active_tab ? active_tab->GetHandle() : tabs::TabHandle::Null();
  local_tab_underlines_[owner_handle].insert(tab_handle);
  NotifyObservers();
}

void ActiveTaskContextProviderImpl::RemoveLocalTabUnderline(
    tabs::TabHandle tab_handle) {
  for (auto& [owner_handle, underlines] : local_tab_underlines_) {
    underlines.erase(tab_handle);
  }
  NotifyObservers();
}

void ActiveTaskContextProviderImpl::ClearAllLocalTabUnderlines() {
  local_tab_underlines_.clear();
  NotifyObservers();
}

ActiveTaskContextProviderImpl::~ActiveTaskContextProviderImpl() {
  tab_navigation_observers_.clear();
  for (auto& observer : observers_) {
    observer.OnActiveTaskContextProviderDestroyed();
  }
  if (auto* tab_list_interface = TabListInterface::From(browser_window_)) {
    tab_list_interface->RemoveTabListInterfaceObserver(this);
  }
}

void ActiveTaskContextProviderImpl::AddObserver(
    ActiveTaskContextProvider::Observer* observer) {
  observers_.AddObserver(observer);
}

void ActiveTaskContextProviderImpl::RemoveObserver(
    ActiveTaskContextProvider::Observer* observer) {
  observers_.RemoveObserver(observer);
}

void ActiveTaskContextProviderImpl::OnTabAdded(TabListInterface& tab_list,
                                               tabs::TabInterface* tab,
                                               int index) {
  ObserveTab(tab);
}

void ActiveTaskContextProviderImpl::OnActiveTabChanged(
    TabListInterface& tab_list,
    tabs::TabInterface* active_tab) {
  ObserveTab(active_tab);
  // Start observing the new active tab's WebContents.
  Observe(active_tab ? active_tab->GetContents() : nullptr);

  // Update the context based on the new active tab.
  RefreshContext();
}

void ActiveTaskContextProviderImpl::OnTabRemoved(
    TabListInterface& tab_list,
    tabs::TabInterface* tab,
    TabRemovedReason removed_reason) {
  if (!tab) {
    return;
  }
  tab_navigation_observers_.erase(tab);
  const tabs::TabHandle handle = tab->GetHandle();
  local_tab_underlines_.erase(handle);
  for (auto& [owner_handle, underlines] : local_tab_underlines_) {
    underlines.erase(handle);
  }

  if (content::WebContents* contents = tab->GetContents()) {
    SessionID tab_session_id = sessions::SessionTabHelper::IdForTab(contents);
    NotifyTabClosedOrNavigated(tab_session_id, std::nullopt, std::nullopt);
  }

  RefreshContext();
}

void ActiveTaskContextProviderImpl::OnWebContentsReplaced(
    TabListInterface& tab_list,
    tabs::TabInterface* tab,
    content::WebContents* old_contents,
    content::WebContents* new_contents) {
  if (!tab) {
    return;
  }
  auto it = tab_navigation_observers_.find(tab);
  if (it != tab_navigation_observers_.end()) {
    it->second->Reobserve(new_contents);
  } else {
    ObserveTab(tab);
  }
}

void ActiveTaskContextProviderImpl::OnTabListDestroyed(
    TabListInterface& tab_list) {
  tab_navigation_observers_.clear();
}

void ActiveTaskContextProviderImpl::ObserveTab(tabs::TabInterface* tab) {
  if (!tab || !tab->GetContents()) {
    return;
  }
  auto it = tab_navigation_observers_.find(tab);
  if (it == tab_navigation_observers_.end()) {
    tab_navigation_observers_.emplace(
        tab, std::make_unique<TabNavigationObserver>(*this, tab));
  } else if (it->second->web_contents() != tab->GetContents()) {
    it->second->Reobserve(tab->GetContents());
  }
}

void ActiveTaskContextProviderImpl::OnTabNavigated(tabs::TabInterface* tab,
                                                   const GURL& previous_url) {
  if (!tab || !tab->GetContents()) {
    return;
  }
  content::WebContents* contents = tab->GetContents();
  SessionID tab_session_id = sessions::SessionTabHelper::IdForTab(contents);
  GURL current_url = contents->GetLastCommittedURL();
  std::string current_title = base::UTF16ToUTF8(tab->GetTitle());

  const bool session_removed =
      NotifyTabClosedOrNavigated(tab_session_id, current_url, current_title);

  bool url_changed = false;
  if (!previous_url.is_empty() && !current_url.is_empty()) {
    // Pass empty titles so URL equivalence is evaluated purely on the URLs
    // (ignoring `current_title` vs. an untracked previous title).
    url_changed =
        session_handle_
            ? !session_handle_->AreUrlsEquivalent(previous_url, "", current_url,
                                                  "")
            : (previous_url.GetWithoutRef() != current_url.GetWithoutRef());
  }

  if (session_removed || url_changed) {
    for (auto& [owner_handle, underlines] : local_tab_underlines_) {
      underlines.erase(tab->GetHandle());
    }
  }

  RefreshContext();
}

bool ActiveTaskContextProviderImpl::NotifyTabClosedOrNavigated(
    SessionID tab_session_id,
    std::optional<GURL> current_url,
    std::optional<std::string> current_title) {
  if (!tab_session_id.is_valid()) {
    return false;
  }
  bool any_removed = false;
  // Each session handle is notified at most once, even if it is reachable from
  // multiple WebContents (e.g. the active panel WebContents is also in the
  // panel WebContents list).
  std::set<contextual_search::ContextualSearchSessionHandle*> notified_handles;

  auto notify_session_handle =
      [&](contextual_search::ContextualSearchSessionHandle* handle) {
        if (!handle || !notified_handles.insert(handle).second) {
          return;
        }
        const size_t attached_before =
            handle->GetTabContextState().attached.size();
        const size_t restored_before =
            handle->GetTabContextState().restored.size();
        auto removed = handle->OnTabClosedOrNavigated(
            tab_session_id, current_url, current_title);
        if (!removed.empty() ||
            handle->GetTabContextState().attached.size() != attached_before ||
            handle->GetTabContextState().restored.size() != restored_before) {
          any_removed = true;
        }
      };

  auto notify_web_contents = [&](content::WebContents* wc) {
    if (!wc) {
      return;
    }
    if (auto* user_data =
            ContextualTasksWebContentsUserData::FromWebContents(wc)) {
      auto* handle = user_data->GetExistingContextualSessionHandle();
      if (!handle || !notified_handles.insert(handle).second) {
        return;
      }
      if (user_data->OnBrowserTabClosedOrNavigated(tab_session_id, current_url,
                                                   current_title)) {
        any_removed = true;
      }
    } else if (auto* helper =
                   ContextualSearchWebContentsHelper::FromWebContents(wc)) {
      notify_session_handle(helper->session_handle());
    }
  };

  if (contextual_tasks_panel_controller_) {
    for (content::WebContents* panel_wc :
         contextual_tasks_panel_controller_->GetPanelWebContentsList()) {
      notify_web_contents(panel_wc);
    }
    notify_web_contents(
        contextual_tasks_panel_controller_->GetActiveWebContents());
  }

  if (auto* tab_list = TabListInterface::From(browser_window_)) {
    for (int i = 0; i < tab_list->GetTabCount(); ++i) {
      if (tabs::TabInterface* open_tab = tab_list->GetTab(i)) {
        notify_web_contents(open_tab->GetContents());
      }
    }
  }

  notify_session_handle(session_handle_.get());

  return any_removed;
}

void ActiveTaskContextProviderImpl::PrimaryPageChanged(content::Page& page) {
  GURL url = page.GetMainDocument().GetLastCommittedURL();
  bool is_aim_page = (url.scheme() == content::kChromeUIScheme &&
                      url.host() == chrome::kChromeUIContextualTasksHost) ||
                     (url.scheme() == content::kChromeUIScheme &&
                      url.host() == chrome::kChromeUINewTabPageHost);
  // Post-rearchitecture, full-tab AI Mode loads the AI Mode URL directly
  // instead of chrome://contextual-tasks. If `is_aim_page` were false, the
  // `!is_aim_page` block below would disassociate the tab's task and clear its
  // session on every navigation.
  if (IsContextualTasksRearchitectureEnabled()) {
    auto* ui_service = ContextualTasksUiServiceFactory::GetForBrowserContext(
        web_contents()->GetBrowserContext());
    is_aim_page |= ui_service && ui_service->IsAiUrl(url);
  }

  if (!is_aim_page) {
    if (base::FeatureList::IsEnabled(omnibox::kContextManagementInComposebox)) {
      tabs::TabInterface* navigating_tab =
          tabs::TabInterface::MaybeGetFromContents(web_contents());
      tabs::TabHandle navigating_tab_handle = navigating_tab
                                                  ? navigating_tab->GetHandle()
                                                  : tabs::TabHandle::Null();

      // Only erase local underlines owned by the navigating tab, not all
      // of the tabs, because now, there are multiple owners of tab underlines,
      // not just one. This is because this tab's AIM/entrypoint searchbox has
      // likely changed upon navigation, and no other tabs. If it navigates
      // back to an AIM page, it will add the underlined tabs again in the
      // contextual searchbox handler.
      local_tab_underlines_.erase(navigating_tab_handle);
    }

    // TODO(crbug.com/573687602): Move tab task/session resetting on navigation
    // out of `ActiveTaskContextProviderImpl` into a per-tab navigation
    // observer.
    auto* helper =
        ContextualSearchWebContentsHelper::FromWebContents(web_contents());
    if (helper && helper->task_id() && helper->session_handle()) {
      SessionID tab_id = sessions::SessionTabHelper::IdForTab(web_contents());
      contextual_tasks_service_->DisassociateTabFromTask(*helper->task_id(),
                                                         tab_id);
      helper->SetTaskSession(/*task_id=*/std::nullopt, /*handle=*/nullptr,
                             /*input_state_model=*/nullptr);
    }
  }

  RefreshContext();
}

void ActiveTaskContextProviderImpl::OnTaskAdded(
    const ContextualTask& task,
    ContextualTasksService::TriggerSource source) {
  RefreshContext();
}

void ActiveTaskContextProviderImpl::OnTaskUpdated(
    const ContextualTask& task,
    ContextualTasksService::TriggerSource source) {
  RefreshContext();
}

void ActiveTaskContextProviderImpl::OnTaskRemoved(
    const base::Uuid& task_id,
    ContextualTasksService::TriggerSource source) {
  RefreshContext();
}

void ActiveTaskContextProviderImpl::OnTaskAssociatedToTab(
    const base::Uuid& task_id,
    SessionID tab_id) {
  RefreshContext();
}

void ActiveTaskContextProviderImpl::OnTaskDisassociatedFromTab(
    const base::Uuid& task_id,
    SessionID tab_id) {
  RefreshContext();
}

void ActiveTaskContextProviderImpl::RefreshContext() {
  if (!contextual_tasks::IsContextualTasksUIEnabled()) {
    ResetStateAndNotifyObservers();
    return;
  }

  // Increment the callback ID to invalidate any outstanding callbacks.
  callback_id_++;

  contextual_search::ContextualSearchSessionHandle* session_handle = nullptr;
  if (contextual_tasks_panel_controller_) {
    auto [task_id, handle] = contextual_tasks_panel_controller_
                                 ->GetSessionHandleForActiveTabOrPanel();
    session_handle = handle;
    active_task_id_ = task_id;
  } else {
    ResetStateAndNotifyObservers();
  }

  session_handle_ = session_handle ? session_handle->AsWeakPtr() : nullptr;

  if (!active_task_id_.has_value()) {
    ResetStateAndNotifyObservers();
    return;
  }

  if (!session_handle) {
    ResetStateAndNotifyObservers();
    return;
  }

  auto context_decoration_params = std::make_unique<ContextDecorationParams>();
  context_decoration_params->contextual_search_session_handle =
      session_handle->AsWeakPtr();
  contextual_tasks_service_->GetContextForTask(
      active_task_id_.value(), {}, std::move(context_decoration_params),
      base::BindOnce(&ActiveTaskContextProviderImpl::OnGetContextForTask,
                     weak_ptr_factory_.GetWeakPtr(), callback_id_));
}

void ActiveTaskContextProviderImpl::OnGetContextForTask(
    int callback_id,
    std::unique_ptr<ContextualTaskContext> context) {
  // Ignore stale results if a newer request has been sent.
  if (callback_id != callback_id_) {
    return;
  }

  if (context) {
    // Ensure all existing tabs are observed for navigation even if they were
    // populated in `TabListInterface` without firing `OnTabAdded` (e.g., in
    // tests or before panel controller initialization).
    if (auto* tab_list = TabListInterface::From(browser_window_)) {
      for (int i = 0; i < tab_list->GetTabCount(); ++i) {
        ObserveTab(tab_list->GetTab(i));
      }
    }
    backend_context_tabs_ =
        GetTabsFromContext(*context, browser_window_, session_handle_.get());
  } else {
    backend_context_tabs_.clear();
  }

  NotifyObservers();
}

void ActiveTaskContextProviderImpl::ResetStateAndNotifyObservers() {
  active_task_id_ = std::nullopt;
  backend_context_tabs_.clear();
  session_handle_ = nullptr;
  NotifyObservers();
}

void ActiveTaskContextProviderImpl::NotifyObservers() {
  // Combine both the backend task context tab underlines and the local manual
  // underlines for the active tab.
  std::set<tabs::TabHandle> tabs_to_underline = backend_context_tabs_;

  auto* tab_list = TabListInterface::From(browser_window_);
  auto* active_tab = tab_list ? tab_list->GetActiveTab() : nullptr;
  tabs::TabHandle owner_handle =
      active_tab ? active_tab->GetHandle() : tabs::TabHandle::Null();

  auto it = local_tab_underlines_.find(owner_handle);
  if (it != local_tab_underlines_.end()) {
    for (auto handle : it->second) {
      tabs_to_underline.insert(handle);
    }
  }

  // Add auto-suggested tab if chip is showing.
  if (contextual_tasks_panel_controller_ &&
      contextual_tasks_panel_controller_->IsPanelOpenForContextualTask()) {
    auto maybe_handle =
        contextual_tasks_panel_controller_->GetAutoSuggestedTabHandle();
    if (maybe_handle) {
      tabs_to_underline.insert(*maybe_handle);
    }
  }

  for (auto& obs : observers_) {
    obs.OnContextTabsChanged(tabs_to_underline);
  }
}

}  // namespace contextual_tasks
