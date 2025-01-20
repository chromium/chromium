// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_CHROME_PASSWORD_CHANGE_SERVICE_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_CHROME_PASSWORD_CHANGE_SERVICE_H_

#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/password_manager/password_change_delegate.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/password_manager/core/browser/password_change_service_interface.h"

class GURL;

namespace affiliations {
class AffiliationService;
}

namespace content {
class WebContents;
}

class ChromePasswordChangeService
    : public KeyedService,
      public password_manager::PasswordChangeServiceInterface,
      public PasswordChangeDelegate::Observer {
 public:
  // Callback which allows to open a new tab for password change. Useful for
  // testing UI interactions in browser tests.
  using OpenNewTabCallback =
      base::RepeatingCallback<content::WebContents*(const GURL&,
                                                    content::WebContents*)>;

  explicit ChromePasswordChangeService(
      affiliations::AffiliationService* affiliation_service);
  ~ChromePasswordChangeService() override;

  // Starts password change for a given `url`, `username` and `password`.
  // `originator` belongs to a tab which initiated the process.
  void StartPasswordChange(const GURL& url,
                           const std::u16string& username,
                           const std::u16string& password,
                           content::WebContents* originator);

  // Responds with PasswordChangeDelegate for a given `web_contents`.
  // The same object is returned for a tab which initiated password change and a
  // tab where password change is performed. Returns nullptr if `web_contents`
  // isn't associated with any delegate.
  PasswordChangeDelegate* GetPasswordChangeDelegate(
      content::WebContents* web_contents);

  // PasswordChangeServiceInterface implementation.
  bool IsPasswordChangeSupported(const GURL& url) override;

  // For testing only.
  void SetCustomTabOpening(OpenNewTabCallback callback) {
    new_tab_callback_ = std::move(callback);
  }

 private:
  // PasswordChangeDelegate::Observer impl.
  void OnPasswordChangeStopped(PasswordChangeDelegate* delegate) override;

  const raw_ptr<affiliations::AffiliationService> affiliation_service_;

  // TODO(crbug.com/382652112): Remove once testing is simplified.
  OpenNewTabCallback new_tab_callback_;

  std::vector<std::unique_ptr<PasswordChangeDelegate>>
      password_change_delegates_;

  base::WeakPtrFactory<ChromePasswordChangeService> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_CHROME_PASSWORD_CHANGE_SERVICE_H_
