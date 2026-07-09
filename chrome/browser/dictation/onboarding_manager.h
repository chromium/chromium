// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DICTATION_ONBOARDING_MANAGER_H_
#define CHROME_BROWSER_DICTATION_ONBOARDING_MANAGER_H_

#include <memory>
#include <optional>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/dictation/target.h"

class PrefService;

namespace tabs {
class TabInterface;
}

namespace dictation {

class DictationKeyedService;
class OnboardingDialogController;

// Managers the first-run onboarding experience for a user the first time
// dictation is triggered.
class OnboardingManager {
 public:
  OnboardingManager(DictationKeyedService& service, PrefService& pref_service);
  ~OnboardingManager();

  OnboardingManager(const OnboardingManager&) = delete;
  OnboardingManager& operator=(const OnboardingManager&) = delete;

  // Returns true if onboarding is needed and the caller must not
  // proceed, in which case OnboardingManager will start a session when the user
  // completes onboarding. Returns false if onboarding is not needed.
  // TODO(b/527240600): This returns true in cases of failure which has correct
  // behavior in terms of preventing a session start but should return an error
  // state.
  bool ShowOnboardingIfNeeded(tabs::TabInterface& tab,
                              const TargetId& target_id);

 private:
  void OnOnboardingCompleted();
  void OnDialogClosed();

  // Owns `this`
  raw_ref<DictationKeyedService> service_;
  raw_ref<PrefService> pref_service_;

  std::unique_ptr<OnboardingDialogController> dialog_controller_;

  base::WeakPtr<tabs::TabInterface> pending_tab_;
  std::optional<TargetId> pending_target_id_;

  base::WeakPtrFactory<OnboardingManager> weak_ptr_factory_{this};
};

}  // namespace dictation

#endif  // CHROME_BROWSER_DICTATION_ONBOARDING_MANAGER_H_
