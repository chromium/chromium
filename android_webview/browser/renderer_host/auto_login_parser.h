// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_RENDERER_HOST_AUTO_LOGIN_PARSER_H_
#define ANDROID_WEBVIEW_BROWSER_RENDERER_HOST_AUTO_LOGIN_PARSER_H_

#include <string>

namespace android_webview {

enum RealmRestriction {
  ONLY_GOOGLE_COM,
  ALLOW_ANY_REALM
};

struct HeaderData {
  HeaderData();
  ~HeaderData();

  // "realm" string from x-auto-login (e.g. "com.google").
  std::string realm;

  // "account" string from x-auto-login.
  std::string account;

  // "args" string from x-auto-login to be passed to MergeSession. This string
  // should be considered opaque and not be cracked open to look inside.
  std::string args;
};

// Returns whether parsing succeeded. Parameter |header_data| will not be
// modified if parsing fails.
bool ParseHeader(const std::string& header,
                 RealmRestriction realm_restriction,
                 HeaderData* header_data);

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_RENDERER_HOST_AUTO_LOGIN_PARSER_H_
