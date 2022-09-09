// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_PROFILE_IO_DATA_H_
#define CHROME_BROWSER_PROFILES_PROFILE_IO_DATA_H_

#include <string>

class GURL;

// The ProfileIOData is slated for deletion. It currently only contains two
// static methods.
class ProfileIOData {
 public:
  ProfileIOData() = delete;

  ProfileIOData(const ProfileIOData&) = delete;
  ProfileIOData& operator=(const ProfileIOData&) = delete;

  ~ProfileIOData() = delete;

  // Returns true if |scheme| is handled in Chrome, or by default handlers in
  // net::URLRequest.
  static bool IsHandledProtocol(const std::string& scheme);

  // Returns true if |url| is handled in Chrome, or by default handlers in
  // net::URLRequest.
  static bool IsHandledURL(const GURL& url);
};

#endif  // CHROME_BROWSER_PROFILES_PROFILE_IO_DATA_H_
