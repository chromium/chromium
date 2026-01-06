// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/tabs/tabs_event_router_platform_delegate_android.h"

#include "base/debug/dump_without_crashing.h"
#include "base/notimplemented.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/extensions/api/tabs/tabs_constants.h"
#include "chrome/browser/extensions/api/tabs/tabs_event_router.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#include "components/sessions/core/session_id.h"
#include "content/public/browser/web_contents.h"

// NOTE: This entire file is throwaway code. It is being used to bootstrap tests
// on the desktop Android platform. It will eventually be replaced by a cross
// platform implementation in tabs_event_router.h/cc.

namespace extensions {

TabsEventRouterPlatformDelegate::TabEntry::TabEntry(
    TabsEventRouterPlatformDelegate* owner,
    content::WebContents* contents)
    : content::WebContentsObserver(contents),
      owner_(owner),
      url_(contents->GetURL()) {}

TabsEventRouterPlatformDelegate::TabEntry::~TabEntry() = default;

void TabsEventRouterPlatformDelegate::TabEntry::DidStopLoading() {
  std::set<std::string> changed_property_names;
  changed_property_names.insert(tabs_constants::kStatusKey);

  if (web_contents()->GetURL() != url_) {
    url_ = web_contents()->GetURL();
    changed_property_names.insert(tabs_constants::kUrlKey);
  }

  owner_->TabUpdated(this, std::move(changed_property_names));
}

void TabsEventRouterPlatformDelegate::TabEntry::TitleWasSet(
    content::NavigationEntry* entry) {
  std::set<std::string> changed_property_names;
  changed_property_names.insert(tabs_constants::kTitleKey);
  owner_->TabUpdated(this, std::move(changed_property_names));
}

void TabsEventRouterPlatformDelegate::TabEntry::WebContentsDestroyed() {
  int tab_id = ExtensionTabUtil::GetTabId(web_contents());
  if (!SessionID::IsValidValue(tab_id)) {
    return;
  }
  int removed_count = owner_->tab_entries_.erase(tab_id);
  DCHECK_GT(removed_count, 0);
}

////////////////////////////////////////////////////////////////////////////////

TabsEventRouterPlatformDelegate::TabsEventRouterPlatformDelegate(
    TabsEventRouter& router,
    Profile& profile)
    : router_(router), profile_(profile) {
  TabModelList::AddObserver(this);
  for (TabModel* const model : TabModelList::models()) {
    OnTabModelAdded(model);
  }
}

TabsEventRouterPlatformDelegate::~TabsEventRouterPlatformDelegate() {
  TabModelList::RemoveObserver(this);
}

void TabsEventRouterPlatformDelegate::OnTabModelAdded(TabModel* tab_model) {
  if (profile_->IsSameOrParent(tab_model->GetProfile())) {
    tab_model_observations_.AddObservation(tab_model);
  }
}

void TabsEventRouterPlatformDelegate::OnTabModelRemoved(TabModel* tab_model) {
  if (tab_model_observations_.IsObservingSource(tab_model)) {
    tab_model_observations_.RemoveObservation(tab_model);
  }
}

void TabsEventRouterPlatformDelegate::DidAddTab(TabAndroid* tab,
                                                TabModel::TabLaunchType type) {
  if (!tab || !tab->web_contents()) {
    return;
  }
  int tab_id = ExtensionTabUtil::GetTabId(tab->web_contents());
  if (!SessionID::IsValidValue(tab_id)) {
    return;
  }
  // In the field, sometimes tabs are added with duplicate IDs. See
  // http://crbug.com/434055707
  if (tab_entries_.contains(tab_id)) {
    LOG(ERROR) << "Duplicate tab ID " << tab_id << " for "
               << tab->GetURL().spec();
    base::debug::DumpWithoutCrashing();
    return;
  }
  tab_entries_.emplace(tab_id,
                       std::make_unique<TabEntry>(this, tab->web_contents()));

  router_->DispatchTabCreatedEvent(tab->web_contents(), tab->IsActivated());
}

void TabsEventRouterPlatformDelegate::TabRemoved(TabAndroid* tab) {
  if (!tab || !tab->web_contents()) {
    return;
  }
  int tab_id = ExtensionTabUtil::GetTabId(tab->web_contents());
  if (!SessionID::IsValidValue(tab_id)) {
    return;
  }
  // NOTE: Some tests call `TabRemoved()` without calling `DidAddTab()`, so
  // there may not be anything to erase.
  tab_entries_.erase(tab_id);
}

void TabsEventRouterPlatformDelegate::TabUpdated(
    TabEntry* entry,
    std::set<std::string> changed_property_names) {
  CHECK(!changed_property_names.empty());
  router_->DispatchTabUpdatedEvent(entry->web_contents(),
                                   std::move(changed_property_names));
}

}  // namespace extensions
