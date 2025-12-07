// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_CHANGE_PASSWORD_CHANGE_HATS_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_CHANGE_PASSWORD_CHANGE_HATS_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/password_manager/core/browser/password_store/password_store_consumer.h"

namespace content {
class WebContents;
}  // namespace content

namespace password_manager {
class PasswordStoreInterface;
}  // namespace password_manager

class HatsService;

// Fetches the product-specific data for password change surveys from password
// store and provides functionality to launch surveys.
class PasswordChangeHats : public password_manager::PasswordStoreConsumer {
 public:
  PasswordChangeHats(HatsService* hats_service,
                     password_manager::PasswordStoreInterface* profile_store,
                     password_manager::PasswordStoreInterface* account_store);

  PasswordChangeHats(const PasswordChangeHats&) = delete;
  PasswordChangeHats& operator=(const PasswordChangeHats&) = delete;
  ~PasswordChangeHats() override;

  // Tries to launch a password change survey in `web_contents`. `trigger`
  // specifies which scenario occurred (e.g. error or successful password
  // change). If not std::nullopt, `password_change_duration` specifies the
  // feature runtime until reaching the trigger condition. If not std::nullopt,
  // `blocking_challenge_detected` specifies whether there was a challenge that
  // user had to resolve to proceed with the flow (e.g. OTP). The survey might
  // not launch based on the global rate limiting logic handled by the
  // `hats_service_`.
  void MaybeLaunchSurvey(
      const std::string& trigger,
      std::optional<base::TimeDelta> password_change_duration,
      std::optional<bool> blocking_challenge_detected,
      content::WebContents* web_contents);

 private:
  // PasswordStoreConsumer:
  void OnGetPasswordStoreResultsOrErrorFrom(
      password_manager::PasswordStoreInterface* store,
      password_manager::LoginsResultOrError results_or_error) override;

  const raw_ptr<HatsService> hats_service_;

  // Count of all saved passwords.
  int64_t passwords_count_ = 0;
  // Counter of all saved leaked passwords.
  int64_t leaked_passwords_count_ = 0;
  // Whether there is any generated password saved.
  bool adopted_generated_passwords_ = false;

  // Counters tracking whether data was fetched from all user's stores.
  int fetch_initiated_count_ = 0;
  int fetch_successful_count_ = 0;

  base::WeakPtrFactory<PasswordChangeHats> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_CHANGE_PASSWORD_CHANGE_HATS_H_
