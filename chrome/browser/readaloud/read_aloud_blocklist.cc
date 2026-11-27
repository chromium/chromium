// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/readaloud/read_aloud_blocklist.h"

#include <string_view>

#include "base/containers/fixed_flat_set.h"
#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace readaloud {

BASE_FEATURE(kReadAloudBlockedDomains, base::FEATURE_ENABLED_BY_DEFAULT);

namespace {

// Default built-in list of non-article domains where Read Aloud is not
// applicable. Entries must be lowercase without leading "www." or "m."
// prefix and are matched exactly against the normalized host of the non-opaque
// origin extracted from the input URL (matching Legacy Read Aloud behavior).
constexpr auto kDefaultBlockedDomains =
    base::MakeFixedFlatSet<std::string_view>({
        "account.live.com",
        "accounts.google.com",
        "amazon.com",
        "character.ai",
        "chatgpt.com",
        "contacts.google.com",
        "corp.google.com",
        "dailymotion.com",
        "docs.google.com",
        "drive.google.com",
        "duckduckgo.com",
        "facebook.com",
        "forms.office.com",
        "gds.google.com",
        "gemini.google.com",
        "grok.com",
        "instagram.com",
        "login.live.com",
        "login.microsoftonline.com",
        "mail.google.com",
        "maps.google.com",
        "messages.google.com",
        "myaccount.google.com",
        "myactivity.google.com",
        "notebooklm.google.com",
        "outlook.live.com",
        "pinterest.com",
        "search.brave.com",
        "studio.firebase.google.com",
        "takeout.google.com",
        "teams.microsoft.com",
        "temu.com",
        "tiktok.com",
        "translate.google.com",
        "twitch.tv",
        "twitter.com",
        "weather.com",
        "web.whatsapp.com",
        "x.com",
        "youtube.com",
    });

}  // namespace

ReadAloudBlocklist::ReadAloudBlocklist() = default;
ReadAloudBlocklist::~ReadAloudBlocklist() = default;

// static
ReadAloudBlocklist& ReadAloudBlocklist::GetInstance() {
  static base::NoDestructor<ReadAloudBlocklist> instance;
  return *instance;
}

bool ReadAloudBlocklist::IsBlocked(const GURL& url) const {
  if (!base::FeatureList::IsEnabled(kReadAloudBlockedDomains)) {
    return false;
  }

  if (!url.is_valid()) {
    return false;
  }

  const url::Origin origin = url::Origin::Create(url);
  if (origin.opaque()) {
    return false;
  }

  std::string_view host = origin.host();
  // Strips only "www." and "m." to match server-side CheckURL normalization
  // without sweeping unrelated subdomains (e.g. blog/developer portals).
  if (host.ends_with('.')) {
    host.remove_suffix(1);
  }
  if (host.starts_with("www.")) {
    host.remove_prefix(4);
  } else if (host.starts_with("m.")) {
    host.remove_prefix(2);
  }

  return kDefaultBlockedDomains.contains(host);
}

}  // namespace readaloud
