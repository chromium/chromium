// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/assistive_input_denylist.h"

#include "base/json/json_reader.h"
#include "chrome/browser/ash/input_method/url_utils.h"

namespace ash {
namespace input_method {
namespace {

// The default denylist of domains that will turn off autocorrect and multi word
// suggestions.
const char* kDefaultDomainDenylist[] = {
    "amazon",
    "b.corp.google",
    "buganizer.corp.google",
    "cider.corp.google",
    "classroom.google",
    "desmos",
    "docs.google",
    "facebook",
    "instagram",
    "mail.google",
    "outlook.live",
    "outlook.office",
    "quizlet",
    "reddit",
    "web.skype",
    "teams.microsoft",
    "twitter",
    "whatsapp",
    "youtube",
};

// Exceptions where the features are enabled.
const char* kAllowedDomainsWithPaths[][2] = {{"mail.google", "/chat"}};

std::vector<std::string> ToDenylist(const base::Value& value) {
  if (!value.is_list()) {
    return {};
  }
  std::vector<std::string> domains;
  for (const auto& item : value.GetList()) {
    if (item.is_string()) {
      domains.push_back(item.GetString());
    }
  }
  return domains;
}

bool MatchesSubDomainFromDefaultList(const GURL& url) {
  for (const char* domain : kDefaultDomainDenylist) {
    if (IsSubDomain(url, domain)) {
      return true;
    }
  }
  return false;
}

bool MatchesSubDomainFrom(const std::vector<std::string>& denylist,
                          const GURL& url) {
  for (const auto& domain : denylist) {
    if (IsSubDomain(url, domain)) {
      return true;
    }
  }
  return false;
}

bool AllowedSubDomainWithPathPrefix(const GURL& url) {
  for (const auto& [domain, path_prefix] : kAllowedDomainsWithPaths) {
    if (IsSubDomainWithPathPrefix(url, domain, path_prefix)) {
      return true;
    }
  }
  return false;
}

}  // namespace

AssistiveInputDenylist::AssistiveInputDenylist(
    const DenylistAdditions& additions) {
  if (auto parsed = base::JSONReader::Read(additions.autocorrect_denylist_json);
      parsed.has_value() && parsed->is_list()) {
    autocorrect_denylist_ = ToDenylist(*parsed);
  }

  if (auto parsed = base::JSONReader::Read(additions.multi_word_denylist_json);
      parsed.has_value() && parsed->is_list()) {
    multi_word_denylist_ = ToDenylist(*parsed);
  }
}

AssistiveInputDenylist::~AssistiveInputDenylist() = default;

bool AssistiveInputDenylist::Contains(const GURL& url) {
  return ((MatchesSubDomainFromDefaultList(url) ||
           MatchesSubDomainFrom(autocorrect_denylist_, url) ||
           MatchesSubDomainFrom(multi_word_denylist_, url)) &&
          // Used to allow specific paths on a top level domain that has been
          // denied (for example, "mail.google.com/chat").
          !AllowedSubDomainWithPathPrefix(url));
}

}  // namespace input_method
}  // namespace ash
