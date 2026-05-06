// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DICTATION_DICTATION_KEYED_SERVICE_H_
#define CHROME_BROWSER_DICTATION_DICTATION_KEYED_SERVICE_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/dictation/session_controller.h"
#include "chrome/browser/dictation/session_controller_delegate.h"
#include "components/keyed_service/core/keyed_service.h"

class BrowserWindowInterface;
class Profile;

namespace content {
class BrowserContext;
}

namespace dictation {

class Target;

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
  // If a target is provided, the new session will immediately start up a
  // stream. Otherwise, if nullptr is passed the session is created without a
  // stream.
  void StartSession(BrowserWindowInterface& window, Target* target);

  // Returns null when no session is in progress.
  SessionController* session_controller() {
    return session_ ? &session_->controller_ : nullptr;
  }
  const SessionController* session_controller() const {
    return const_cast<DictationKeyedService*>(this)->session_controller();
  }

 private:
  raw_ptr<Profile> profile_;

  struct SessionState {
    SessionState(SessionControllerDelegate& delegate,
                 base::WeakPtr<BrowserWindowInterface> window);
    ~SessionState();

    SessionController controller_;
    base::WeakPtr<BrowserWindowInterface> window_;
  };
  std::optional<SessionState> session_;
};

}  // namespace dictation

#endif  // CHROME_BROWSER_DICTATION_DICTATION_KEYED_SERVICE_H_
