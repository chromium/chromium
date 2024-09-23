// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_BOUND_SESSION_CREDENTIALS_BOUND_SESSION_DEBUG_INFO_H_
#define CHROME_BROWSER_SIGNIN_BOUND_SESSION_CREDENTIALS_BOUND_SESSION_DEBUG_INFO_H_

#include <string>

#include "base/time/time.h"
#include "url/gurl.h"

class BoundSessionCookieController;

// Used to display bound session info in `chrome://signin-internals` page.
struct BoundSessionDebugInfo {
  static BoundSessionDebugInfo Create(
      const BoundSessionCookieController& controller);

  BoundSessionDebugInfo(std::string session_id,
                        std::string domain,
                        std::string path,
                        bool throttling_paused,
                        base::Time expiration_time,
                        std::string bound_cookie_names,
                        GURL refresh_url);
  ~BoundSessionDebugInfo();

  BoundSessionDebugInfo(const BoundSessionDebugInfo&) = delete;
  BoundSessionDebugInfo& operator=(const BoundSessionDebugInfo&) = delete;

  BoundSessionDebugInfo(BoundSessionDebugInfo&& other) noexcept;
  BoundSessionDebugInfo& operator=(BoundSessionDebugInfo&& other) noexcept;

  std::string session_id;
  std::string domain;
  std::string path;
  bool throttling_paused;
  base::Time expiration_time;
  std::string bound_cookie_names;
  GURL refresh_url;
};

#endif  // CHROME_BROWSER_SIGNIN_BOUND_SESSION_CREDENTIALS_BOUND_SESSION_DEBUG_INFO_H_
