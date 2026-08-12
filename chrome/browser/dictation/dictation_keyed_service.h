// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DICTATION_DICTATION_KEYED_SERVICE_H_
#define CHROME_BROWSER_DICTATION_DICTATION_KEYED_SERVICE_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/dictation/connector_component_extension.h"
#include "chrome/browser/dictation/dictation_multiplexer.h"
#include "chrome/browser/dictation/local_hotkey_manager.h"
#include "chrome/browser/dictation/metrics.h"
#include "chrome/browser/dictation/onboarding_manager.h"
#include "chrome/browser/dictation/session_controller.h"
#include "chrome/browser/dictation/session_controller_delegate.h"
#include "chrome/browser/dictation/target.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"

class Profile;

namespace content {
class BrowserContext;
}

namespace tabs {
class TabInterface;
}

namespace dictation {

// Created on a per-profile basis for any regular profile (i.e. excludes OTR,
// service, etc. profiles) and only when the Dictation base::Feature is enabled.
// Generally responsible for managing session lifetime and creation of concrete
// dictation objects.
class DictationKeyedService : public KeyedService,
                              public SessionControllerDelegate {
 public:
  // Null when profile doesn't support/enable Dictation.
  static DictationKeyedService* Get(content::BrowserContext* context);

  explicit DictationKeyedService(Profile* profile);
  DictationKeyedService(const DictationKeyedService&) = delete;
  DictationKeyedService& operator=(const DictationKeyedService&) = delete;
  ~DictationKeyedService() override;

  // KeyedService:
  void Shutdown() override;

  // SessionControllerDelegate:
  std::unique_ptr<StreamProvider> CreateStreamProvider(
      SessionController& controller) const override;
  std::unique_ptr<SessionUi> CreateUi(
      SessionController& controller) const override;
  void EndSession() override;

  // Called when onboarding is completed. Starts a new session from the given
  // target. It's the caller's responsibility to ensure this is never called
  // while an existing session is in progress.
  void DidCompleteOnboarding(tabs::TabInterface& tab,
                             const TargetDetails& target_details,
                             DictationSessionEntryPoint entry_point);

  // Called when the Dictation Connector component extension finishes
  // installing.
  void DidInstallConnector();

  // If `target_details` has a null DOMNodeId, the focused element in the
  // specified Document is used.
  //
  // TODO(b/531049588): Update tests to always provide a valid target, remove
  // the "focused element" semantic, and CHECK that the provided target is
  // always non-null.
  //
  // TODO(amyasinghal): Update tests to call ContextMenuHandler or
  // ToggleHotkeyHandler and remove StartSessionForTesting.
  void StartSessionForTesting(tabs::TabInterface& tab,
                              const TargetDetails& target_details,
                              DictationSessionEntryPoint entry_point);

  bool ShouldShowContextMenuItem() const;

  // Handles the context menu item click.
  void ContextMenuHandler(const TargetDetails& target_details);

  // Handles the dictation hotkey press.
  virtual void ToggleHotkeyHandler();

  // Returns null when no session is in progress.
  SessionController* session_controller() {
    return session_ ? &session_->controller_ : nullptr;
  }
  const SessionController* session_controller() const {
    return const_cast<DictationKeyedService*>(this)->session_controller();
  }

  DictationMultiplexer& multiplexer() { return multiplexer_; }

  // Updates audio level in the current session.
  void UpdateAudioLevel(float audio_level);

  bool IsEnabledAndReady() const;

  LocalHotkeyManager* local_hotkey_manager_for_testing() {
    return local_hotkey_manager_.get();
  }

 private:
  // Starts a new session from the given target. It's the caller's
  // responsibility to ensure this never called while an existing session in
  // progress.
  void StartSession(tabs::TabInterface& tab,
                    const TargetDetails& target_details,
                    DictationSessionEntryPoint entry_point);

  void OnPrefChanged();
  void UpdateHotkeyManager();

  // Handles triggering a session, either starting a new one
  // or updating an existing session to start a new stream, possibly in a new
  // tab.
  void TriggerSession(const TargetDetails& target_details,
                      DictationSessionEntryPoint entry_point);

  raw_ptr<Profile> profile_;

  PrefChangeRegistrar pref_change_registrar_;

  ConnectorComponentExtension connector_extension_;

  DictationMultiplexer multiplexer_;

  OnboardingManager onboarding_manager_;

  std::unique_ptr<LocalHotkeyManager> local_hotkey_manager_;

  struct SessionState {
    SessionState(SessionControllerDelegate& delegate, tabs::TabInterface& tab);
    ~SessionState();

    SessionController controller_;
    base::WeakPtr<tabs::TabInterface> tab_;
  };
  std::optional<SessionState> session_;
};

}  // namespace dictation

#endif  // CHROME_BROWSER_DICTATION_DICTATION_KEYED_SERVICE_H_
