// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/telemetry/api/events/event_manager.h"

#include "base/check.h"
#include "base/no_destructor.h"
#include "chrome/browser/chromeos/extensions/telemetry/api/events/event_router.h"
#include "chrome/browser/chromeos/extensions/telemetry/api/events/remote_event_service_strategy.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/common/chromeos/extensions/chromeos_system_extension_info.h"
#include "chromeos/crosapi/mojom/telemetry_event_service.mojom.h"
#include "chromeos/crosapi/mojom/telemetry_extension_exception.mojom.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/url_pattern.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "url/gurl.h"

namespace chromeos {

namespace {

namespace crosapi = ::crosapi::mojom;

bool IsRelatedPwaUrl(const std::string& related_pwa, GURL url_to_compare) {
  URLPattern pattern(URLPattern::SCHEME_ALL);
  return pattern.Parse(related_pwa) == URLPattern::ParseResult::kSuccess &&
         pattern.MatchesURL(url_to_compare);
}

bool IsPwaOpenForExtensionId(extensions::ExtensionId extension_id,
                             content::BrowserContext* context) {
  if (!IsChromeOSSystemExtension(extension_id)) {
    return false;
  }
  auto related_pwa = GetChromeOSExtensionInfoForId(extension_id).pwa_origin;

  Profile* profile = Profile::FromBrowserContext(context);
  for (auto* target_browser : *BrowserList::GetInstance()) {
    // Ignore incognito.
    if (target_browser->profile() != profile) {
      continue;
    }

    TabStripModel* target_tab_strip = target_browser->tab_strip_model();
    for (int i = 0; i < target_tab_strip->count(); ++i) {
      content::WebContents* target_contents =
          target_tab_strip->GetWebContentsAt(i);

      if (IsRelatedPwaUrl(related_pwa,
                          target_contents->GetLastCommittedURL())) {
        return true;
      }
    }
  }

  return false;
}

}  // namespace

// static
extensions::BrowserContextKeyedAPIFactory<EventManager>*
EventManager::GetFactoryInstance() {
  static base::NoDestructor<
      extensions::BrowserContextKeyedAPIFactory<EventManager>>
      instance;
  return instance.get();
}

// static
EventManager* EventManager::Get(content::BrowserContext* browser_context) {
  return extensions::BrowserContextKeyedAPIFactory<EventManager>::Get(
      browser_context);
}

EventManager::EventManager(content::BrowserContext* context)
    : event_router_(context), browser_context_(context) {
  // Register this class as an observer of the correct tab strip models.
  auto* profile = Profile::FromBrowserContext(context);
  auto* browser_list = BrowserList::GetInstance();
  browser_list->AddObserver(this);

  for (auto* target_browser : *browser_list) {
    if (target_browser->profile() != profile) {
      continue;
    }

    TabStripModel* tab_strip = target_browser->tab_strip_model();
    tab_strip->AddObserver(this);
  }
}

EventManager::~EventManager() {
  BrowserList::RemoveObserver(this);
  for (auto* target_browser : *BrowserList::GetInstance()) {
    if (target_browser->profile() != target_browser->profile()) {
      continue;
    }

    TabStripModel* tab_strip = target_browser->tab_strip_model();
    tab_strip->RemoveObserver(this);
  }
}

void EventManager::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  if (tab_strip_model->profile() !=
      Profile::FromBrowserContext(browser_context_)) {
    return;
  }

  for (auto& [extension_id, open] : open_pwas_) {
    auto related_pwa = GetChromeOSExtensionInfoForId(extension_id).pwa_origin;
    if (change.type() == TabStripModelChange::kRemoved) {
      for (auto& removed_tab : change.GetRemove()->contents) {
        if (IsRelatedPwaUrl(related_pwa,
                            removed_tab.contents->GetLastCommittedURL())) {
          // The PWA has been closed, safe that state and cut all connections.
          open = false;
          event_router_.ResetReceiversForExtension(extension_id);
        }
      }
    } else if (change.type() == TabStripModelChange::kInserted) {
      for (auto& inserted_tab : change.GetInsert()->contents) {
        if (IsRelatedPwaUrl(related_pwa,
                            inserted_tab.contents->GetLastCommittedURL())) {
          // The PWA is now open.
          open = true;
        }
      }
    }
  }
}

void EventManager::OnBrowserAdded(Browser* browser) {
  // Add ourselves as an observer in case a new browser is opened.
  if (browser->profile() == Profile::FromBrowserContext(browser_context_)) {
    browser->tab_strip_model()->AddObserver(this);
  }
}

EventManager::RegisterEventResult EventManager::RegisterExtensionForEvent(
    extensions::ExtensionId extension_id,
    crosapi::TelemetryEventCategoryEnum category) {
  auto is_related_pwa_open =
      IsPwaOpenForExtensionId(extension_id, browser_context_);
  // This is a noop in case the pwa is closed or there is already an existing
  // observation.
  if (!is_related_pwa_open) {
    return kPwaClosed;
  }
  if (event_router_.IsExtensionObservingForCategory(extension_id, category)) {
    // Early return in case the category is already observed by the extension.
    return kSuccess;
  }

  open_pwas_.emplace(extension_id, is_related_pwa_open);
  GetRemoteService()->AddEventObserver(
      category, event_router_.GetPendingRemoteForCategoryAndExtension(
                    category, extension_id));
  return kSuccess;
}

void EventManager::RemoveObservationsForExtensionAndCategory(
    extensions::ExtensionId extension_id,
    crosapi::TelemetryEventCategoryEnum category) {
  event_router_.ResetReceiversOfExtensionByCategory(extension_id, category);
}

void EventManager::IsEventSupported(
    crosapi::TelemetryEventCategoryEnum category,
    crosapi::TelemetryEventService::IsEventSupportedCallback callback) {
  GetRemoteService()->IsEventSupported(category, std::move(callback));
}

mojo::Remote<crosapi::TelemetryEventService>& EventManager::GetRemoteService() {
  if (!remote_event_service_strategy_) {
    remote_event_service_strategy_ = RemoteEventServiceStrategy::Create();
  }
  return remote_event_service_strategy_->GetRemoteService();
}

}  // namespace chromeos
