// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_switcher/browser_switcher_prefs.h"

#include <string_view>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "chrome/browser/browser_switcher/browser_switcher_sitelist.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>
#endif

namespace browser_switcher {

namespace {

std::vector<std::string> GetListPref(PrefService* prefs,
                                     const std::string& pref_name) {
  std::vector<std::string> list;
  if (pref_name.empty())
    return list;
  for (const auto& value : prefs->GetList(pref_name))
    list.push_back(value.GetString());
  return list;
}

RawRuleSet GetCachedRules(PrefService* prefs,
                          const std::string& sitelist_pref_name,
                          const std::string& greylist_pref_name) {
  return RawRuleSet(GetListPref(prefs, sitelist_pref_name),
                    GetListPref(prefs, greylist_pref_name));
}

void SetListPref(PrefService* prefs,
                 const std::string& pref_name,
                 const std::vector<std::string>& list) {
  if (pref_name.empty())
    return;
  base::Value::List list_value;
  for (const auto& str : list)
    list_value.Append(str);
  prefs->SetList(pref_name, std::move(list_value));
}

void SetCachedRules(PrefService* prefs,
                    const std::string& sitelist_pref_name,
                    const std::string& greylist_pref_name,
                    const RawRuleSet& rules) {
  SetListPref(prefs, sitelist_pref_name, rules.sitelist);
  SetListPref(prefs, greylist_pref_name, rules.greylist);
}

}  // namespace

NoCopyUrl::NoCopyUrl(const GURL& original) : original_(original) {
  spec_without_port_ = original_->spec();

  int int_port = original_->IntPort();
  std::string port_suffix;
  if (int_port != url::PORT_UNSPECIFIED) {
    port_suffix = base::StrCat({":", base::NumberToString(int_port)});
    base::ReplaceSubstringsAfterOffset(&spec_without_port_, 0, port_suffix,
                                       std::string_view());
  }

  host_and_port_ = base::StrCat({original.host(), port_suffix});
}

Rule::Rule(std::string_view original_rule)
    : priority_(original_rule.size()),
      inverted_(base::StartsWith(original_rule, "!")) {}

RawRuleSet::RawRuleSet() = default;
RawRuleSet::RawRuleSet(RawRuleSet&&) = default;
RawRuleSet::~RawRuleSet() = default;

RawRuleSet::RawRuleSet(std::vector<std::string>&& sitelist_,
                       std::vector<std::string>&& greylist_)
    : sitelist(std::move(sitelist_)), greylist(std::move(greylist_)) {}

RawRuleSet& RawRuleSet::operator=(RawRuleSet&& that) = default;

RuleSet::RuleSet() = default;
RuleSet::RuleSet(RuleSet&&) = default;
RuleSet::~RuleSet() = default;

BrowserSwitcherPrefs::BrowserSwitcherPrefs(Profile* profile)
    : BrowserSwitcherPrefs(
          profile->GetPrefs(),
          profile->GetProfilePolicyConnector()->policy_service()) {}

BrowserSwitcherPrefs::BrowserSwitcherPrefs(
    PrefService* prefs,
    policy::PolicyService* policy_service)
    : policy_service_(policy_service), prefs_(prefs) {
  filtering_change_registrar_.Init(prefs_);

  const struct {
    const char* pref_name;
    base::RepeatingCallback<void(BrowserSwitcherPrefs*)> callback;
  } hooks[] = {
    {prefs::kAlternativeBrowserPath,
     base::BindRepeating(&BrowserSwitcherPrefs::AlternativeBrowserPathChanged)},
    {prefs::kAlternativeBrowserParameters,
     base::BindRepeating(
         &BrowserSwitcherPrefs::AlternativeBrowserParametersChanged)},
    {prefs::kParsingMode,
     base::BindRepeating(&BrowserSwitcherPrefs::ParsingModeChanged)},
    {prefs::kUrlList,
     base::BindRepeating(&BrowserSwitcherPrefs::UrlListChanged)},
    {prefs::kUrlGreylist,
     base::BindRepeating(&BrowserSwitcherPrefs::GreylistChanged)},
#if BUILDFLAG(IS_WIN)
    {prefs::kChromePath,
     base::BindRepeating(&BrowserSwitcherPrefs::ChromePathChanged)},
    {prefs::kChromeParameters,
     base::BindRepeating(&BrowserSwitcherPrefs::ChromeParametersChanged)},
#endif
  };

  // Listen for pref changes, and run all the hooks once to initialize state.
  for (const auto& hook : hooks) {
    auto callback = base::BindRepeating(hook.callback, base::Unretained(this));
    filtering_change_registrar_.Add(hook.pref_name, callback);
    callback.Run();
  }

  // When any pref changes, mark this object as 'dirty' for the purpose of
  // triggering observers.
  notifying_change_registrar_.Init(prefs_);
  const char* all_prefs[] = {
    prefs::kEnabled,
    prefs::kAlternativeBrowserPath,
    prefs::kAlternativeBrowserParameters,
    prefs::kKeepLastTab,
    prefs::kParsingMode,
    prefs::kUrlList,
    prefs::kUrlGreylist,
    prefs::kExternalSitelistUrl,
    prefs::kExternalGreylistUrl,
#if BUILDFLAG(IS_WIN)
    prefs::kUseIeSitelist,
    prefs::kChromePath,
    prefs::kChromeParameters,
#endif
  };
  for (const char* pref_name : all_prefs) {
    notifying_change_registrar_.Add(
        pref_name, base::BindRepeating(&BrowserSwitcherPrefs::MarkDirty,
                                       base::Unretained(this)));
  }

  if (policy_service_)
    policy_service_->AddObserver(policy::POLICY_DOMAIN_CHROME, this);
}

BrowserSwitcherPrefs::~BrowserSwitcherPrefs() = default;

void BrowserSwitcherPrefs::Shutdown() {
  if (policy_service_)
    policy_service_->RemoveObserver(policy::POLICY_DOMAIN_CHROME, this);
}

// static
void BrowserSwitcherPrefs::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(prefs::kEnabled, false);
  registry->RegisterIntegerPref(prefs::kDelay, 0);
  registry->RegisterStringPref(prefs::kAlternativeBrowserPath, "");
  registry->RegisterListPref(prefs::kAlternativeBrowserParameters);
  registry->RegisterBooleanPref(prefs::kKeepLastTab, true);
  registry->RegisterIntegerPref(prefs::kParsingMode, 0);
  registry->RegisterListPref(prefs::kUrlList);
  registry->RegisterListPref(prefs::kUrlGreylist);
  registry->RegisterStringPref(prefs::kExternalSitelistUrl, "");
  registry->RegisterListPref(prefs::kCachedExternalSitelist);
  registry->RegisterListPref(prefs::kCachedExternalSitelistGreylist);
  registry->RegisterStringPref(prefs::kExternalGreylistUrl, "");
  registry->RegisterListPref(prefs::kCachedExternalGreylist);
#if BUILDFLAG(IS_WIN)
  registry->RegisterBooleanPref(prefs::kUseIeSitelist, false);
  registry->RegisterListPref(prefs::kCachedIeSitelist);
  registry->RegisterListPref(prefs::kCachedIeSitelistGreylist);
  registry->RegisterStringPref(prefs::kChromePath, "");
  registry->RegisterListPref(prefs::kChromeParameters);
#endif
}

bool BrowserSwitcherPrefs::IsEnabled() const {
  return prefs_->GetBoolean(prefs::kEnabled) &&
         prefs_->IsManagedPreference(prefs::kEnabled);
}

const std::string& BrowserSwitcherPrefs::GetAlternativeBrowserPath() const {
  return alt_browser_path_;
}

const std::vector<std::string>&
BrowserSwitcherPrefs::GetAlternativeBrowserParameters() const {
  return alt_browser_params_;
}

bool BrowserSwitcherPrefs::KeepLastTab() const {
  return prefs_->GetBoolean(prefs::kKeepLastTab);
}

int BrowserSwitcherPrefs::GetDelay() const {
  return prefs_->GetInteger(prefs::kDelay);
}

ParsingMode BrowserSwitcherPrefs::GetParsingMode() const {
  return parsing_mode_;
}

const RuleSet& BrowserSwitcherPrefs::GetRules() const {
  return rules_;
}

RawRuleSet BrowserSwitcherPrefs::GetCachedExternalSitelist() const {
  return GetCachedRules(prefs_, prefs::kCachedExternalSitelist,
                        prefs::kCachedExternalSitelistGreylist);
}

void BrowserSwitcherPrefs::SetCachedExternalSitelist(const RawRuleSet& rules) {
  SetCachedRules(prefs_, prefs::kCachedExternalSitelist,
                 prefs::kCachedExternalSitelistGreylist, rules);
}

RawRuleSet BrowserSwitcherPrefs::GetCachedExternalGreylist() const {
  return GetCachedRules(prefs_, std::string(), prefs::kCachedExternalGreylist);
}

void BrowserSwitcherPrefs::SetCachedExternalGreylist(const RawRuleSet& rules) {
  SetCachedRules(prefs_, std::string(), prefs::kCachedExternalGreylist, rules);
}

#if BUILDFLAG(IS_WIN)
RawRuleSet BrowserSwitcherPrefs::GetCachedIeemSitelist() const {
  return GetCachedRules(prefs_, prefs::kCachedIeSitelist,
                        prefs::kCachedIeSitelistGreylist);
}

void BrowserSwitcherPrefs::SetCachedIeemSitelist(const RawRuleSet& rules) {
  SetCachedRules(prefs_, prefs::kCachedIeSitelist,
                 prefs::kCachedIeSitelistGreylist, rules);
}
#endif

GURL BrowserSwitcherPrefs::GetExternalSitelistUrl() const {
  if (!IsEnabled() || !prefs_->IsManagedPreference(prefs::kExternalSitelistUrl))
    return GURL();
  return GURL(prefs_->GetString(prefs::kExternalSitelistUrl));
}

GURL BrowserSwitcherPrefs::GetExternalGreylistUrl() const {
  if (!IsEnabled() || !prefs_->IsManagedPreference(prefs::kExternalGreylistUrl))
    return GURL();
  return GURL(prefs_->GetString(prefs::kExternalGreylistUrl));
}

#if BUILDFLAG(IS_WIN)
bool BrowserSwitcherPrefs::UseIeSitelist() const {
  if (!IsEnabled() || !prefs_->IsManagedPreference(prefs::kUseIeSitelist))
    return false;
  return prefs_->GetBoolean(prefs::kUseIeSitelist);
}

const base::FilePath& BrowserSwitcherPrefs::GetChromePath() const {
  return chrome_path_;
}

const std::vector<std::string>& BrowserSwitcherPrefs::GetChromeParameters()
    const {
  return chrome_params_;
}
#endif

void BrowserSwitcherPrefs::OnPolicyUpdated(const policy::PolicyNamespace& ns,
                                           const policy::PolicyMap& previous,
                                           const policy::PolicyMap& current) {
  // Let all the other policy observers run first, so that prefs are up-to-date
  // when we run our own callbacks.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&BrowserSwitcherPrefs::RunCallbacksIfDirty,
                                weak_ptr_factory_.GetWeakPtr()));
}

base::CallbackListSubscription
BrowserSwitcherPrefs::RegisterPrefsChangedCallback(
    BrowserSwitcherPrefs::PrefsChangedCallback cb) {
  return callback_list_.Add(cb);
}

void BrowserSwitcherPrefs::RunCallbacksIfDirty() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!dirty_prefs_.empty())
    callback_list_.Notify(this, dirty_prefs_);
  dirty_prefs_.clear();
}

void BrowserSwitcherPrefs::MarkDirty(const std::string& pref_name) {
  dirty_prefs_.push_back(pref_name);
}

void BrowserSwitcherPrefs::AlternativeBrowserPathChanged() {
  alt_browser_path_.clear();
  if (prefs_->IsManagedPreference(prefs::kAlternativeBrowserPath))
    alt_browser_path_ = prefs_->GetString(prefs::kAlternativeBrowserPath);
}

void BrowserSwitcherPrefs::AlternativeBrowserParametersChanged() {
  alt_browser_params_.clear();
  if (!prefs_->IsManagedPreference(prefs::kAlternativeBrowserParameters))
    return;
  const base::Value::List& params =
      prefs_->GetList(prefs::kAlternativeBrowserParameters);
  for (const auto& param : params) {
    std::string param_string = param.GetString();
    alt_browser_params_.push_back(param_string);
  }
}

void BrowserSwitcherPrefs::ParsingModeChanged() {
  ParsingMode old_parsing_mode = parsing_mode_;
  parsing_mode_ =
      static_cast<ParsingMode>(prefs_->GetInteger(prefs::kParsingMode));
  if (parsing_mode_ < ParsingMode::kDefault ||
      parsing_mode_ > ParsingMode::kMaxValue) {
    LOG(WARNING) << "Unknown BrowserSwitcherParsingMode value "
                 << static_cast<int>(parsing_mode_)
                 << ". Falling back to 'Default' parsing mode.";
    parsing_mode_ = ParsingMode::kDefault;
  }

  if (parsing_mode_ != old_parsing_mode) {
    // Parsing mode just changed, re-canonicalize rules.
    UrlListChanged();
    GreylistChanged();
  }
}

void BrowserSwitcherPrefs::UrlListChanged() {
  rules_.sitelist.clear();

  if (!prefs_->IsManagedPreference(prefs::kUrlList))
    return;

  UMA_HISTOGRAM_COUNTS_100000("BrowserSwitcher.UrlListSize",
                              prefs_->GetList(prefs::kUrlList).size());

  bool has_wildcard = false;
  for (const auto& url : prefs_->GetList(prefs::kUrlList)) {
    std::unique_ptr<Rule> rule =
        CanonicalizeRule(url.GetString(), parsing_mode_);
    if (rule)
      rules_.sitelist.push_back(std::move(rule));
    if (url.GetString() == "*")
      has_wildcard = true;
  }

  UMA_HISTOGRAM_BOOLEAN("BrowserSwitcher.UrlListWildcard", has_wildcard);
}

void BrowserSwitcherPrefs::GreylistChanged() {
  rules_.greylist.clear();

  // This pref is sensitive. Only set through policies.
  if (!prefs_->IsManagedPreference(prefs::kUrlGreylist))
    return;

  const base::Value::List& url_gray_list = prefs_->GetList(prefs::kUrlGreylist);
  UMA_HISTOGRAM_COUNTS_100000("BrowserSwitcher.GreylistSize",
                              url_gray_list.size());

  bool has_wildcard = false;
  for (const auto& url : url_gray_list) {
    std::unique_ptr<Rule> rule =
        CanonicalizeRule(url.GetString(), parsing_mode_);
    if (rule)
      rules_.greylist.push_back(std::move(rule));
    if (url.GetString() == "*")
      has_wildcard = true;
  }

  UMA_HISTOGRAM_BOOLEAN("BrowserSwitcher.UrlListWildcard", has_wildcard);
}

#if BUILDFLAG(IS_WIN)
void BrowserSwitcherPrefs::ChromePathChanged() {
  chrome_path_.clear();
  if (prefs_->IsManagedPreference(prefs::kChromePath))
    chrome_path_ = prefs_->GetFilePath(prefs::kChromePath);
#if BUILDFLAG(IS_WIN)
  if (chrome_path_.empty()) {
    base::FilePath::CharType chrome_path[MAX_PATH];
    ::GetModuleFileName(NULL, chrome_path, ARRAYSIZE(chrome_path));
    chrome_path_ = base::FilePath(chrome_path);
  }
#endif
}

void BrowserSwitcherPrefs::ChromeParametersChanged() {
  chrome_params_.clear();
  if (!prefs_->IsManagedPreference(prefs::kChromeParameters))
    return;
  const base::Value::List& params = prefs_->GetList(prefs::kChromeParameters);
  for (const auto& param : params) {
    std::string param_string = param.GetString();
    chrome_params_.push_back(param_string);
  }
}
#endif

namespace prefs {

// Path to the executable of the alternative browser, or one of "${chrome}",
// "${ie}", "${firefox}", "${opera}", "${safari}".
const char kAlternativeBrowserPath[] =
    "browser_switcher.alternative_browser_path";

// Arguments to pass to the alternative browser when invoking it via
// |ShellExecute()|.
const char kAlternativeBrowserParameters[] =
    "browser_switcher.alternative_browser_parameters";

// If true, always keep at least one tab open after switching.
const char kKeepLastTab[] = "browser_switcher.keep_last_tab";

// List of host domain names to be opened in an alternative browser.
const char kUrlList[] = "browser_switcher.url_list";

// List of hosts that should not trigger a transition in either browser.
const char kUrlGreylist[] = "browser_switcher.url_greylist";

// URL with an external XML sitelist file to load. The cached ruleset has 2
// parts (sitelist and greylist).
const char kExternalSitelistUrl[] = "browser_switcher.external_sitelist_url";
const char kCachedExternalSitelist[] =
    "browser_switcher.cached_external_sitelist";
const char kCachedExternalSitelistGreylist[] =
    "browser_switcher.cached_external_sitelist_greylist";

// URL with an external XML greylist file to load. Unlike the other cached XML
// rulesets, this one is just a greylist (rather than a pair of lists).
const char kExternalGreylistUrl[] = "browser_switcher.external_greylist_url";
const char kCachedExternalGreylist[] =
    "browser_switcher.cached_external_greylist";

#if BUILDFLAG(IS_WIN)
// If set to true, use the IE Enterprise Mode Sitelist policy. The cached
// ruleset has 2 parts (sitelist and greylist).
const char kUseIeSitelist[] = "browser_switcher.use_ie_sitelist";
const char kCachedIeSitelist[] = "browser_switcher.cached_ie_sitelist";
const char kCachedIeSitelistGreylist[] =
    "browser_switcher.cached_ie_sitelist_greylist";

// Path to the Chrome executable for the alternative browser.
const char kChromePath[] = "browser_switcher.chrome_path";

// Arguments the alternative browser should pass to Chrome when launching it.
const char kChromeParameters[] = "browser_switcher.chrome_parameters";
#endif

// Disable browser_switcher unless this is set to true.
const char kEnabled[] = "browser_switcher.enabled";

// How long to wait on chrome://browser-switch (milliseconds).
const char kDelay[] = "browser_switcher.delay";

// Behavior switch for BrowserSwitcherSitelist.
const char kParsingMode[] = "browser_switcher.parsing_mode";

}  // namespace prefs
}  // namespace browser_switcher
