// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSER_SWITCHER_BROWSER_SWITCHER_PREFS_H_
#define CHROME_BROWSER_BROWSER_SWITCHER_BROWSER_SWITCHER_PREFS_H_

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "base/callback_list.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
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

// This type pre-computes some strings from the URL's parts, so we don't have
// to pay the cost of constructing those strings multiple times. i.e., the same
// NoCopyUrl can be passed to multiple calls of Rule::Matches().
class NoCopyUrl {
 public:
  explicit NoCopyUrl(const GURL& original);
  NoCopyUrl(const NoCopyUrl&) = delete;

  const GURL& original() const { return *original_; }
  std::string_view host_and_port() const { return host_and_port_; }
  std::string_view spec() const { return original_->spec(); }
  std::string_view spec_without_port() const { return spec_without_port_; }

 private:
  const raw_ref<const GURL> original_;
  // If there is a port number, then this is "<host>:<port>". Otherwise, this is
  // just the host.
  std::string host_and_port_;
  // Same as |original_.spec()|, but with the port removed.
  std::string spec_without_port_;
};

// A named pair type, for rules that haven't been pre-processed and
// canonicalized (e.g., just loaded from prefs or policies).
struct RawRuleSet {
  RawRuleSet();
  RawRuleSet(std::vector<std::string>&& sitelist,
             std::vector<std::string>&& greylist);
  RawRuleSet(RawRuleSet&&);
  ~RawRuleSet();

  RawRuleSet& operator=(RawRuleSet&&);

  std::vector<std::string> sitelist;
  std::vector<std::string> greylist;
};

// A single "rule" from a sitelist or greylist, after pre-processing and
// canonicalization.
class Rule {
 public:
  explicit Rule(std::string_view original_rule);
  virtual ~Rule() = default;

  // Returns true if |no_copy_url| matches this rule. Ignores the value of
  // |inverted_|.
  virtual bool Matches(const NoCopyUrl& no_copy_url) const = 0;

  // Returns true if the rule is valid. If this returns false, the rule will be
  // removed from the final list of canonicalized rules.
  virtual bool IsValid() const = 0;

  // Converts the rule to a human-readable string, for display on
  // chrome://browser-switch/internals and serializing to cache.dat.
  virtual std::string ToString() const = 0;

  int priority() const { return priority_; }

  bool inverted() const { return inverted_; }

 private:
  // The "priority" of this rule for making decisions. This should be the length
  // of the original string this rule was parsed from. When 2 rules conflict,
  // the one with higher priority always wins.
  int priority_;

  // Whether this rule is inverted or not. Inverted rules change the decision
  // from "open in alternative browser" to "don't open in alternative browser".
  bool inverted_;
};

// A named pair type, for pre-processed and canonicalized rules.
struct RuleSet {
  RuleSet();
  RuleSet(RuleSet&&);
  ~RuleSet();

  std::vector<std::unique_ptr<Rule>> sitelist;
  std::vector<std::unique_ptr<Rule>> greylist;
};

// Values for the BrowserSwitcherParsingMode policy. Make sure they match the
// values in policy_templates.json.
enum class ParsingMode {
  kDefault = 0,
  kIESiteListMode = 1,
  kMaxValue = kIESiteListMode,  // Always keep up-to-date.
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

  BrowserSwitcherPrefs() = delete;

  explicit BrowserSwitcherPrefs(Profile* profile);

  BrowserSwitcherPrefs(const BrowserSwitcherPrefs&) = delete;
  BrowserSwitcherPrefs& operator=(const BrowserSwitcherPrefs&) = delete;

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

  ParsingMode GetParsingMode() const;

  // Returns the sitelist + greylist configured directly through Chrome
  // policies. If the pref is not managed, returns an empty vector.
  const RuleSet& GetRules() const;

  // Retrieves or stores the locally cached external sitelist from the
  // PrefStore.
  RawRuleSet GetCachedExternalSitelist() const;
  void SetCachedExternalSitelist(const RawRuleSet& sitelist);

  // Retrieves or stores the locally cached external greylist from the
  // PrefStore.
  RawRuleSet GetCachedExternalGreylist() const;
  void SetCachedExternalGreylist(const RawRuleSet& greylist);

#if BUILDFLAG(IS_WIN)
  // Retrieves or stores the locally cached IEEM sitelist from the PrefStore.
  RawRuleSet GetCachedIeemSitelist() const;
  void SetCachedIeemSitelist(const RawRuleSet& sitelist);
#endif

  // Returns the URL to download for an external XML sitelist. If the pref is
  // not managed, returns an invalid URL.
  GURL GetExternalSitelistUrl() const;

  // Returns the URL to download for an external XML greylist. If the pref is
  // not managed, returns an invalid URL.
  GURL GetExternalGreylistUrl() const;

#if BUILDFLAG(IS_WIN)
  // Returns true if Chrome should download and apply the XML sitelist from
  // IEEM's SiteList policy. If the pref is not managed, returns false.
  bool UseIeSitelist() const;

  // Returns the path to the Chrome executable to launch when switching from IE,
  // before substitutions.
  const base::FilePath& GetChromePath() const;

  // Returns the arguments to pass to Chrome when switching from IE, before
  // substitutions.
  const std::vector<std::string>& GetChromeParameters() const;
#endif

  // policy::PolicyService::Observer
  void OnPolicyUpdated(const policy::PolicyNamespace& ns,
                       const policy::PolicyMap& previous,
                       const policy::PolicyMap& current) override;

  base::CallbackListSubscription RegisterPrefsChangedCallback(
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
  void ParsingModeChanged();
  void UrlListChanged();
  void GreylistChanged();
#if BUILDFLAG(IS_WIN)
  void ChromePathChanged();
  void ChromeParametersChanged();
#endif

  const raw_ptr<policy::PolicyService> policy_service_;
  const raw_ptr<PrefService> prefs_;

  // We need 2 change registrars because we can't bind 2 observers to the same
  // pref on the same registrar.

  // Listens on *some* prefs, to apply a filter to them
  // (e.g. convert Value::List => vector<string>).
  PrefChangeRegistrar filtering_change_registrar_;

  // Listens on *all* BrowserSwitcher prefs, to notify observers when prefs
  // change as a result of a policy refresh.
  PrefChangeRegistrar notifying_change_registrar_;

  // Type-converted and/or expanded pref values, updated by the
  // PrefChangeRegistrar hooks.
  std::string alt_browser_path_;
  std::vector<std::string> alt_browser_params_;
  ParsingMode parsing_mode_ = ParsingMode::kDefault;
#if BUILDFLAG(IS_WIN)
  base::FilePath chrome_path_;
  std::vector<std::string> chrome_params_;
#endif

  // Rules from the BrowserSwitcherUrlList and BrowserSwitcherGreylist policies.
  //
  // Other rules are parsed from XML, and stored in BrowserSwitcherSitelist
  // instead of here.
  RuleSet rules_;

  // List of prefs (pref names) that changed since the last policy refresh.
  std::vector<std::string> dirty_prefs_;

  base::RepeatingCallbackList<PrefsChangedSignature> callback_list_;

  base::WeakPtrFactory<BrowserSwitcherPrefs> weak_ptr_factory_{this};
};

namespace prefs {

extern const char kEnabled[];
extern const char kDelay[];
extern const char kAlternativeBrowserPath[];
extern const char kAlternativeBrowserParameters[];
extern const char kKeepLastTab[];
extern const char kParsingMode[];
extern const char kUrlList[];
extern const char kUrlGreylist[];
extern const char kExternalSitelistUrl[];
extern const char kCachedExternalSitelist[];
extern const char kCachedExternalSitelistGreylist[];
extern const char kExternalGreylistUrl[];
extern const char kCachedExternalGreylist[];

#if BUILDFLAG(IS_WIN)
extern const char kUseIeSitelist[];
extern const char kCachedIeSitelist[];
extern const char kCachedIeSitelistGreylist[];
extern const char kChromePath[];
extern const char kChromeParameters[];
#endif

}  // namespace prefs

}  // namespace browser_switcher

#endif  // CHROME_BROWSER_BROWSER_SWITCHER_BROWSER_SWITCHER_PREFS_H_
