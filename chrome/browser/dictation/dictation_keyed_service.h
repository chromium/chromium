// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DICTATION_DICTATION_KEYED_SERVICE_H_
#define CHROME_BROWSER_DICTATION_DICTATION_KEYED_SERVICE_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/dictation/connector_component_extension.h"
#include "chrome/browser/dictation/dictation_multiplexer.h"
#include "chrome/browser/dictation/metrics.h"
#include "chrome/browser/dictation/onboarding_manager.h"
#include "chrome/browser/dictation/session_controller.h"
#include "chrome/browser/dictation/session_controller_delegate.h"
#include "chrome/browser/dictation/target.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "content/public/browser/global_dom_node_id.h"

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

  // Starts a new session from the given target. It's the caller's
  // responsibility to ensure this never called while an existing session in
  // progress.
  //
  // The new session will immediately start up a stream using the given
  // target_details.
  //
  // If `target_details` has a null DOMNodeId, the focused element in the
  // specified Document is used.
  // TODO(b/531049588): Update tests to always provide a valid target, remove
  // the "focused element" semantic, and CHECK that the provided target is
  // always non-null.
  void StartSession(tabs::TabInterface& tab,
                    const TargetDetails& target_details,
                    DictationSessionEntryPoint entry_point);

  bool ShouldShowContextMenuItem() const;

  // Handles the context menu item click.
  void ContextMenuHandler(const TargetDetails& target_details);

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

 private:
  void OnPrefChanged();

  // Returns true if dictation feature is enabled by all flags and policies and
  // the system is fully initialized and ready to use.
  bool IsEnabledAndReady() const;

  raw_ptr<Profile> profile_;

  PrefChangeRegistrar pref_change_registrar_;

  ConnectorComponentExtension connector_extension_;

  DictationMultiplexer multiplexer_;

  OnboardingManager onboarding_manager_;

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
