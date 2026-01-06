// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/tabs/tabs_event_router.h"

#include <memory>
#include <set>

#include "base/values.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/tabs.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/event_router.h"

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

TabsEventRouter::TabsEventRouter(Profile* profile)
    : platform_delegate_(*this, *profile) {}

TabsEventRouter::~TabsEventRouter() = default;

void TabsEventRouter::DispatchTabUpdatedEvent(
    content::WebContents* contents,
    std::set<std::string> changed_property_names) {
  DCHECK(!changed_property_names.empty());
  DCHECK(contents);

  Profile* profile = Profile::FromBrowserContext(contents->GetBrowserContext());

  auto event = std::make_unique<Event>(
      events::TABS_ON_UPDATED, api::tabs::OnUpdated::kEventName,
      // The event arguments depend on the extension's permission. They are set
      // in WillDispatchTabUpdatedEvent().
      base::Value::List(), profile);
  event->user_gesture = EventRouter::UserGestureState::kNotEnabled;
  event->will_dispatch_callback =
      base::BindRepeating(&WillDispatchTabUpdatedEvent, contents,
                          std::move(changed_property_names));
  EventRouter::Get(profile)->BroadcastEvent(std::move(event));
}

}  // namespace extensions
