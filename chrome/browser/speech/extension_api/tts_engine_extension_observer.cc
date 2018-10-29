// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/speech/extension_api/tts_engine_extension_observer.h"

#include "base/logging.h"
#include "base/memory/singleton.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/speech/extension_api/tts_engine_extension_api.h"
#include "chrome/browser/speech/tts_controller.h"
#include "chrome/common/extensions/api/speech/tts_engine_manifest_handler.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/event_router_factory.h"

// Factory to load one instance of TtsExtensionLoaderChromeOs per profile.
class TtsEngineExtensionObserverFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  static TtsEngineExtensionObserver* GetForProfile(Profile* profile) {
    return static_cast<TtsEngineExtensionObserver*>(
        GetInstance()->GetServiceForBrowserContext(profile, true));
  }

  static TtsEngineExtensionObserverFactory* GetInstance() {
    return base::Singleton<TtsEngineExtensionObserverFactory>::get();
  }

 private:
  friend struct base::DefaultSingletonTraits<TtsEngineExtensionObserverFactory>;

  TtsEngineExtensionObserverFactory()
      : BrowserContextKeyedServiceFactory(
            "TtsEngineExtensionObserver",
            BrowserContextDependencyManager::GetInstance()) {
    DependsOn(extensions::EventRouterFactory::GetInstance());
  }

  ~TtsEngineExtensionObserverFactory() override {}

  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override {
    // If given an incognito profile (including the Chrome OS login
    // profile), share the service with the original profile.
    return chrome::GetBrowserContextRedirectedInIncognito(context);
  }

  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const override {
    return new TtsEngineExtensionObserver(static_cast<Profile*>(profile));
  }
};

TtsEngineExtensionObserver* TtsEngineExtensionObserver::GetInstance(
    Profile* profile) {
  return TtsEngineExtensionObserverFactory::GetInstance()->GetForProfile(
      profile);
}

TtsEngineExtensionObserver::TtsEngineExtensionObserver(Profile* profile)
    : extension_registry_observer_(this), profile_(profile) {
  extension_registry_observer_.Add(
      extensions::ExtensionRegistry::Get(profile_));

  extensions::EventRouter* event_router =
      extensions::EventRouter::Get(profile_);
  DCHECK(event_router);
  event_router->RegisterObserver(this, tts_engine_events::kOnSpeak);
  event_router->RegisterObserver(this, tts_engine_events::kOnStop);
}

TtsEngineExtensionObserver::~TtsEngineExtensionObserver() {
}

bool TtsEngineExtensionObserver::SawExtensionLoad(
    const std::string& extension_id,
    bool update) {
  bool previously_loaded =
      engine_extension_ids_.find(extension_id) != engine_extension_ids_.end();

  if (update)
    engine_extension_ids_.insert(extension_id);

  return previously_loaded;
}

const std::set<std::string> TtsEngineExtensionObserver::GetTtsExtensions() {
  return engine_extension_ids_;
}

void TtsEngineExtensionObserver::Shutdown() {
  extensions::EventRouter::Get(profile_)->UnregisterObserver(this);
}

bool TtsEngineExtensionObserver::IsLoadedTtsEngine(
    const std::string& extension_id) {
  extensions::EventRouter* event_router =
      extensions::EventRouter::Get(profile_);
  DCHECK(event_router);
  if (event_router->ExtensionHasEventListener(extension_id,
                                              tts_engine_events::kOnSpeak) &&
      event_router->ExtensionHasEventListener(extension_id,
                                              tts_engine_events::kOnStop)) {
    return true;
  }

  return false;
}

void TtsEngineExtensionObserver::OnListenerAdded(
    const extensions::EventListenerInfo& details) {
  if (!IsLoadedTtsEngine(details.extension_id))
    return;

  TtsController::GetInstance()->VoicesChanged();
  engine_extension_ids_.insert(details.extension_id);
}

void TtsEngineExtensionObserver::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension,
    extensions::UnloadedExtensionReason reason) {
  size_t erase_count = 0;
  erase_count += engine_extension_ids_.erase(extension->id());
  if (erase_count > 0)
    TtsController::GetInstance()->VoicesChanged();
}
