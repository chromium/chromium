// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_switcher/browser_switcher_sitelist.h"

#include <string.h>

#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/ranges/algorithm.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "chrome/browser/browser_switcher/browser_switcher_prefs.h"
#include "chrome/browser/browser_switcher/ieem_sitelist_parser.h"
#include "components/prefs/pref_service.h"
#include "components/url_formatter/url_fixer.h"
#include "third_party/re2/src/re2/re2.h"
#include "url/gurl.h"
#include "url/url_util.h"

namespace browser_switcher {

namespace {

// Find the position of |token| inside |input|, if present. Ignore case for
// ASCII characters.
//
// If |token| is not in |input|, return a pointer to the null-byte at the end
// of |input|.
auto StringFindInsensitiveASCII(std::string_view input,
                                std::string_view token) {
  return base::ranges::search(input, token, std::equal_to<>(),
                              &base::ToLowerASCII<char>,
                              &base::ToLowerASCII<char>);
}

// Checks if the omitted prefix for a non-fully specific prefix is one of the
// expected parts that are allowed to be omitted (e.g. "https://").
bool IsValidPrefix(std::string_view prefix) {
  static re2::LazyRE2 re = {"(https?|file):(//)?"};
  return prefix.empty() || re2::RE2::FullMatch(prefix, *re);
}

// Checks whether |patterns| contains a pattern that matches |url|, and returns
// the longest matching pattern. If there are no matches, an empty pattern is
// returned.
//
// If |contains_inverted_matches| is true, treat patterns that start with "!" as
// inverted matches.
const Rule* MatchUrlToList(const NoCopyUrl& url,
                           const std::vector<std::unique_ptr<Rule>>& rules,
                           bool contains_inverted_matches) {
  const Rule* reason = nullptr;
  for (const std::unique_ptr<Rule>& rule : rules) {
    DCHECK(rule);
    if (reason && rule->priority() <= reason->priority())
      continue;
    if (rule->inverted() && !contains_inverted_matches)
      continue;
    if (rule->Matches(url))
      reason = rule.get();
  }
  return reason;
}

// Rules that are just an "*" are the most simple: they just return true all the
// time, regardless of ParsingMode.
class WildcardRule : public Rule {
 public:
  WildcardRule() : Rule("*") {}
  ~WildcardRule() override = default;

  bool Matches(const NoCopyUrl& url) const override { return true; }

  bool IsValid() const override { return true; }

  std::string ToString() const override { return "*"; }
};

// Rules with ParsingMode::kDefault. They treat rules with/without a '/'
// separately. They do some pre-processing to come up with a |canonical_| rule
// string, then some simple string searches.
class DefaultModeRule : public Rule {
 public:
  explicit DefaultModeRule(std::string_view original_rule)
      : Rule(original_rule) {
    canonical_ = std::string(original_rule);

    // Drop the leading "!", if present.
    if (inverted())
      canonical_ = canonical_.substr(1);

    if (canonical_.find("/") == std::string::npos) {
      // No "/" in the string. It's a hostnmae or wildcard, so just convert to
      // lowercase.
      canonical_ = base::ToLowerASCII(canonical_);
      return;
    }

    // The string has a "/" in it. It could be:
    // - "//example.com/abc", convert hostname to lowercase
    // - "example.com/abc", treat same as "//example.com/abc"
    // - "http://example.com/abc", convert hostname and scheme to lowercase
    // - "/abc", keep capitalization

    if (base::StartsWith(canonical_, "/") &&
        !base::StartsWith(canonical_, "//")) {
      // Rule starts with a single slash, e.g. "/abc". Don't change case.
      return;
    }

    if (canonical_.find("/") != 0 &&
        canonical_.find("://") == std::string::npos) {
      // Transform "example.com/abc" => "//example.com/abc".
      canonical_.insert(0, "//");
    }

    // For patterns that include a "/": parse the URL to get the proper
    // capitalization (for scheme/hostname).
    //
    // To properly parse URLs with no scheme, we need a valid base URL. We use
    // "ftp://XXX/", which is a valid URL with an unsupported scheme. That
    // way, parsing still succeeds, and we can easily know when the scheme
    // isn't part of the original pattern (and omit it from the output).
    const char* placeholder_scheme = "ftp:";
    std::string placeholder = base::StrCat({placeholder_scheme, "//XXX/"});
    GURL base_url(placeholder);

    GURL relative_url = base_url.Resolve(canonical_);
    std::string_view spec = relative_url.possibly_invalid_spec();

    // The parsed URL might start with "ftp://XXX/" or "ftp://". Remove that
    // prefix.
    if (base::StartsWith(spec, placeholder,
                         base::CompareCase::INSENSITIVE_ASCII)) {
      spec = spec.substr(placeholder.size());
    }
    if (base::StartsWith(spec, placeholder_scheme,
                         base::CompareCase::INSENSITIVE_ASCII)) {
      spec = spec.substr(strlen(placeholder_scheme));
    }

    canonical_ = std::string(spec);
  }

  ~DefaultModeRule() override = default;

  bool Matches(const NoCopyUrl& url) const override {
    std::string_view pattern = canonical_;

    if (pattern.find('/') != std::string_view::npos) {
      // Check that the prefix is valid. The URL's hostname/scheme have
      // already been case-normalized, so that part of the URL is always
      // case-insensitive.
      size_t pos = url.spec().find(pattern);
      if (pos != std::string_view::npos &&
          IsValidPrefix(std::string_view(url.spec().data(), pos))) {
        return true;
      }
      if (!url.spec_without_port().empty()) {
        pos = url.spec_without_port().find(pattern);
        return pos != std::string_view::npos &&
               IsValidPrefix(
                   std::string_view(url.spec_without_port().data(), pos));
      }
      return false;
    }

    // Compare hosts and ports, case-insensitive.
    auto it = StringFindInsensitiveASCII(url.host_and_port(), pattern);
    return it != url.host_and_port().end();
  }

  bool IsValid() const override { return true; }

  std::string ToString() const override {
    if (inverted())
      return "!" + canonical_;
    return canonical_;
  }

 private:
  // The canonical version of the rule, with the leading "!" removed if it's
  // inverted.
  std::string canonical_;
};

// Rules with ParsingMode::kIESiteListMode. They treat rules the same regardless
// of whether a '/' is present. They parse the rule as a URL, then split it
// into scheme, host, port, and path parts. They compare each of these parts
// with the URL to be matched.
class IESiteListModeRule : public Rule {
 public:
  explicit IESiteListModeRule(std::string_view original_rule)
      : Rule(original_rule) {
    // Parse the string as a URL and extract its parts.
    //
    // Some parts of the URL will be dropped, to match IE/Edge behavior:
    //   - username
    //   - password
    //   - query
    //   - fragment

    // Drop the leading "!", if present.
    if (inverted())
      original_rule = original_rule.substr(1);

    // Rules with leading slashes are interpreted as file:// URLs on POSIX
    // systems. To make it more consistent with Windows, remove the leading
    // slashes.
    //
    // Only remove the first leading slash, to be consistent with Edge (which
    // *does* parse it as a file:// URL if there are 2 slashes).
    if (base::StartsWith(original_rule, "/"))
      original_rule = original_rule.substr(1);

    // Parse as a URL. This is more relaxed than GURL's constructor, e.g. it
    // adds http:// if the scheme is missing.
    //
    // This lets us parse strings like "example.com", even though they're not
    // fully-specified URLs (missing scheme and path).
    GURL url = url_formatter::FixupURL(std::string(original_rule), "");

    if (!url.is_valid() ||
        (!url.SchemeIsHTTPOrHTTPS() && !url.SchemeIsFile())) {
      // The rule is invalid, so it won't match anything. Continue parsing it,
      // in case we want to print it later for debugging/troubleshooting.
      valid_ = false;
    }

    // If it starts with http:// or https://, preserve the scheme. Otherwise,
    // use a wildcard ("*") as the scheme.
    //
    // "http://" may have been added by FixupUrl(), so look for it in the
    // original string instead.
    if (valid_ && (StringFindInsensitiveASCII(original_rule, "http://") ==
                       original_rule.begin() ||
                   StringFindInsensitiveASCII(original_rule, "https://") ==
                       original_rule.begin() ||
                   url.SchemeIsFile())) {
      scheme_ = url.scheme();
    }

    if (url.has_host())
      host_ = url.host();

    if (url.has_port())
      port_ = url.IntPort();

    // Make sure |path_| always has at least the leading slash.
    if (url.has_path() && !url.path_piece().empty())
      path_ = base::ToLowerASCII(url.path());
    else
      path_ = "/";
  }

  ~IESiteListModeRule() override = default;

  bool Matches(const NoCopyUrl& no_copy_url) const override {
    DCHECK(valid_);

    const GURL& url = no_copy_url.original();
    // Compare schemes, if present in the rule.
    if (scheme_ && url.scheme_piece() != *scheme_) {
      return false;
    }

    // Compare hosts.
    if (!url::DomainIs(url.host_piece(), host_))
      return false;

    // Compare ports, if present in the rule.
    if (port_ && url.IntPort() != *port_)
      return false;

    // Compare paths, case-insensitively. They must match at the beginning.
    auto pos = StringFindInsensitiveASCII(url.path_piece(), path_);
    if (pos != url.path_piece().begin())
      return false;

    return true;
  }

  bool IsValid() const override { return valid_; }

  // Typical return value looks like "*://example.com:8000/path".
  std::string ToString() const override {
    DCHECK(valid_);

    std::ostringstream out;

    if (inverted())
      out << "!";

    // <scheme>://
    if (scheme_)
      out << *scheme_;
    else
      out << "*";
    out << "://";

    // <host>:<port>
    out << host_;
    if (port_)
      out << ":" << *port_;

    // <path>
    out << path_;

    return out.str();
  }

 private:
  std::optional<std::string> scheme_;
  std::string host_;
  std::optional<int> port_;
  // Always at least a "/".
  std::string path_;

  bool valid_ = true;
};

}  // namespace

std::unique_ptr<Rule> CanonicalizeRule(std::string_view original_rule,
                                       ParsingMode parsing_mode) {
  std::unique_ptr<Rule> rule;

  if (original_rule == "*") {
    rule = std::make_unique<WildcardRule>();
  } else {
    switch (parsing_mode) {
      case ParsingMode::kDefault:
        rule = std::make_unique<DefaultModeRule>(original_rule);
        break;
      case ParsingMode::kIESiteListMode:
        rule = std::make_unique<IESiteListModeRule>(original_rule);
        break;
      default:
        NOTREACHED_IN_MIGRATION();
    }
  }

  if (!rule || !rule->IsValid())
    return nullptr;
  else
    return rule;
}

Decision::Decision(Action action_, Reason reason_, const Rule* matching_rule_)
    : action(action_), reason(reason_), matching_rule(matching_rule_) {}

Decision::Decision() = default;
Decision::Decision(Decision&) = default;
Decision::Decision(Decision&&) = default;

bool Decision::operator==(const Decision& that) const {
  if (action != that.action || reason != that.reason)
    return false;
  if (matching_rule == that.matching_rule)
    return true;
  if (!matching_rule || !that.matching_rule)
    return false;
  return matching_rule->ToString() == that.matching_rule->ToString();
}

BrowserSwitcherSitelist::~BrowserSwitcherSitelist() = default;

bool BrowserSwitcherSitelist::ShouldSwitch(const GURL& url) const {
  return GetDecision(url).action == kGo;
}

BrowserSwitcherSitelistImpl::BrowserSwitcherSitelistImpl(
    BrowserSwitcherPrefs* prefs)
    : prefs_(prefs) {
  prefs_changed_subscription_ = prefs_->RegisterPrefsChangedCallback(
      base::BindRepeating(&BrowserSwitcherSitelistImpl::OnPrefsChanged,
                          base::Unretained(this)));
}

BrowserSwitcherSitelistImpl::~BrowserSwitcherSitelistImpl() = default;

Decision BrowserSwitcherSitelistImpl::GetDecision(const GURL& url) const {
  // Don't record metrics for LBS non-users.
  if (!IsActive())
    return {kStay, kDisabled, nullptr};

  Decision decision = GetDecisionImpl(url);
  UMA_HISTOGRAM_BOOLEAN("BrowserSwitcher.Decision", decision.action == kGo);
  return decision;
}

Decision BrowserSwitcherSitelistImpl::GetDecisionImpl(const GURL& url) const {
  SCOPED_UMA_HISTOGRAM_TIMER("BrowserSwitcher.DecisionTime");

  if (!url.SchemeIsHTTPOrHTTPS() && !url.SchemeIsFile()) {
    return {kStay, kProtocol, nullptr};
  }

  NoCopyUrl no_copy_url(url);
  const RuleSet* rulesets[] = {&prefs_->GetRules(), &ieem_sitelist_,
                               &external_sitelist_, &external_greylist_};

  const Rule* reason_to_go = nullptr;
  for (const RuleSet* rules : rulesets) {
    const Rule* match = MatchUrlToList(no_copy_url, rules->sitelist,
                                       /*contains_inverted_matches=*/true);
    if (!match)
      continue;
    if (!reason_to_go || match->priority() > reason_to_go->priority())
      reason_to_go = match;
  }

  // If sitelists don't match, no need to check the greylists.
  if (!reason_to_go)
    return {kStay, kDefault, nullptr};
  if (reason_to_go->inverted())
    return {kStay, kSitelist, reason_to_go};

  const Rule* reason_to_stay = nullptr;
  for (const RuleSet* rules : rulesets) {
    const Rule* match = MatchUrlToList(no_copy_url, rules->greylist,
                                       /*contains_inverted_matches=*/false);
    if (!match)
      continue;
    if (!reason_to_stay || match->priority() > reason_to_stay->priority())
      reason_to_stay = match;
  }

  if (reason_to_go->priority() <= 1 && reason_to_stay)
    return {kStay, kGreylist, reason_to_stay};

  if (!reason_to_stay || reason_to_go->priority() >= reason_to_stay->priority())
    return {kGo, kSitelist, reason_to_go};
  else
    return {kStay, kGreylist, reason_to_stay};
}

void BrowserSwitcherSitelistImpl::SetIeemSitelist(RawRuleSet&& rules) {
  UMA_HISTOGRAM_COUNTS_100000("BrowserSwitcher.IeemSitelistSize",
                              rules.sitelist.size());
  StoreRules(ieem_sitelist_, rules);
  original_ieem_sitelist_ = std::move(rules);
}

void BrowserSwitcherSitelistImpl::SetExternalSitelist(RawRuleSet&& rules) {
  UMA_HISTOGRAM_COUNTS_100000("BrowserSwitcher.ExternalSitelistSize",
                              rules.sitelist.size());
  StoreRules(external_sitelist_, rules);
  original_external_sitelist_ = std::move(rules);
}

void BrowserSwitcherSitelistImpl::SetExternalGreylist(RawRuleSet&& rules) {
  UMA_HISTOGRAM_COUNTS_100000("BrowserSwitcher.ExternalGreylistSize",
                              rules.sitelist.size());
  DCHECK(rules.sitelist.empty());
  StoreRules(external_greylist_, rules);
  original_external_greylist_ = std::move(rules);
}

const RuleSet* BrowserSwitcherSitelistImpl::GetIeemSitelist() const {
  return &ieem_sitelist_;
}

const RuleSet* BrowserSwitcherSitelistImpl::GetExternalSitelist() const {
  return &external_sitelist_;
}

const RuleSet* BrowserSwitcherSitelistImpl::GetExternalGreylist() const {
  return &external_greylist_;
}

void BrowserSwitcherSitelistImpl::StoreRules(RuleSet& dst,
                                             const RawRuleSet& src) {
  dst.sitelist.clear();
  dst.greylist.clear();
  ParsingMode parsing_mode = prefs_->GetParsingMode();
  for (const std::string& original_rule : src.sitelist) {
    std::unique_ptr<Rule> rule = CanonicalizeRule(original_rule, parsing_mode);
    if (rule)
      dst.sitelist.push_back(std::move(rule));
  }
  for (const std::string& original_rule : src.greylist) {
    std::unique_ptr<Rule> rule = CanonicalizeRule(original_rule, parsing_mode);
    if (rule)
      dst.greylist.push_back(std::move(rule));
  }
}

void BrowserSwitcherSitelistImpl::OnPrefsChanged(
    BrowserSwitcherPrefs* prefs,
    const std::vector<std::string>& changed_prefs) {
  auto it = base::ranges::find(changed_prefs, prefs::kParsingMode);
  if (it != changed_prefs.end()) {
    // ParsingMode changed, re-canonicalize rules.
    StoreRules(ieem_sitelist_, original_ieem_sitelist_);
    StoreRules(external_sitelist_, original_external_sitelist_);
    StoreRules(external_greylist_, original_external_greylist_);
  }
}

bool BrowserSwitcherSitelistImpl::IsActive() const {
  if (!prefs_->IsEnabled())
    return false;

  const RuleSet* rulesets[] = {&prefs_->GetRules(), &ieem_sitelist_,
                               &external_sitelist_, &external_greylist_};
  for (const RuleSet* rules : rulesets) {
    if (!rules->sitelist.empty() || !rules->greylist.empty())
      return true;
  }
  return false;
}

}  // namespace browser_switcher
