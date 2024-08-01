// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_PRIVACY_HUB_CONTENT_BLOCK_OBSERVATION_H_
#define CHROME_BROWSER_ASH_PRIVACY_HUB_CONTENT_BLOCK_OBSERVATION_H_

#include "ash/public/cpp/session/session_controller.h"
#include "ash/public/cpp/session/session_observer.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ash/privacy_hub/privacy_hub_util.h"
#include "components/prefs/pref_change_registrar.h"

namespace ash::privacy_hub_util {

// This class is used to maintain observation of the permissions blocked at the
// system level. The callback provided at constructions is called always when
// the system permissions change. The observation will continue until the object
// is destroyed.
class ContentBlockObservation : public SessionObserver {
 private:
  // This is used to restrict access to the constructor of
  // ContentBlockObservation.
  class CreationPermissionTag {
   private:
    friend class ContentBlockObservation;
    CreationPermissionTag() = default;
  };

 public:
  // Access restricted constructor.
  ContentBlockObservation(CreationPermissionTag,
                          SessionController* session_controller,
                          ContentBlockCallback callback);

  ~ContentBlockObservation() override;

  // Tries to create a new pbservation that calls the given callback.
  // Returns nullptr at failure.
  static std::unique_ptr<ContentBlockObservation> Create(
      ContentBlockCallback callback);

  // SessionObserver:
  void OnActiveUserPrefServiceChanged(PrefService* pref_service) override;

 private:
  // Handles changes in the user pref ( e.g. toggling the camera switch on
  // Privacy Hub UI).
  void OnPreferenceChanged(const std::string& pref_name);

  ContentBlockCallback callback_;
  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;
  base::ScopedObservation<SessionController, ContentBlockObservation>
      session_observation_;
  base::WeakPtrFactory<ContentBlockObservation> weak_ptr_factory_{this};
};

}  // namespace ash::privacy_hub_util
#endif  // CHROME_BROWSER_ASH_PRIVACY_HUB_CONTENT_BLOCK_OBSERVATION_H_
