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

class OptimizationGuideKeyedService;

namespace content {
class WebContents;
}

namespace password_manager {
class PasswordFeatureManager;
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
  static constexpr char kHasPasswordChangeUrlHistogram[] =
      "PasswordManager.HasPasswordChangeUrl";

  ChromePasswordChangeService(
      affiliations::AffiliationService* affiliation_service,
      OptimizationGuideKeyedService* optimization_keyed_service,
      std::unique_ptr<password_manager::PasswordFeatureManager>
          feature_manager);
  ~ChromePasswordChangeService() override;

  // Indicates that password change will be proposed to the user for a given
  // `url`, `username` and `password`. `originator` belongs to a tab which
  // initiated the process.
  virtual void OfferPasswordChangeUi(const GURL& url,
                                     const std::u16string& username,
                                     const std::u16string& password,
                                     content::WebContents* originator);

  // Responds with PasswordChangeDelegate for a given `web_contents`.
  // The same object is returned for a tab which initiated password change and a
  // tab where password change is performed. Returns nullptr if `web_contents`
  // isn't associated with any delegate.
  virtual PasswordChangeDelegate* GetPasswordChangeDelegate(
      content::WebContents* web_contents);

  // PasswordChangeServiceInterface implementation.
  bool IsPasswordChangeAvailable() override;
  bool IsPasswordChangeSupported(const GURL& url) override;

 private:
  // PasswordChangeDelegate::Observer impl.
  void OnPasswordChangeStopped(PasswordChangeDelegate* delegate) override;

  // KeyedService impl.
  void Shutdown() override;

  const raw_ptr<affiliations::AffiliationService> affiliation_service_;
  const raw_ptr<OptimizationGuideKeyedService> optimization_keyed_service_;
  std::unique_ptr<password_manager::PasswordFeatureManager> feature_manager_;

  std::vector<std::unique_ptr<PasswordChangeDelegate>>
      password_change_delegates_;

  base::WeakPtrFactory<ChromePasswordChangeService> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_CHROME_PASSWORD_CHANGE_SERVICE_H_
