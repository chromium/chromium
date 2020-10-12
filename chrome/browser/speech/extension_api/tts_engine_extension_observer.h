// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SPEECH_EXTENSION_API_TTS_ENGINE_EXTENSION_OBSERVER_H_
#define CHROME_BROWSER_SPEECH_EXTENSION_API_TTS_ENGINE_EXTENSION_OBSERVER_H_

#include "base/macros.h"
#include "base/scoped_observer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/audio_service.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/accessibility/accessibility_manager.h"
#include "chromeos/services/tts/public/mojom/tts_service.mojom.h"
#endif

class Profile;

// Profile-keyed class that observes the extension registry to determine load of
// extension-based tts engines.
class TtsEngineExtensionObserver
    : public KeyedService,
      public extensions::EventRouter::Observer,
      public extensions::ExtensionRegistryObserver {
 public:
  static TtsEngineExtensionObserver* GetInstance(Profile* profile);

  // Gets the currently loaded TTS extension ids.
  const std::set<std::string> GetTtsExtensions();

  Profile* profile() { return profile_; }

#if defined(OS_CHROMEOS)
  void BindTtsStream(
      mojo::PendingReceiver<chromeos::tts::mojom::TtsStream> receiver);
#endif  // defined(OS_CHROMEOS)

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
  explicit TtsEngineExtensionObserver(Profile* profile);
  ~TtsEngineExtensionObserver() override;

  bool IsLoadedTtsEngine(const std::string& extension_id);

#if defined(OS_CHROMEOS)
  void OnAccessibilityStatusChanged(
      const chromeos::AccessibilityStatusEventDetails& details);
#endif

  ScopedObserver<extensions::ExtensionRegistry,
                 extensions::ExtensionRegistryObserver>
      extension_registry_observer_;

  Profile* profile_;

  std::set<std::string> engine_extension_ids_;

#if defined(OS_CHROMEOS)
  std::unique_ptr<chromeos::AccessibilityStatusSubscription>
      accessibility_status_subscription_;

  mojo::Remote<chromeos::tts::mojom::TtsService> tts_service_;
#endif

  friend class TtsEngineExtensionObserverFactory;

  DISALLOW_COPY_AND_ASSIGN(TtsEngineExtensionObserver);
};

#endif  // CHROME_BROWSER_SPEECH_EXTENSION_API_TTS_ENGINE_EXTENSION_OBSERVER_H_
