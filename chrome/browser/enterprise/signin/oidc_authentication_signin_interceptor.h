// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_SIGNIN_OIDC_AUTHENTICATION_SIGNIN_INTERCEPTOR_H_
#define CHROME_BROWSER_ENTERPRISE_SIGNIN_OIDC_AUTHENTICATION_SIGNIN_INTERCEPTOR_H_

#include <memory>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/enterprise/signin/oidc_metrics_utils.h"
#include "chrome/browser/enterprise/signin/token_managed_profile_creation_delegate.h"
#include "chrome/browser/signin/web_signin_interceptor.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"

namespace content {
class WebContents;
}

namespace policy {
class CloudPolicyClientRegistrationHelper;
}  // namespace policy

class OidcAuthenticationSigninInterceptorTest;

class Profile;
class ProfileAttributesEntry;

using OidcInterceptionCallback = base::OnceCallback<void()>;
using policy::CloudPolicyClient;

// Called after a valid OIDC authentication redirection is captured. The
// interceptor is responsible for starting registration process, collecting user
// consent, and creating/switching to a new managed profile if agreed.

// The main steps of the interception are:
// - Check if the user is elligible to interception in
// MaybeInterceptOidcAuthentication()
// - Show the dialog and wait for the user choice in
// ShowOIDCInterceptionDialog()
// - User choice is received in OnProfileCreationChoice()
// - Go through registration process with StartOidcRegistration() and
// OnClientRegistered()
// - Create a new profile with ManagedProfileCreator and then
// OnNewSignedInProfileCreated()
// - Fetch policies, received in OnPolicyFetchCompleteInNewProfile()
// - Notify the dialog that the profile creation is complete with
// user_choice_handling_done_callback_
// - Wait for the dialog to be closed by the user and open a browser registered
// for policies via oidc
class OidcAuthenticationSigninInterceptor
    : public WebSigninInterceptor,

      // TODO(350960816): Restructure `OidcAuthenticationSigninInterceptor` to
      // be a state machine instead of a keyed service.
      public KeyedService {
 public:
  enum class SigninInterceptionType {
    kProfileSwitch,
    kEnterprise,
  };

  OidcAuthenticationSigninInterceptor(
      Profile* profile,
      std::unique_ptr<WebSigninInterceptor::Delegate> delegate);
  ~OidcAuthenticationSigninInterceptor() override;

  OidcAuthenticationSigninInterceptor(
      const OidcAuthenticationSigninInterceptor&) = delete;
  OidcAuthenticationSigninInterceptor& operator=(
      const OidcAuthenticationSigninInterceptor&) = delete;

  // Intercept and kick off OIDC registration process if the tokens we received
  // are valid.
  virtual void MaybeInterceptOidcAuthentication(
      content::WebContents* intercepted_contents,
      const ProfileManagementOidcTokens& oidc_tokens,
      const std::string& issuer_id,
      const std::string& subject_id,
      OidcInterceptionCallback oidc_callback);

  // KeyedService:
  void Shutdown() override;

  void SetCloudPolicyClientForTesting(
      std::unique_ptr<CloudPolicyClient> client) {
    client_for_testing_ = std::move(client);
  }

 protected:
  virtual void OnPolicyFetchCompleteInNewProfile(bool success);
  virtual void FinalizeSigninInterception();
  virtual void CreateBrowserAfterSigninInterception();

 private:
  friend class MockOidcAuthenticationSigninInterceptor;

  // Cancels any current signin interception and resets the interceptor to its
  // initial state.
  void Reset();

  // `is_dasher_based` should be nullopt when `result` has type
  // `OidcInterceptionResult` since its histogram does not have
  // Dasher-based/Dasherless variants; it should be either True or False when
  // `result` has type `OidcProfileCreationResult` and be used for histogram
  // recording.
  void HandleError(
      std::variant<OidcInterceptionResult, OidcProfileCreationResult> result,
      std::optional<bool> is_dasher_based = std::nullopt);

  // Try to send OIDC tokens to DM server for registration.
  void StartOidcRegistration();
  // Called when OIDC registration finishes, the client should be registered
  // (aka has a dm token) and various information should be included, most
  // importantly, if the 3P user identity is sync-ed to Google or not.
  void OnClientRegistered(std::unique_ptr<CloudPolicyClient> client,
                          std::string preset_profile_guid,
                          base::TimeTicks registration_start_time,
                          CloudPolicyClient::Result result);

  // Called when user makes a decision on the profile creation dialog.
  void OnProfileCreationChoice(
      signin::SigninChoice choice,
      signin::SigninChoiceOperationDoneCallback callback);
  void OnProfileSwitchChoice(SigninInterceptionResult result);
  // Called when the new profile has been created.
  void OnNewSignedInProfileCreated(base::WeakPtr<Profile> new_profile);

  const raw_ptr<Profile, DanglingUntriaged> profile_;
  base::WeakPtr<Profile> new_profile_;
  std::unique_ptr<WebSigninInterceptor::Delegate> delegate_;
  std::unique_ptr<ManagedProfileCreator> profile_creator_;

  // Members below are related to the interception in progress.
  base::WeakPtr<content::WebContents> web_contents_;
  ProfileManagementOidcTokens oidc_tokens_;
  std::string dm_token_;
  std::string client_id_;
  std::string user_display_name_;
  std::string user_email_;
  // Unique id for the OIDC user, format:
  // "iss:<value of 'iss' field>,sub:<value of 'sub'field>"
  // For context, 'iss' is the ID of the OIDC issuer and 'sub' is the
  // unique-per-user subject ID within the issuer.
  std::string unique_user_identifier_;
  bool dasher_based_ = true;
  std::string preset_profile_id_;
  raw_ptr<const ProfileAttributesEntry> switch_to_entry_ = nullptr;
  SkColor profile_color_;
  bool interception_in_progress_ = false;

  std::unique_ptr<policy::CloudPolicyClientRegistrationHelper>
      registration_helper_for_temporary_client_;

  // Used to retain the interception UI bubble until profile creation completes.
  std::unique_ptr<ScopedWebSigninInterceptionBubbleHandle>
      interception_bubble_handle_;

  std::unique_ptr<CloudPolicyClient> client_for_testing_ = nullptr;

  OidcInterceptionCallback oidc_callback_;

  signin::SigninChoiceOperationDoneCallback user_choice_handling_done_callback_;

  base::WeakPtrFactory<OidcAuthenticationSigninInterceptor> weak_factory_{this};

  friend class OidcAuthenticationSigninInterceptorTest;
};

#endif  // CHROME_BROWSER_ENTERPRISE_SIGNIN_OIDC_AUTHENTICATION_SIGNIN_INTERCEPTOR_H_
