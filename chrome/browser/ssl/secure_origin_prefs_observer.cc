// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/secure_origin_prefs_observer.h"

#include <string>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/string_util.h"
#include "base/syslog_logging.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"

SecureOriginPrefsObserver::SecureOriginPrefsObserver(
    PrefService* pref_service) {
  pref_change_registrar_.Init(pref_service);

  // Start listening for pref change notifications.
  //
  // base::Unretained is okay below, because |pref_change_registrar_|'s lifetime
  // is owned by (and shorter than) |this|.
  pref_change_registrar_.Add(
      prefs::kUnsafelyTreatInsecureOriginAsSecure,
      base::BindRepeating(
          &SecureOriginPrefsObserver::OnChangeInSecureOriginPref,
          base::Unretained(this)));

  // Make sure that not only *future* changes of prefs are applied, but that
  // also the *current* state of prefs is applied.
  OnChangeInSecureOriginPref();
}

void SecureOriginPrefsObserver::OnChangeInSecureOriginPref() {
  // Don't do anything if the policy was removed or shouldn't apply.
  std::string pref_value;
  if (pref_change_registrar_.prefs()->HasPrefPath(
          prefs::kUnsafelyTreatInsecureOriginAsSecure)) {
    pref_value = pref_change_registrar_.prefs()->GetString(
        prefs::kUnsafelyTreatInsecureOriginAsSecure);
  }

  std::vector<std::string> rejected_patterns;
  network::SecureOriginAllowlist::GetInstance().SetAuxiliaryAllowlist(
      pref_value, &rejected_patterns);

  if (!rejected_patterns.empty()) {
    SYSLOG(ERROR) << "The '" << prefs::kUnsafelyTreatInsecureOriginAsSecure
                  << "' preference or policy contained invalid values "
                  << "(they have been ignored): "
                  << base::JoinString(rejected_patterns, ", ");
  }
}
