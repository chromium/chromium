// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_MODE_KIOSK_PROFILE_LOADER_H_
#define CHROME_BROWSER_ASH_APP_MODE_KIOSK_PROFILE_LOADER_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "chrome/browser/ash/app_mode/cancellable_job.h"
#include "chrome/browser/ash/app_mode/kiosk_app_launch_error.h"
#include "chrome/browser/ash/app_mode/kiosk_app_types.h"
#include "chrome/browser/ash/login/session/user_session_manager.h"
#include "chromeos/ash/components/login/auth/login_performer.h"
#include "components/account_id/account_id.h"

class Profile;

namespace ash {

class UserContext;

// KioskProfileLoader loads a special profile for a given app. It first
// attempts to login for the app's generated user id. If the login is
// successful, it prepares app profile then calls the delegate.
class KioskProfileLoader {
 public:
  using OldEncryptionUserContext = std::unique_ptr<UserContext>;

  class Delegate {
   public:
    virtual void OnProfileLoaded(Profile* profile) = 0;
    virtual void OnProfileLoadFailed(KioskAppLaunchError::Error error) = 0;
    virtual void OnOldEncryptionDetected(
        OldEncryptionUserContext user_context) = 0;

   protected:
    virtual ~Delegate() = default;
  };

  KioskProfileLoader(const AccountId& app_account_id,
                     KioskAppType app_type,
                     Delegate* delegate);
  KioskProfileLoader(const KioskProfileLoader&) = delete;
  KioskProfileLoader& operator=(const KioskProfileLoader&) = delete;
  ~KioskProfileLoader();

  // Starts profile load. Calls delegate on success or failure.
  void Start();

 private:
  void LoginAsKioskAccount();
  void PrepareProfile(const UserContext& user_context);
  void ReportProfileLoaded(Profile& profile);
  void ReportLaunchResult(KioskAppLaunchError::Error error);
  void ReportOldEncryptionUserContext(OldEncryptionUserContext user_context);

  const AccountId account_id_;
  const KioskAppType app_type_;
  raw_ptr<Delegate, ExperimentalAsh> delegate_
      GUARDED_BY_CONTEXT(sequence_checker_);
  std::unique_ptr<CancellableJob> current_step_
      GUARDED_BY_CONTEXT(sequence_checker_);

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_APP_MODE_KIOSK_PROFILE_LOADER_H_
