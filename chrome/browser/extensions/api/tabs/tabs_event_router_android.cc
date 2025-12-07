// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/tabs/tabs_event_router_android.h"

#include "base/debug/dump_without_crashing.h"
#include "base/notimplemented.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/extensions/api/tabs/tabs_constants.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#include "components/sessions/core/session_id.h"
#include "content/public/browser/web_contents.h"

namespace extensions {
namespace {

// Callback for the event dispatch system. Computes which tab properties have
// changed. Builds an argument list with an entry for the changed properties and
// another entry with all properties. The properties may be "scrubbed" of
// sensitive information (like the previous URL). Returns true so the event will
// be dispatched.
bool WillDispatchTabUpdatedEvent(
    content::WebContents* contents,
    const std::set<std::string>& changed_property_names,
    bool complete,
    content::BrowserContext* browser_context,
    mojom::ContextType target_context,
    const Extension* extension,
    const base::Value::Dict* listener_filter,
    std::optional<base::Value::List>& event_args_out,
    mojom::EventFilteringInfoPtr& event_filtering_info_out,
    bool* dispatch_separate_event_out) {
  auto scrub_tab_behavior = ExtensionTabUtil::GetScrubTabBehavior(
      extension, target_context, contents);
  api::tabs::Tab tab_object = ExtensionTabUtil::CreateTabObject(
      contents, scrub_tab_behavior, extension);

  base::Value::Dict tab_value = tab_object.ToValue();

  base::Value::Dict changed_properties;
  for (const auto& property : changed_property_names) {
    if (const base::Value* value = tab_value.Find(property)) {
      changed_properties.Set(property, value->Clone());
    }
  }

  event_args_out.emplace();
  event_args_out->Append(ExtensionTabUtil::GetTabId(contents));
  event_args_out->Append(std::move(changed_properties));
  event_args_out->Append(std::move(tab_value));
  return true;
}

}  // namespace

TabsEventRouterAndroid::TabEntry::TabEntry(TabsEventRouterAndroid* router,
                                           content::WebContents* contents)
    : content::WebContentsObserver(contents),
      router_(router),
      url_(contents->GetURL()) {}

TabsEventRouterAndroid::TabEntry::~TabEntry() = default;

void TabsEventRouterAndroid::TabEntry::DidStopLoading() {
  std::set<std::string> changed_property_names;
  changed_property_names.insert(tabs_constants::kStatusKey);

  if (web_contents()->GetURL() != url_) {
    url_ = web_contents()->GetURL();
    changed_property_names.insert(tabs_constants::kUrlKey);
  }

  router_->TabUpdated(this, std::move(changed_property_names));
}

void TabsEventRouterAndroid::TabEntry::TitleWasSet(
    content::NavigationEntry* entry) {
  std::set<std::string> changed_property_names;
  changed_property_names.insert(tabs_constants::kTitleKey);
  router_->TabUpdated(this, std::move(changed_property_names));
}

void TabsEventRouterAndroid::TabEntry::WebContentsDestroyed() {
  int tab_id = ExtensionTabUtil::GetTabId(web_contents());
  if (!SessionID::IsValidValue(tab_id)) {
    return;
  }
  int removed_count = router_->tab_entries_.erase(tab_id);
  DCHECK_GT(removed_count, 0);
}

////////////////////////////////////////////////////////////////////////////////

TabsEventRouterAndroid::TabsEventRouterAndroid(Profile* profile)
    : profile_(profile) {
  TabModelList::AddObserver(this);
  for (TabModel* const model : TabModelList::models()) {
    OnTabModelAdded(model);
  }
}

TabsEventRouterAndroid::~TabsEventRouterAndroid() {
  TabModelList::RemoveObserver(this);
}

void TabsEventRouterAndroid::OnTabModelAdded(TabModel* tab_model) {
  if (profile_->IsSameOrParent(tab_model->GetProfile())) {
    tab_model_observations_.AddObservation(tab_model);
  }
}

void TabsEventRouterAndroid::OnTabModelRemoved(TabModel* tab_model) {
  if (tab_model_observations_.IsObservingSource(tab_model)) {
    tab_model_observations_.RemoveObservation(tab_model);
  }
}

void TabsEventRouterAndroid::DidAddTab(TabAndroid* tab,
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
}

void TabsEventRouterAndroid::TabRemoved(TabAndroid* tab) {
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

void TabsEventRouterAndroid::TabUpdated(
    TabEntry* entry,
    std::set<std::string> changed_property_names) {
  CHECK(!changed_property_names.empty());
  DispatchTabUpdatedEvent(entry->web_contents(),
                          std::move(changed_property_names));
}

void TabsEventRouterAndroid::DispatchTabUpdatedEvent(
    content::WebContents* contents,
    std::set<std::string> changed_property_names) {
  DCHECK(!changed_property_names.empty());
  DCHECK(contents);

  Profile* const profile =
      Profile::FromBrowserContext(contents->GetBrowserContext());

  const base::Value::List event_args;
  auto event = std::make_unique<Event>(
      events::TABS_ON_UPDATED, api::tabs::OnUpdated::kEventName,
      // The event arguments depend on the extension's permission. They are set
      // in WillDispatchTabUpdatedEvent().
      base::Value::List(), profile);
  event->user_gesture = EventRouter::UserGestureState::kNotEnabled;
  event->will_dispatch_callback = base::BindRepeating(
      &WillDispatchTabUpdatedEvent, contents, std::move(changed_property_names),
      /*complete=*/true);
  EventRouter::Get(profile)->BroadcastEvent(std::move(event));
}

}  // namespace extensions
