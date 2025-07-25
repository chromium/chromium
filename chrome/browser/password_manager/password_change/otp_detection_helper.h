// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_CHANGE_OTP_DETECTION_HELPER_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_CHANGE_OTP_DETECTION_HELPER_H_

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/timer/timer.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/password_manager/core/browser/one_time_passwords/otp_manager.h"
#include "content/public/browser/web_contents_observer.h"

namespace content {
class WebContents;
}

// Helper object which waits for One Time Password (OTP) fields to disappear.
// Callers must ensure that `IsOtpPresent()` is true before creating this
// object.
class OtpDetectionHelper : public password_manager::OtpManager::Observer,
                           public content::WebContentsObserver {
 public:
  using OtpChallengeResolvedCallback = base::OnceCallback<void()>;

  OtpDetectionHelper(content::WebContents* web_contents,
                     password_manager::PasswordManagerClient* client,
                     OtpChallengeResolvedCallback callback);

  ~OtpDetectionHelper() override;

  static bool IsOtpPresent(content::WebContents* web_contents,
                           password_manager::PasswordManagerClient* client);

  // content::WebContentsObserver:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

  // password_manager::OtpManager::Observer
  void OnOtpFieldDetected(
      password_manager::OtpFormManager* form_manager) override;

 private:
  const raw_ptr<content::WebContents> web_contents_;
  const raw_ptr<password_manager::PasswordManagerClient> client_;
  OtpChallengeResolvedCallback callback_;

  // Holds detected OTP fields. Only a single field per form is stored. Used
  // later to detect when OTP disappears from a page.
  std::vector<autofill::FieldGlobalId> otp_fields_;

  base::ScopedObservation<password_manager::OtpManager,
                          password_manager::OtpManager::Observer>
      otp_observation_{this};

  base::WeakPtrFactory<OtpDetectionHelper> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_CHANGE_OTP_DETECTION_HELPER_H_
