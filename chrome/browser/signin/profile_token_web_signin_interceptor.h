// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_PROFILE_TOKEN_WEB_SIGNIN_INTERCEPTOR_H_
#define CHROME_BROWSER_SIGNIN_PROFILE_TOKEN_WEB_SIGNIN_INTERCEPTOR_H_

#include <memory>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/signin/token_managed_profile_creator.h"
#include "components/keyed_service/core/keyed_service.h"

namespace content {
class WebContents;
}

class Profile;
class ProfileAttributesEntry;

// Called after web signed in, an enrollment token has been found in a web page.
//
// Implementation notes: here is how an entire interception flow work for the
// enterprise or multi-user case:
// * MaybeInterceptSigninProfile() is called after an enrollment token is found.
// * Interception UI is shown by the delegate.
// * If the user approved, a new profile is created and the token written in the
//   new profile's storage, using TokenManagedProfileCreator.
// * At this point, the flow ends in this profile, and continues in the new
//   profile.
// * When the account is available on the web in the new profile:
//   - A new browser window is created for the new profile,
//   - The tab is moved to the new profile.
class ProfileTokenWebSigninInterceptor : public KeyedService {
 public:
  enum class SigninInterceptionType {
    kProfileSwitch,
    kEnterprise,
  };

  class Delegate {
   public:
    virtual ~Delegate() = default;
    virtual void ShowCreateNewProfileBubble(
        const ProfileAttributesEntry* switch_to_profile,
        base::OnceCallback<void(bool)> callback) = 0;
  };

  explicit ProfileTokenWebSigninInterceptor(Profile* profile);
  ProfileTokenWebSigninInterceptor(Profile* profile,
                                   std::unique_ptr<Delegate> delegate);
  ~ProfileTokenWebSigninInterceptor() override;

  ProfileTokenWebSigninInterceptor(const ProfileTokenWebSigninInterceptor&) =
      delete;
  ProfileTokenWebSigninInterceptor& operator=(
      const ProfileTokenWebSigninInterceptor&) = delete;

  void MaybeInterceptSigninProfile(content::WebContents* intercepted_contents,
                                   const std::string& id,
                                   const std::string& enrollment_token);

  void SetDisableBrowserCreationAfterInterceptionForTesting(bool disable) {
    disable_browser_creation_after_interception_for_testing_ = disable;
  }

  // KeyedService:
  void Shutdown() override;

 private:
  void CreateBrowserAfterSigninInterception(content::WebContents* web_contents);
  // Cancels any current signin interception and resets the interceptor to its
  // initial state.
  void Reset();

  bool IsValidEnrollmentToken(const std::string& enrollment_token) const;

  void OnProfileCreationChoice(bool accepted);

  // Called when the new browser is created after interception. Passed as
  // callback to `session_startup_helper_`.
  void OnNewBrowserCreated(bool is_new_profile);

  void OnNewSignedInProfileCreated(Profile* new_profile);

  const raw_ptr<Profile> profile_;
  std::unique_ptr<Delegate> delegate_;
  std::unique_ptr<TokenManagedProfileCreator> profile_creator_;

  // Members below are related to the interception in progress.
  base::WeakPtr<content::WebContents> web_contents_;
  std::string enrollment_token_;
  std::string intercepted_id_;
  bool disable_browser_creation_after_interception_for_testing_ = false;
  raw_ptr<const ProfileAttributesEntry> switch_to_entry_ = nullptr;
};

#endif  // CHROME_BROWSER_SIGNIN_PROFILE_TOKEN_WEB_SIGNIN_INTERCEPTOR_H_
