// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SPEECH_EXTENSION_API_TTS_ENGINE_EXTENSION_OBSERVER_CHROMEOS_H_
#define CHROME_BROWSER_SPEECH_EXTENSION_API_TTS_ENGINE_EXTENSION_OBSERVER_CHROMEOS_H_

#include "base/macros.h"
#include "base/scoped_observer.h"
#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#include "chromeos/services/tts/public/mojom/tts_service.mojom.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/audio_service.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"

class Profile;

// Profile-keyed class that observes the extension registry to determine load of
// extension-based tts engines.
class TtsEngineExtensionObserverChromeOS
    : public KeyedService,
      public extensions::EventRouter::Observer,
      public extensions::ExtensionRegistryObserver {
 public:
  static TtsEngineExtensionObserverChromeOS* GetInstance(Profile* profile);

  // Gets the currently loaded TTS extension ids.
  const std::set<std::string>& engine_extension_ids() {
    return engine_extension_ids_;
  }

  Profile* profile() { return profile_; }

  void BindTtsStreamFactory(
      mojo::PendingReceiver<chromeos::tts::mojom::TtsStreamFactory> receiver);

  // Implementation of KeyedService.
  void Shutdown() override;

  // Implementation of extensions::EventRouter::Observer.
  void OnListenerAdded(const extensions::EventListenerInfo& details) override;

  // extensions::ExtensionRegistryObserver overrides.
  void OnExtensionLoaded(content::BrowserContext* browser_context,
                         const extensions::Extension* extension) override;
  void OnExtensionUnloaded(content::BrowserContext* browser_context,
                           const extensions::Extension* extension,
                           extensions::UnloadedExtensionReason reason) override;

 private:
  explicit TtsEngineExtensionObserverChromeOS(Profile* profile);
  ~TtsEngineExtensionObserverChromeOS() override;

  bool IsLoadedTtsEngine(const std::string& extension_id);

  void OnAccessibilityStatusChanged(
      const ash::AccessibilityStatusEventDetails& details);

  ScopedObserver<extensions::ExtensionRegistry,
                 extensions::ExtensionRegistryObserver>
      extension_registry_observer_;

  Profile* profile_;

  std::set<std::string> engine_extension_ids_;

  base::CallbackListSubscription accessibility_status_subscription_;

  mojo::Remote<chromeos::tts::mojom::TtsService> tts_service_;

  friend class TtsEngineExtensionObserverChromeOSFactory;

  DISALLOW_COPY_AND_ASSIGN(TtsEngineExtensionObserverChromeOS);
};

#endif  // CHROME_BROWSER_SPEECH_EXTENSION_API_TTS_ENGINE_EXTENSION_OBSERVER_CHROMEOS_H_
