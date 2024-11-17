// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_action_dispatcher.h"

#include "base/lazy_instance.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/sessions/core/session_id.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_action.h"
#include "extensions/browser/extension_action_manager.h"
#include "extensions/browser/extension_event_histogram_value.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/common/mojom/context_type.mojom.h"

namespace extensions {

static base::LazyInstance<
    BrowserContextKeyedAPIFactory<ExtensionActionDispatcher>>::DestructorAtExit
    g_extension_action_dispatcher_factory = LAZY_INSTANCE_INITIALIZER;

ExtensionActionDispatcher::ExtensionActionDispatcher(
    content::BrowserContext* context)
    : browser_context_(context) {}

ExtensionActionDispatcher::~ExtensionActionDispatcher() = default;

// static
BrowserContextKeyedAPIFactory<ExtensionActionDispatcher>*
ExtensionActionDispatcher::GetFactoryInstance() {
  return g_extension_action_dispatcher_factory.Pointer();
}

// static
ExtensionActionDispatcher* ExtensionActionDispatcher::Get(
    content::BrowserContext* context) {
  return BrowserContextKeyedAPIFactory<ExtensionActionDispatcher>::Get(context);
}

void ExtensionActionDispatcher::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void ExtensionActionDispatcher::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void ExtensionActionDispatcher::NotifyChange(ExtensionAction* extension_action,
                                             content::WebContents* web_contents,
                                             content::BrowserContext* context) {
  for (auto& observer : observers_) {
    observer.OnExtensionActionUpdated(extension_action, web_contents, context);
  }
}

void ExtensionActionDispatcher::DispatchExtensionActionClicked(
    const ExtensionAction& extension_action,
    content::WebContents* web_contents,
    const Extension* extension) {
  events::HistogramValue histogram_value = events::UNKNOWN;
  const char* event_name = nullptr;
  switch (extension_action.action_type()) {
    case ActionInfo::Type::kAction:
      histogram_value = events::ACTION_ON_CLICKED;
      event_name = "action.onClicked";
      break;
    case ActionInfo::Type::kBrowser:
      histogram_value = events::BROWSER_ACTION_ON_CLICKED;
      event_name = "browserAction.onClicked";
      break;
    case ActionInfo::Type::kPage:
      histogram_value = events::PAGE_ACTION_ON_CLICKED;
      event_name = "pageAction.onClicked";
      break;
  }

  if (event_name) {
    base::Value::List args;
    // The action APIs (browserAction, pageAction, action) are only available
    // to privileged extension contexts. As such, we deterministically know that
    // the right context type here is privileged.
    constexpr mojom::ContextType context_type =
        mojom::ContextType::kPrivilegedExtension;
    ExtensionTabUtil::ScrubTabBehavior scrub_tab_behavior =
        ExtensionTabUtil::GetScrubTabBehavior(extension, context_type,
                                              web_contents);
    args.Append(ExtensionTabUtil::CreateTabObject(web_contents,
                                                  scrub_tab_behavior, extension)
                    .ToValue());

    DispatchEventToExtension(web_contents->GetBrowserContext(),
                             extension_action.extension_id(), histogram_value,
                             event_name, std::move(args));
  }
}

void ExtensionActionDispatcher::ClearAllValuesForTab(
    content::WebContents* web_contents) {
  DCHECK(web_contents);
  const SessionID tab_id = sessions::SessionTabHelper::IdForTab(web_contents);
  content::BrowserContext* browser_context = web_contents->GetBrowserContext();
  const ExtensionSet& enabled_extensions =
      ExtensionRegistry::Get(browser_context_)->enabled_extensions();
  ExtensionActionManager* action_manager =
      ExtensionActionManager::Get(browser_context_);

  for (const auto& extension : enabled_extensions) {
    ExtensionAction* extension_action =
        action_manager->GetExtensionAction(*extension);
    if (extension_action) {
      extension_action->ClearAllValuesForTab(tab_id.id());
      NotifyChange(extension_action, web_contents, browser_context);
    }
  }
}

ExtensionPrefs* ExtensionActionDispatcher::GetExtensionPrefs() {
  // This lazy initialization is more than just an optimization, because it
  // allows tests to associate a new ExtensionPrefs with the browser context
  // before we access it.
  if (!extension_prefs_) {
    extension_prefs_ = ExtensionPrefs::Get(browser_context_);
  }

  return extension_prefs_;
}

void ExtensionActionDispatcher::DispatchEventToExtension(
    content::BrowserContext* context,
    const ExtensionId& extension_id,
    events::HistogramValue histogram_value,
    const std::string& event_name,
    base::Value::List event_args) {
  if (!EventRouter::Get(context)) {
    return;
  }

  auto event = std::make_unique<Event>(histogram_value, event_name,
                                       std::move(event_args), context);
  event->user_gesture = EventRouter::USER_GESTURE_ENABLED;
  EventRouter::Get(context)->DispatchEventToExtension(extension_id,
                                                      std::move(event));
}

void ExtensionActionDispatcher::Shutdown() {
  for (auto& observer : observers_) {
    observer.OnShuttingDown();
  }
}

void ExtensionActionDispatcher::OnActionPinnedStateChanged(
    const ExtensionId& extension_id,
    bool is_pinned) {
  // TODO(crbug.com/360916928): Today, no action APIs are compiled.
  // Unfortunately, this means we miss out on the compiled types, which would be
  // rather helpful here.
  base::Value::List args;
  base::Value::Dict change;
  change.Set("isOnToolbar", is_pinned);
  args.Append(std::move(change));
  DispatchEventToExtension(browser_context_, extension_id,
                           events::ACTION_ON_USER_SETTINGS_CHANGED,
                           "action.onUserSettingsChanged", std::move(args));
}

}  // namespace extensions
