// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/renderer_host/auto_login_parser.h"

#include <utility>
#include <vector>

#include "base/notreached.h"
#include "base/strings/escape.h"
#include "base/strings/string_split.h"

namespace android_webview {

namespace {

bool MatchRealm(const std::string& realm, RealmRestriction restriction) {
  switch (restriction) {
    case ONLY_GOOGLE_COM:
      return realm == "com.google";
    case ALLOW_ANY_REALM:
      return true;
    default:
      NOTREACHED();
  }
}

}  // namespace

HeaderData::HeaderData() {}
HeaderData::~HeaderData() {}

bool ParseHeader(const std::string& header,
                 RealmRestriction realm_restriction,
                 HeaderData* header_data) {
  // TODO(pliard): Investigate/fix potential internationalization issue. It
  // seems that "account" from the x-auto-login header might contain non-ASCII
  // characters.
  if (header.empty())
    return false;

  base::StringPairs pairs;
  if (!base::SplitStringIntoKeyValuePairs(header, '=', '&', &pairs))
    return false;

  // Parse the information from the |header| string.
  HeaderData local_params;
  for (base::StringPairs::const_iterator it = pairs.begin(); it != pairs.end();
       ++it) {
    const std::string& key = it->first;
    const std::string& value = it->second;
    std::string unescaped_value = base::UnescapeBinaryURLComponent(value);
    if (key == "realm") {
      if (!MatchRealm(unescaped_value, realm_restriction))
        return false;
      local_params.realm = unescaped_value;
    } else if (key == "account") {
      local_params.account = unescaped_value;
    } else if (key == "args") {
      local_params.args = unescaped_value;
    }
  }
  if (local_params.realm.empty() || local_params.args.empty())
    return false;

  *header_data = local_params;
  return true;
}

}  // namespace android_webview
