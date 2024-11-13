// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_CHROME_PASSWORD_CHANGE_SERVICE_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_CHROME_PASSWORD_CHANGE_SERVICE_H_

#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "components/password_manager/core/browser/password_change_service_interface.h"

class GURL;

namespace affiliations {
class AffiliationService;
}

namespace content {
class WebContents;
}

class ChromePasswordChangeService
    : public password_manager::PasswordChangeServiceInterface {
 public:
  explicit ChromePasswordChangeService(
      affiliations::AffiliationService* affiliation_service);

  // Starts password change for a given |url|, |username| and |password|.
  // |originator| belongs to a tab which initiated the process.
  void StartPasswordChange(const GURL& url,
                           const std::u16string& username,
                           const std::u16string& password,
                           content::WebContents* originator);
  // Responds whether password change is ongoing for a given |web_contents|.
  // This is true both for originator and a tab where password change is
  // performed.
  bool IsPasswordChangeOngoing(content::WebContents* web_contents);

  // PasswordChangeServiceInterface implementation.
  bool IsPasswordChangeSupported(const GURL& url) override;

 private:
  raw_ptr<affiliations::AffiliationService> affiliation_service_;
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_CHROME_PASSWORD_CHANGE_SERVICE_H_
