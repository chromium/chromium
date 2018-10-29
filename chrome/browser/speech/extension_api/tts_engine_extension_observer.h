// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SPEECH_EXTENSION_API_TTS_ENGINE_EXTENSION_OBSERVER_H_
#define CHROME_BROWSER_SPEECH_EXTENSION_API_TTS_ENGINE_EXTENSION_OBSERVER_H_

#include "base/macros.h"
#include "base/scoped_observer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"

class Profile;

// Profile-keyed class that observes the extension registry to determine load of
// extension-based tts engines.
class TtsEngineExtensionObserver
    : public KeyedService,
      public extensions::EventRouter::Observer,
      public extensions::ExtensionRegistryObserver {
 public:
  static TtsEngineExtensionObserver* GetInstance(Profile* profile);

  // Returns if this observer saw the given extension load. Adds |extension_id|
  // as loaded immediately if |update| is set to true.
  bool SawExtensionLoad(const std::string& extension_id, bool update);

  // Gets the currently loaded TTS extension ids.
  const std::set<std::string> GetTtsExtensions();

  // Implementation of KeyedService.
  void Shutdown() override;

  // Implementation of extensions::EventRouter::Observer.
  void OnListenerAdded(const extensions::EventListenerInfo& details) override;

  // extensions::ExtensionRegistryObserver overrides.
  void OnExtensionUnloaded(content::BrowserContext* browser_context,
                           const extensions::Extension* extension,
                           extensions::UnloadedExtensionReason reason) override;

 private:
  explicit TtsEngineExtensionObserver(Profile* profile);
  ~TtsEngineExtensionObserver() override;

  bool IsLoadedTtsEngine(const std::string& extension_id);

  ScopedObserver<extensions::ExtensionRegistry,
                 extensions::ExtensionRegistryObserver>
      extension_registry_observer_;

  Profile* profile_;

  std::set<std::string> engine_extension_ids_;

  friend class TtsEngineExtensionObserverFactory;

  DISALLOW_COPY_AND_ASSIGN(TtsEngineExtensionObserver);
};

#endif  // CHROME_BROWSER_SPEECH_EXTENSION_API_TTS_ENGINE_EXTENSION_OBSERVER_H_
