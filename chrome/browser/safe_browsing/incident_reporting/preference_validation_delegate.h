// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_INCIDENT_REPORTING_PREFERENCE_VALIDATION_DELEGATE_H_
#define CHROME_BROWSER_SAFE_BROWSING_INCIDENT_REPORTING_PREFERENCE_VALIDATION_DELEGATE_H_

#include <memory>
#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_observer.h"
#include "services/preferences/public/mojom/tracked_preference_validation_delegate.mojom.h"

namespace safe_browsing {

class IncidentReceiver;

// A preference validation delegate that adds incidents to a given receiver
// for preference validation failures. The profile for which the delegate
// operates must outlive the delegate itself.
class PreferenceValidationDelegate
    : public prefs::mojom::TrackedPreferenceValidationDelegate,
      public ProfileObserver {
 public:
  PreferenceValidationDelegate(
      Profile* profile,
      std::unique_ptr<IncidentReceiver> incident_receiver);

  PreferenceValidationDelegate(const PreferenceValidationDelegate&) = delete;
  PreferenceValidationDelegate& operator=(const PreferenceValidationDelegate&) =
      delete;

  ~PreferenceValidationDelegate() override;

 private:
  // TrackedPreferenceValidationDelegate methods.
  void OnAtomicPreferenceValidation(
      const std::string& pref_path,
      std::optional<base::Value> value,
      prefs::mojom::TrackedPreferenceValidationDelegate::ValueState value_state,
      prefs::mojom::TrackedPreferenceValidationDelegate::ValueState
          external_validation_value_state,
      bool is_personal) override;
  void OnSplitPreferenceValidation(
      const std::string& pref_path,
      const std::vector<std::string>& invalid_keys,
      const std::vector<std::string>& external_validation_invalid_keys,
      prefs::mojom::TrackedPreferenceValidationDelegate::ValueState value_state,
      prefs::mojom::TrackedPreferenceValidationDelegate::ValueState
          external_validation_value_state,
      bool is_personal) override;

  // ProfileManagerObserver methods:
  void OnProfileWillBeDestroyed(Profile* profile) override;

  raw_ptr<Profile> profile_;
  std::unique_ptr<IncidentReceiver> incident_receiver_;

  base::ScopedObservation<Profile, ProfileObserver> profile_observation_{this};
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_INCIDENT_REPORTING_PREFERENCE_VALIDATION_DELEGATE_H_
