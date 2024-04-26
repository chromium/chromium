// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_PROFILES_SIGNIN_PROFILE_HANDLER_H_
#define CHROME_BROWSER_ASH_PROFILES_SIGNIN_PROFILE_HANDLER_H_

#include <stdint.h>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/login/signin/oauth2_login_manager.h"
#include "content/public/browser/browsing_data_remover.h"

class Profile;

namespace ash {

// Handles sign-in profile related operations, specifically its removal
// on session start.
// TODO(crbug.com/40225390): Cleans up the internal code structure to remove
// unneeded header dependency.
class SigninProfileHandler : public OAuth2LoginManager::Observer,
                             public content::BrowsingDataRemover::Observer {
 public:
  SigninProfileHandler();
  SigninProfileHandler(const SigninProfileHandler&) = delete;
  SigninProfileHandler& operator=(const SigninProfileHandler) = delete;
  ~SigninProfileHandler() override;

  // SigninProfileHandler is like a singleton practically. Returns the
  // instance.
  static SigninProfileHandler* Get();

  // Initialize a bunch of services that are tied to a browser profile.
  // TODO(dzhioev): Investigate whether or not this method is needed.
  void ProfileStartUp(Profile* profile);

  // Clears site data (cookies, history, etc) for signin profile.
  // Callback can be empty. Not thread-safe.
  void ClearSigninProfile(base::OnceClosure callback);

 private:
  // OAuth2LoginManager::Observer override.
  void OnSessionRestoreStateChanged(
      Profile* user_profile,
      OAuth2LoginManager::SessionRestoreState state) override;

  // content::BrowserDataRemover::Observer override.
  void OnBrowsingDataRemoverDone(uint64_t failed_data_types) override;

  // Called when sign-in profile clearing is completed.
  void OnSigninProfileCleared();

  raw_ptr<content::BrowsingDataRemover> browsing_data_remover_ = nullptr;
  base::RepeatingClosure on_clear_profile_stage_finished_;
  std::vector<base::OnceClosure> on_clear_callbacks_;

  base::WeakPtrFactory<SigninProfileHandler> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_PROFILES_SIGNIN_PROFILE_HANDLER_H_
