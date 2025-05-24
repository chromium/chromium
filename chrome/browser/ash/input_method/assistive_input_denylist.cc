// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/assistive_input_denylist.h"

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

bool MatchesSubDomainFromDefaultList(const GURL& url) {
  for (const char* domain : kDefaultDomainDenylist) {
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

AssistiveInputDenylist::AssistiveInputDenylist() = default;

AssistiveInputDenylist::~AssistiveInputDenylist() = default;

bool AssistiveInputDenylist::Contains(const GURL& url) {
  return (MatchesSubDomainFromDefaultList(url) &&
          // Used to allow specific paths on a top level domain that has been
          // denied (for example, "mail.google.com/chat").
          !AllowedSubDomainWithPathPrefix(url));
}

}  // namespace input_method
}  // namespace ash
