// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_BOUND_SESSION_CREDENTIALS_BOUND_SESSION_COOKIE_CONTROLLER_H_
#define CHROME_BROWSER_SIGNIN_BOUND_SESSION_CREDENTIALS_BOUND_SESSION_COOKIE_CONTROLLER_H_

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "url/gurl.h"

class BoundSessionCookieController {
 public:
  class Delegate {
   public:
    // Called when the cookie tracked in this controller has a change in its
    // expiration date. Cookie deletion is considered as a change in the
    // expiration date to the null time.
    virtual void OnCookieExpirationDateChanged() = 0;
  };

  BoundSessionCookieController(const GURL& url,
                               const std::string& cookie_name,
                               Delegate* delegate);

  virtual ~BoundSessionCookieController();

  virtual void Initialize();

  const GURL& url() const { return url_; }
  const std::string& cookie_name() const { return cookie_name_; }
  base::Time cookie_expiration_time() { return cookie_expiration_time_; }

 protected:
  const GURL url_;
  const std::string cookie_name_;
  base::Time cookie_expiration_time_;
  raw_ptr<Delegate> delegate_;
};

#endif  // CHROME_BROWSER_SIGNIN_BOUND_SESSION_CREDENTIALS_BOUND_SESSION_COOKIE_CONTROLLER_H_
