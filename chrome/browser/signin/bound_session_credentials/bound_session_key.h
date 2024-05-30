// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_BOUND_SESSION_CREDENTIALS_BOUND_SESSION_KEY_H_
#define CHROME_BROWSER_SIGNIN_BOUND_SESSION_CREDENTIALS_BOUND_SESSION_KEY_H_

#include <string>

#include "url/gurl.h"

struct BoundSessionKey {
  friend auto operator<=>(const BoundSessionKey&,
                          const BoundSessionKey&) = default;
  friend bool operator==(const BoundSessionKey&,
                         const BoundSessionKey&) = default;

  GURL site;
  std::string session_id;
};

#endif  // CHROME_BROWSER_SIGNIN_BOUND_SESSION_CREDENTIALS_BOUND_SESSION_KEY_H_
