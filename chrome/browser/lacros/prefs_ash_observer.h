// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LACROS_PREFS_ASH_OBSERVER_H_
#define CHROME_BROWSER_LACROS_PREFS_ASH_OBSERVER_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "chromeos/lacros/crosapi_pref_observer.h"
#include "components/prefs/pref_service.h"

class PrefService;
class Profile;

// Observes ash-chrome for changes and propagates them to the local state and
// user pref services.
class PrefsAshObserver {
 public:
  explicit PrefsAshObserver(PrefService* local_state);
  PrefsAshObserver(const PrefsAshObserver&) = delete;
  PrefsAshObserver& operator=(const PrefsAshObserver&) = delete;
  ~PrefsAshObserver();

  void Init();
  void InitPostProfileInitialized(Profile* profile);

 private:
  FRIEND_TEST_ALL_PREFIXES(PrefsAshObserver,
                           DnsOverHttpsEffectiveTemplatesChromeOSChanged);

  void OnDnsOverHttpsModeChanged(base::Value value);
  void OnDnsOverHttpsEffectiveTemplatesChromeOSChanged(base::Value value);
  void OnMahiEnabledChanged(base::Value value);
  // TODO(acostinas, b/328566515) Remove monitoring of the
  // kDnsOverHttpsTemplates after version 126.
  void OnDeprecatedDnsOverHttpsTemplatesChanged(base::Value value);

  void OnUserProfileValueChanged(const std::string& target_pref,
                                 base::Value value);
  static void ListChangedHandler(PrefService* pref_service,
                                 const std::string& pref_name,
                                 base::Value value);

  // Pref values that are supposed to be propagated to the user pref service
  // could be sent from ash to lacros before the user profile is created.
  // Attempting to propagate the value at this time will prematurely create the
  // user profile and will trigger pref access for prefs that are not registered
  // yet (c.f. crbug.com/1459027). Pref values that are received before the user
  // profile is initialized are cached in `pre_profile_initialized_values_` and
  // propagated using the handlers registered in
  // `post_profile_initialized_handlers_` as soon as the user profile is
  // initialized.
  std::map<std::string, base::Value> pre_profile_initialized_values_;
  std::map<
      std::string,
      base::RepeatingCallback<
          void(PrefService*, const std::string& pref_name, base::Value value)>>
      post_profile_initialized_handlers_;
  // `is_profile_initialized_` determines whether a user pref needs to be cached
  // or can be forwarded immediately after the profile is initialized.
  bool is_profile_initialized_ = false;

  raw_ptr<PrefService> local_state_{nullptr};
  std::unique_ptr<CrosapiPrefObserver> doh_mode_observer_;
  std::unique_ptr<CrosapiPrefObserver> doh_templates_observer_;
  std::unique_ptr<CrosapiPrefObserver> mahi_prefs_observer_;
  // Tracks whether the DoH template URI is set via the pref
  // kDnsOverHttpsTemplates (which is deprecated in Lacros) or via the new pref,
  // kDnsOverHttpsEffectiveTemplatesChromeOS.
  bool effective_chromeos_secure_dns_settings_active_ = false;
  // TODO(acostinas, b/328566515) Remove monitoring of the
  // kDnsOverHttpsTemplates after version 126.
  std::unique_ptr<CrosapiPrefObserver> deprecated_doh_templates_observer_;
  std::unique_ptr<CrosapiPrefObserver>
      access_to_get_all_screens_media_in_session_allowed_for_urls_observer_;
};

#endif  // CHROME_BROWSER_LACROS_PREFS_ASH_OBSERVER_H_
