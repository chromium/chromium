// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_QUICK_UNLOCK_PRIVATE_QUICK_UNLOCK_PRIVATE_ASH_UTILS_H_
#define CHROME_BROWSER_EXTENSIONS_API_QUICK_UNLOCK_PRIVATE_QUICK_UNLOCK_PRIVATE_ASH_UTILS_H_

#include <memory>

#include "base/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "chromeos/ash/components/login/auth/auth_status_consumer.h"
#include "content/public/browser/browser_thread.h"

class Profile;

namespace ash {
class ExtendedAuthenticator;
class UserContext;
}  // namespace ash

namespace extensions {

namespace api {
namespace quick_unlock_private {
struct TokenInfo;
}  // namespace quick_unlock_private
}  // namespace api

// A single-use adaptor to make calls to
//   ash::ExtendedAuthenticator::AuthenticateToCheck()
// and pass result back to a single callback. Re. object lifetime, caller just
// have to call:
//
//   scoped_refptr<QuickUnlockPrivateGetAuthTokenHelper> helper =
//      base::MakeRefCounted<QuickUnlockPrivateGetAuthTokenHelper>(...);
//   ...
//   // Attach |helper| to a ash::ExtendedAuthenticator.
//   ...
//   // Bind callback and pass as argument.
//   helper->Run(...);
//
// Hereafter, the caller need not worry about |helper|'s lifetime.
class QuickUnlockPrivateGetAuthTokenHelper
    : public ash::AuthStatusConsumer,
      public base::RefCountedThreadSafe<
          QuickUnlockPrivateGetAuthTokenHelper,
          content::BrowserThread::DeleteOnUIThread> {
 public:
  using TokenInfo = api::quick_unlock_private::TokenInfo;

  // |error_message| is empty if |success|, and non-empty otherwise.
  // |token_info| is non-null if |success|, and null otherwise.
  using ResultCallback =
      base::OnceCallback<void(bool success,
                              std::unique_ptr<TokenInfo> token_info,
                              const std::string& error_message)>;

  explicit QuickUnlockPrivateGetAuthTokenHelper(Profile* profile);
  QuickUnlockPrivateGetAuthTokenHelper(
      const QuickUnlockPrivateGetAuthTokenHelper&) = delete;
  QuickUnlockPrivateGetAuthTokenHelper& operator=(
      const QuickUnlockPrivateGetAuthTokenHelper&) = delete;

  void Run(ash::ExtendedAuthenticator* extended_authenticator,
           const std::string& password,
           ResultCallback callback);

 protected:
  ~QuickUnlockPrivateGetAuthTokenHelper() override;

 private:
  friend class base::RefCountedThreadSafe<QuickUnlockPrivateGetAuthTokenHelper>;
  friend class base::DeleteHelper<QuickUnlockPrivateGetAuthTokenHelper>;
  friend struct content::BrowserThread::DeleteOnThread<
      content::BrowserThread::UI>;

  // AuthStatusConsumer overrides.
  void OnAuthFailure(const ash::AuthFailure& error) override;
  void OnAuthSuccess(const ash::UserContext& user_context) override;

  raw_ptr<Profile> profile_;
  ResultCallback callback_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_QUICK_UNLOCK_PRIVATE_QUICK_UNLOCK_PRIVATE_ASH_UTILS_H_
