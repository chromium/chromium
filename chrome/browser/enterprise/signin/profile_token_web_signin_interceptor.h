// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_SIGNIN_PROFILE_TOKEN_WEB_SIGNIN_INTERCEPTOR_H_
#define CHROME_BROWSER_ENTERPRISE_SIGNIN_PROFILE_TOKEN_WEB_SIGNIN_INTERCEPTOR_H_

#include <memory>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/enterprise/signin/token_managed_profile_creation_delegate.h"
#include "chrome/browser/signin/web_signin_interceptor.h"
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
//   new profile's storage, using `TokenManagedProfileCreationDelegate`.
// * At this point, the flow ends in this profile, and continues in the new
//   profile.
// * When the account is available on the web in the new profile:
//   - A new browser window is created for the new profile,
//   - The tab is moved to the new profile.
class ProfileTokenWebSigninInterceptor : public WebSigninInterceptor,
                                         public KeyedService {
 public:
  enum class SigninInterceptionType {
    kProfileSwitch,
    kEnterprise,
  };

  ProfileTokenWebSigninInterceptor(
      Profile* profile,
      std::unique_ptr<WebSigninInterceptor::Delegate> delegate);
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

  void OnProfileCreationChoice(SigninInterceptionResult create);

  // Called when the new browser is created after interception. Passed as
  // callback to `session_startup_helper_`.
  void OnNewBrowserCreated(bool is_new_profile);

  void OnNewSignedInProfileCreated(base::WeakPtr<Profile> new_profile);

  const raw_ptr<Profile, DanglingUntriaged> profile_;
  std::unique_ptr<Delegate> delegate_;
  std::unique_ptr<ManagedProfileCreator> profile_creator_;

  // Members below are related to the interception in progress.
  base::WeakPtr<content::WebContents> web_contents_;
  std::string enrollment_token_;
  std::string intercepted_id_;
  bool disable_browser_creation_after_interception_for_testing_ = false;
  raw_ptr<const ProfileAttributesEntry> switch_to_entry_ = nullptr;
  SkColor profile_color_;
  // Used to retain the interception UI bubble until profile creation completes.
  std::unique_ptr<ScopedWebSigninInterceptionBubbleHandle>
      interception_bubble_handle_;
};

#endif  // CHROME_BROWSER_ENTERPRISE_SIGNIN_PROFILE_TOKEN_WEB_SIGNIN_INTERCEPTOR_H_
