// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRIVACY_SANDBOX_GENERATED_FLOC_PREF_H_
#define CHROME_BROWSER_PRIVACY_SANDBOX_GENERATED_FLOC_PREF_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/extensions/api/settings_private/generated_pref.h"
#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_change_registrar.h"

extern const char kGeneratedFlocPref[];

// A generated preference which is used on the chrome://settings/privacySandbox
// page to drive the FLoC toggle. This preference reflects the effective state
// of FLoC, which also respects the main Privacy Sandbox pref as an opt out,
// rather than the state of the underlying FLoC preference.
class GeneratedFlocPref : public extensions::settings_private::GeneratedPref {
 public:
  explicit GeneratedFlocPref(Profile* profile);

  // extensions::settings_private::GeneratedPref:
  extensions::settings_private::SetPrefResult SetPref(
      const base::Value* value) override;
  std::unique_ptr<extensions::api::settings_private::PrefObject> GetPrefObject()
      const override;

  // Called when one of the real preferences this generated pref depends on is
  // changed.
  void OnSourcePreferencesChanged();

 private:
  const raw_ptr<Profile> profile_;

  PrefChangeRegistrar user_prefs_registrar_;
};

#endif  // CHROME_BROWSER_PRIVACY_SANDBOX_GENERATED_FLOC_PREF_H_
