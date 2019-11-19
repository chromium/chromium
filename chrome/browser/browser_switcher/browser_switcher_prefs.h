// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSER_SWITCHER_BROWSER_SWITCHER_PREFS_H_
#define CHROME_BROWSER_BROWSER_SWITCHER_BROWSER_SWITCHER_PREFS_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/callback_list.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/policy/core/common/policy_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "url/gurl.h"

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

class PrefService;
class Profile;

namespace browser_switcher {

// A named pair type.
struct RuleSet {
  RuleSet();
  RuleSet(const RuleSet&);
  ~RuleSet();

  std::vector<std::string> sitelist;
  std::vector<std::string> greylist;
};

// Contains the current state of the prefs related to LBS. For sensitive prefs,
// only respects managed prefs. Also does some type conversions and
// transformations on the prefs (e.g. expanding preset values for
// AlternativeBrowserPath).
class BrowserSwitcherPrefs : public KeyedService,
                             public policy::PolicyService::Observer {
 private:
  using PrefsChangedSignature = void(BrowserSwitcherPrefs*,
                                     const std::vector<std::string>&);

 public:
  using PrefsChangedCallback = base::RepeatingCallback<PrefsChangedSignature>;
  using CallbackSubscription =
      base::CallbackList<PrefsChangedSignature>::Subscription;

  explicit BrowserSwitcherPrefs(Profile* profile);
  ~BrowserSwitcherPrefs() override;

  // KeyedService:
  void Shutdown() override;

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // Returns true if the BrowserSwitcher feature is enabled via policy.
  bool IsEnabled() const;

  // Returns the path to the alternative browser to launch, before
  // substitutions. If the pref is not managed, returns the empty string.
  const std::string& GetAlternativeBrowserPath() const;

  // Returns the arguments to pass to the alternative browser, before
  // substitutions. If the pref is not managed, returns the empty string.
  const std::vector<std::string>& GetAlternativeBrowserParameters() const;

  // Returns true if Chrome should keep at least one tab open after switching.
  bool KeepLastTab() const;

  // Returns the delay before switching, in milliseconds.
  int GetDelay() const;

  // Returns the sitelist + greylist configured directly through Chrome
  // policies. If the pref is not managed, returns an empty vector.
  const RuleSet& GetRules() const;

  // Retrieves or stores the locally cached external sitelist from the
  // PrefStore.
  std::vector<std::string> GetCachedExternalSitelist() const;
  void SetCachedExternalSitelist(const std::vector<std::string>& sitelist);

  // Retrieves or stores the locally cached external greylist from the
  // PrefStore.
  std::vector<std::string> GetCachedExternalGreylist() const;
  void SetCachedExternalGreylist(const std::vector<std::string>& greylist);

#if defined(OS_WIN)
  // Retrieves or stores the locally cached IEEM sitelist from the PrefStore.
  std::vector<std::string> GetCachedIeemSitelist() const;
  void SetCachedIeemSitelist(const std::vector<std::string>& sitelist);
#endif

  // Returns the URL to download for an external XML sitelist. If the pref is
  // not managed, returns an invalid URL.
  GURL GetExternalSitelistUrl() const;

  // Returns the URL to download for an external XML greylist. If the pref is
  // not managed, returns an invalid URL.
  GURL GetExternalGreylistUrl() const;

#if defined(OS_WIN)
  // Returns true if Chrome should download and apply the XML sitelist from
  // IEEM's SiteList policy. If the pref is not managed, returns false.
  bool UseIeSitelist() const;

  // Returns the path to the Chrome executable to launch when switching from IE,
  // before substitutions.
  const std::string& GetChromePath() const;

  // Returns the arguments to pass to Chrome when switching from IE, before
  // substitutions.
  const std::vector<std::string>& GetChromeParameters() const;
#endif

  // policy::PolicyService::Observer
  void OnPolicyUpdated(const policy::PolicyNamespace& ns,
                       const policy::PolicyMap& previous,
                       const policy::PolicyMap& current) override;

  std::unique_ptr<CallbackSubscription> RegisterPrefsChangedCallback(
      PrefsChangedCallback cb);

 protected:
  // For internal use and testing.
  BrowserSwitcherPrefs(PrefService* prefs,
                       policy::PolicyService* policy_service);

 private:
  void RunCallbacksIfDirty();
  void MarkDirty(const std::string& pref_name);

  // Hooks for PrefChangeRegistrar.
  void AlternativeBrowserPathChanged();
  void AlternativeBrowserParametersChanged();
  void UrlListChanged();
  void GreylistChanged();
#if defined(OS_WIN)
  void ChromePathChanged();
  void ChromeParametersChanged();
#endif

  policy::PolicyService* const policy_service_;
  PrefService* const prefs_;

  // We need 2 change registrars because we can't bind 2 observers to the same
  // pref on the same registrar.

  // Listens on *some* prefs, to apply a filter to them
  // (e.g. convert ListValue => vector<string>).
  PrefChangeRegistrar filtering_change_registrar_;

  // Listens on *all* BrowserSwitcher prefs, to notify observers when prefs
  // change as a result of a policy refresh.
  PrefChangeRegistrar notifying_change_registrar_;

  // Type-converted and/or expanded pref values, updated by the
  // PrefChangeRegistrar hooks.
  std::string alt_browser_path_;
  std::vector<std::string> alt_browser_params_;
#if defined(OS_WIN)
  std::string chrome_path_;
  std::vector<std::string> chrome_params_;
#endif

  RuleSet rules_;

  // List of prefs (pref names) that changed since the last policy refresh.
  std::vector<std::string> dirty_prefs_;

  base::CallbackList<PrefsChangedSignature> callback_list_;

  base::WeakPtrFactory<BrowserSwitcherPrefs> weak_ptr_factory_{this};

  DISALLOW_IMPLICIT_CONSTRUCTORS(BrowserSwitcherPrefs);
};

namespace prefs {

extern const char kEnabled[];
extern const char kDelay[];
extern const char kAlternativeBrowserPath[];
extern const char kAlternativeBrowserParameters[];
extern const char kKeepLastTab[];
extern const char kUrlList[];
extern const char kUrlGreylist[];
extern const char kExternalSitelistUrl[];
extern const char kCachedExternalSitelist[];
extern const char kExternalGreylistUrl[];
extern const char kCachedExternalGreylist[];

#if defined(OS_WIN)
extern const char kUseIeSitelist[];
extern const char kCachedIeSitelist[];
extern const char kChromePath[];
extern const char kChromeParameters[];
#endif

}  // namespace prefs

}  // namespace browser_switcher

#endif  // CHROME_BROWSER_BROWSER_SWITCHER_BROWSER_SWITCHER_PREFS_H_
