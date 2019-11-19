// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_switcher/browser_switcher_sitelist.h"

#include <string.h>

#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "chrome/browser/browser_switcher/browser_switcher_prefs.h"
#include "chrome/browser/browser_switcher/ieem_sitelist_parser.h"
#include "components/prefs/pref_service.h"
#include "url/gurl.h"

#include "third_party/re2/src/re2/re2.h"

namespace browser_switcher {

namespace {

// This type is cheap and lives on the stack, which can be faster compared to
// calling |GURL::host()| multiple times.
struct NoCopyUrl {
  base::StringPiece host;
  base::StringPiece spec;
};

// Returns true if |input| contains |token|, ignoring case for ASCII
// characters.
bool StringContainsInsensitiveASCII(base::StringPiece input,
                                    base::StringPiece token) {
  const char* found =
      std::search(input.begin(), input.end(), token.begin(), token.end(),
                  [](char a, char b) {
                    return base::ToLowerASCII(a) == base::ToLowerASCII(b);
                  });
  return found != input.end();
}

// Checks if the omitted prefix for a non-fully specific prefix is one of the
// expected parts that are allowed to be omitted (e.g. "https://").
bool IsValidPrefix(base::StringPiece prefix) {
  static re2::LazyRE2 re = {"(https?|file):(//)?"};
  re2::StringPiece converted_prefix(prefix.data(), prefix.size());
  return (prefix.empty() || re2::RE2::FullMatch(converted_prefix, *re));
}

bool IsInverted(base::StringPiece pattern) {
  return (!pattern.empty() && pattern[0] == '!');
}

bool UrlMatchesPattern(const NoCopyUrl& url, base::StringPiece pattern) {
  if (pattern == "*") {
    // Wildcard, always match.
    return true;
  }
  if (pattern.find('/') != base::StringPiece::npos) {
    // Check prefix using the normalized URL. Case sensitive, but with
    // case-insensitive scheme/hostname.
    size_t pos = url.spec.find(pattern);
    if (pos == base::StringPiece::npos)
      return false;
    return IsValidPrefix(base::StringPiece(url.spec.data(), pos));
  }
  // Compare hosts, case-insensitive.
  return StringContainsInsensitiveASCII(url.host, pattern);
}

// Checks whether |patterns| contains a pattern that matches |url|, and returns
// the longest matching pattern. If there are no matches, an empty pattern is
// returned.
//
// If |contains_inverted_matches| is true, treat patterns that start with "!" as
// inverted matches.
base::StringPiece MatchUrlToList(const NoCopyUrl& url,
                                 const std::vector<std::string>& patterns,
                                 bool contains_inverted_matches) {
  base::StringPiece reason;
  for (const std::string& pattern : patterns) {
    if (pattern.size() <= reason.size())
      continue;
    bool inverted = IsInverted(pattern);
    if (inverted && !contains_inverted_matches)
      continue;
    if (UrlMatchesPattern(url, (inverted ? pattern.substr(1) : pattern))) {
      reason = pattern;
    }
  }
  return reason;
}

bool StringSizeCompare(const base::StringPiece& a, const base::StringPiece& b) {
  return a.size() < b.size();
}

}  // namespace

void CanonicalizeRule(std::string* pattern) {
  if (*pattern == "*" || pattern->find("/") == std::string::npos) {
    // No "/" in the string. It's a hostnmae or wildcard, convert to lowercase.
    *pattern = base::ToLowerASCII(*pattern);
    return;
  }

  // The string has a "/" in it. It could be:
  // - "//example.com/abc", convert hostname to lowercase
  // - "example.com/abc", treat same as "//example.com/abc"
  // - "http://example.com/abc", convert hostname and scheme to lowercase
  // - "/abc", keep capitalization

  const char* prefix = "";
  base::StringPiece pattern_strpiece(*pattern);
  if (IsInverted(*pattern)) {
    prefix = "!";
    *pattern = pattern->substr(1);
  }

  if (base::StartsWith(*pattern, "/", base::CompareCase::SENSITIVE) &&
      !base::StartsWith(*pattern, "//", base::CompareCase::SENSITIVE)) {
    // Rule starts with a single slash, e.g. "/abc". Don't change case.
    pattern->insert(0, prefix);
    return;
  }

  if (pattern->find("/") != 0 && pattern->find("://") == std::string::npos) {
    // Transform "example.com/abc" => "//example.com/abc".
    pattern->insert(0, "//");
  }

  // For patterns that include a "/": parse the URL to get the proper
  // capitalization (for scheme/hostname).
  //
  // To properly parse URLs with no scheme, we need a valid base URL. We use
  // "ftp://XXX/", which is a valid URL with an unsupported scheme. That way,
  // parsing still succeeds, and we can easily know when the scheme isn't part
  // of the original pattern (and omit it from the output).
  const char* placeholder_scheme = "ftp:";
  std::string placeholder = base::StrCat({placeholder_scheme, "//XXX/"});
  GURL base_url(placeholder);

  GURL relative_url = base_url.Resolve(*pattern);
  base::StringPiece spec = relative_url.possibly_invalid_spec();

  // The parsed URL might start with "ftp://XXX/" or "ftp://". Remove that
  // prefix.
  if (base::StartsWith(spec, placeholder, base::CompareCase::INSENSITIVE_ASCII))
    spec = spec.substr(placeholder.size());
  if (base::StartsWith(spec, placeholder_scheme,
                       base::CompareCase::INSENSITIVE_ASCII))
    spec = spec.substr(strlen(placeholder_scheme));

  *pattern = base::StrCat({prefix, spec.as_string()});
}

Decision::Decision(Action action_,
                   Reason reason_,
                   base::StringPiece matching_rule_)
    : action(action_), reason(reason_), matching_rule(matching_rule_) {}

Decision::Decision() = default;
Decision::Decision(Decision&) = default;
Decision::Decision(Decision&&) = default;

bool Decision::operator==(const Decision& that) const {
  return (action == that.action && reason == that.reason &&
          matching_rule == that.matching_rule);
}

BrowserSwitcherSitelist::~BrowserSwitcherSitelist() = default;

bool BrowserSwitcherSitelist::ShouldSwitch(const GURL& url) const {
  return GetDecision(url).action == kGo;
}

BrowserSwitcherSitelistImpl::BrowserSwitcherSitelistImpl(
    const BrowserSwitcherPrefs* prefs)
    : prefs_(prefs) {}

BrowserSwitcherSitelistImpl::~BrowserSwitcherSitelistImpl() = default;

Decision BrowserSwitcherSitelistImpl::GetDecision(const GURL& url) const {
  // Don't record metrics for LBS non-users.
  if (!IsActive())
    return {kStay, kDisabled, ""};

  Decision decision = GetDecisionImpl(url);
  UMA_HISTOGRAM_BOOLEAN("BrowserSwitcher.Decision", decision.action == kGo);
  return decision;
}

Decision BrowserSwitcherSitelistImpl::GetDecisionImpl(const GURL& url) const {
  SCOPED_UMA_HISTOGRAM_TIMER("BrowserSwitcher.DecisionTime");

  if (!url.SchemeIsHTTPOrHTTPS() && !url.SchemeIsFile()) {
    return {kStay, kProtocol, ""};
  }

  std::string url_host = url.host();
  NoCopyUrl no_copy_url = {url_host, url.spec()};

  base::StringPiece reason_to_go = std::max(
      {
          MatchUrlToList(no_copy_url, prefs_->GetRules().sitelist, true),
          MatchUrlToList(no_copy_url, ieem_sitelist_.sitelist, true),
          MatchUrlToList(no_copy_url, external_sitelist_.sitelist, true),
      },
      StringSizeCompare);

  // If sitelists don't match, no need to check the greylists.
  if (reason_to_go.empty())
    return {kStay, kDefault, ""};
  if (IsInverted(reason_to_go))
    return {kStay, kSitelist, reason_to_go};

  base::StringPiece reason_to_stay = std::max(
      {
          MatchUrlToList(no_copy_url, prefs_->GetRules().greylist, false),
          MatchUrlToList(no_copy_url, ieem_sitelist_.greylist, false),
          MatchUrlToList(no_copy_url, external_sitelist_.greylist, false),
      },
      StringSizeCompare);

  if (reason_to_go == "*" && !reason_to_stay.empty())
    return {kStay, kGreylist, reason_to_stay};

  if (reason_to_go.size() >= reason_to_stay.size())
    return {kGo, kSitelist, reason_to_go};
  else
    return {kStay, kGreylist, reason_to_stay};
}

void BrowserSwitcherSitelistImpl::SetIeemSitelist(ParsedXml&& parsed_xml) {
  DCHECK(!parsed_xml.error);

  UMA_HISTOGRAM_COUNTS_100000("BrowserSwitcher.IeemSitelistSize",
                              parsed_xml.rules.size());

  ieem_sitelist_.sitelist = std::move(parsed_xml.rules);
}

void BrowserSwitcherSitelistImpl::SetExternalSitelist(ParsedXml&& parsed_xml) {
  DCHECK(!parsed_xml.error);

  UMA_HISTOGRAM_COUNTS_100000("BrowserSwitcher.ExternalSitelistSize",
                              parsed_xml.rules.size());

  external_sitelist_.sitelist = std::move(parsed_xml.rules);
}

void BrowserSwitcherSitelistImpl::SetExternalGreylist(ParsedXml&& parsed_xml) {
  DCHECK(!parsed_xml.error);

  UMA_HISTOGRAM_COUNTS_100000("BrowserSwitcher.ExternalGreylistSize",
                              parsed_xml.rules.size());

  external_sitelist_.greylist = std::move(parsed_xml.rules);
}

const RuleSet* BrowserSwitcherSitelistImpl::GetIeemSitelist() const {
  return &ieem_sitelist_;
}

const RuleSet* BrowserSwitcherSitelistImpl::GetExternalSitelist() const {
  return &external_sitelist_;
}

bool BrowserSwitcherSitelistImpl::IsActive() const {
  if (!prefs_->IsEnabled())
    return false;

  const RuleSet* rulesets[] = {&prefs_->GetRules(), &ieem_sitelist_,
                               &external_sitelist_};
  for (const RuleSet* rules : rulesets) {
    if (!rules->sitelist.empty() || !rules->greylist.empty())
      return true;
  }
  return false;
}

}  // namespace browser_switcher
