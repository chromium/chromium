// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DICTATION_ONBOARDING_MANAGER_H_
#define CHROME_BROWSER_DICTATION_ONBOARDING_MANAGER_H_

#include <memory>
#include <optional>

#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/dictation/metrics.h"

class PrefService;

namespace tabs {
class TabInterface;
}

namespace dictation {

class DictationKeyedService;
class OnboardingDialogController;
class Target;

// Manages the first-run onboarding experience for a user the first time
// dictation is triggered.
class OnboardingManager {
 public:
  OnboardingManager(DictationKeyedService& service, PrefService& pref_service);
  ~OnboardingManager();

  OnboardingManager(const OnboardingManager&) = delete;
  OnboardingManager& operator=(const OnboardingManager&) = delete;

  // Returns true if first-run onboarding has not yet been completed.
  bool NeedsOnboarding() const;

  // Shows the onboarding dialog on `tab`, storing `target` and `entry_point` to
  // start the session once onboarding completes.
  // TODO(b/527240600): Consider returning an error state when failing closed.
  void ShowOnboarding(tabs::TabInterface& tab,
                      std::unique_ptr<Target> target,
                      DictationSessionEntryPoint entry_point);

 private:
  void OnOnboardingCompleted();
  void OnDialogClosed();

  // Owns `this`.
  raw_ref<DictationKeyedService> service_;
  raw_ref<PrefService> pref_service_;

  std::unique_ptr<OnboardingDialogController> dialog_controller_;

  struct PendingSession {
    PendingSession(base::WeakPtr<tabs::TabInterface> tab,
                   std::unique_ptr<Target> target,
                   DictationSessionEntryPoint entry_point);
    PendingSession(PendingSession&&);
    PendingSession& operator=(PendingSession&&);
    ~PendingSession();

    base::WeakPtr<tabs::TabInterface> tab;
    std::unique_ptr<Target> target;
    DictationSessionEntryPoint entry_point;
  };
  std::optional<PendingSession> pending_session_;

  base::WeakPtrFactory<OnboardingManager> weak_ptr_factory_{this};
};

}  // namespace dictation

#endif  // CHROME_BROWSER_DICTATION_ONBOARDING_MANAGER_H_
