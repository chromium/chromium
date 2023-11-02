// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_QUICK_UNLOCK_PRIVATE_QUICK_UNLOCK_PRIVATE_ASH_UTILS_H_
#define CHROME_BROWSER_EXTENSIONS_API_QUICK_UNLOCK_PRIVATE_QUICK_UNLOCK_PRIVATE_ASH_UTILS_H_

#include <memory>

#include "base/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "chromeos/ash/components/login/auth/auth_factor_editor.h"
#include "chromeos/ash/components/login/auth/auth_performer.h"
#include "chromeos/ash/components/login/auth/auth_status_consumer.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

// This file contains the legacy and new implementations of the
// quickUnlockPrivate.getAuthToken extension API call. The legacy
// implementation relies on the deprecated CheckKey call to communicate with
// the cryptohome cros system daemon, while the new implementation uses auth
// session and auth factor based methods.

class Profile;

namespace ash {
class ExtendedAuthenticator;
class UserContext;
class AuthenticationError;
class AuthPerformer;
}  // namespace ash

namespace extensions {

namespace api {
namespace quick_unlock_private {
struct TokenInfo;
}  // namespace quick_unlock_private
}  // namespace api

//
// A single-use adaptor to make calls to
//   ash::ExtendedAuthenticator::AuthenticateToCheck()
// and pass result back to a single callback. Re. object lifetime, caller just
// have to call:
//
//   scoped_refptr<LegacyQuickUnlockPrivateGetAuthTokenHelper> helper =
//      base::MakeRefCounted<LegacyQuickUnlockPrivateGetAuthTokenHelper>(...);
//   ...
//   // Attach |helper| to a ash::ExtendedAuthenticator.
//   ...
//   // Bind callback and pass as argument.
//   helper->Run(...);
//
// Hereafter, the caller need not worry about |helper|'s lifetime.
class LegacyQuickUnlockPrivateGetAuthTokenHelper
    : public ash::AuthStatusConsumer,
      public base::RefCountedThreadSafe<
          LegacyQuickUnlockPrivateGetAuthTokenHelper,
          content::BrowserThread::DeleteOnUIThread> {
 public:
  using TokenInfo = api::quick_unlock_private::TokenInfo;

  // The only error message that this class ever returns, even if the error is
  // some internal error and not due to an incorrect password. Should
  // eventually be refactored/removed together with this legacy class.
  static const char kPasswordIncorrect[];

  // |error_message| is empty if |success|, and non-empty otherwise.
  // |token_info| is non-null if |success|, and null otherwise.
  using ResultCallback =
      base::OnceCallback<void(bool success,
                              std::unique_ptr<TokenInfo> token_info,
                              const std::string& error_message)>;

  explicit LegacyQuickUnlockPrivateGetAuthTokenHelper(Profile* profile);
  LegacyQuickUnlockPrivateGetAuthTokenHelper(
      const LegacyQuickUnlockPrivateGetAuthTokenHelper&) = delete;
  LegacyQuickUnlockPrivateGetAuthTokenHelper& operator=(
      const LegacyQuickUnlockPrivateGetAuthTokenHelper&) = delete;

  void Run(ash::ExtendedAuthenticator* extended_authenticator,
           const std::string& password,
           ResultCallback callback);

 protected:
  ~LegacyQuickUnlockPrivateGetAuthTokenHelper() override;

 private:
  friend class base::RefCountedThreadSafe<
      LegacyQuickUnlockPrivateGetAuthTokenHelper>;
  friend class base::DeleteHelper<LegacyQuickUnlockPrivateGetAuthTokenHelper>;
  friend struct content::BrowserThread::DeleteOnThread<
      content::BrowserThread::UI>;

  // AuthStatusConsumer overrides.
  void OnAuthFailure(const ash::AuthFailure& error) override;
  void OnAuthSuccess(const ash::UserContext& user_context) override;

  raw_ptr<Profile> profile_;
  ResultCallback callback_;
};

class QuickUnlockPrivateGetAuthTokenHelper {
 public:
  QuickUnlockPrivateGetAuthTokenHelper(Profile*, std::string password);
  ~QuickUnlockPrivateGetAuthTokenHelper();

  QuickUnlockPrivateGetAuthTokenHelper(
      const QuickUnlockPrivateGetAuthTokenHelper&) = delete;
  QuickUnlockPrivateGetAuthTokenHelper& operator=(
      const QuickUnlockPrivateGetAuthTokenHelper&) = delete;

  using Callback = base::OnceCallback<void(
      absl::optional<api::quick_unlock_private::TokenInfo> token,
      absl::optional<ash::AuthenticationError>)>;

  // `Run` does the following:
  // 1. Switch to the UI thread (all communication with the cryptohome daemon
  //    should happen on the UI thread).
  // 2. Start an auth session.
  // 3. Authenticate the auth session with the password that was supplied in
  //    the constructor.
  // 4. Load the list of auth factors that are configured for the user.
  //
  // If all calls succeeds, we create an auth token, save the user context
  // there, and run `callback` with the auth token.
  void Run(Callback callback);

 private:
  void RunOnUIThread(Callback);

  void OnAuthSessionStarted(Callback,
                            bool user_exists,
                            std::unique_ptr<ash::UserContext>,
                            absl::optional<ash::AuthenticationError>);

  void OnAuthenticated(Callback,
                       std::unique_ptr<ash::UserContext>,
                       absl::optional<ash::AuthenticationError>);

  void OnAuthFactorsConfiguration(Callback,
                                  std::unique_ptr<ash::UserContext>,
                                  absl::optional<ash::AuthenticationError>);

  raw_ptr<Profile> profile_;
  std::string password_;
  ash::AuthPerformer auth_performer_;
  ash::AuthFactorEditor auth_factor_editor_;

  base::WeakPtrFactory<QuickUnlockPrivateGetAuthTokenHelper> weak_factory_{
      this};
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_QUICK_UNLOCK_PRIVATE_QUICK_UNLOCK_PRIVATE_ASH_UTILS_H_
