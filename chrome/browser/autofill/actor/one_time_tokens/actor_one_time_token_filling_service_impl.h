// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_ACTOR_ONE_TIME_TOKENS_ACTOR_ONE_TIME_TOKEN_FILLING_SERVICE_IMPL_H_
#define CHROME_BROWSER_AUTOFILL_ACTOR_ONE_TIME_TOKENS_ACTOR_ONE_TIME_TOKEN_FILLING_SERVICE_IMPL_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/types/expected.h"
#include "chrome/browser/autofill/actor/one_time_tokens/actor_login_context.h"
#include "chrome/browser/autofill/actor/one_time_tokens/actor_one_time_token_filling_service.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/one_time_tokens/core/browser/one_time_token_retrieval_error.h"
#include "components/one_time_tokens/core/browser/one_time_token_service.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents_observer.h"

class Profile;

namespace content {
class NavigationHandle;
}

namespace autofill {

class ActorFillingObserver;

// Implementation for the ActorOneTimeTokenFillingService. This is owned by
// `actor::ExecutionEngine`. It's called by the AttemptOtpFillingTool and
// interacts with the backend OneTimeTokenService.
class ActorOneTimeTokenFillingServiceImpl
    : public ActorOneTimeTokenFillingService,
      public content::WebContentsObserver {
 public:
  explicit ActorOneTimeTokenFillingServiceImpl(Profile* profile);
  ~ActorOneTimeTokenFillingServiceImpl() override;

  // ActorOneTimeTokenFillingService:
  void OnPasswordFillingStarted(
      tabs::TabHandle tab_handle,
      const url::Origin& origin,
      bool should_use_strong_matching,
      base::span<const int> global_frame_ids) override;
  void AbortLoginTracking() override;
  std::optional<ActorLoginContext> ConsumeLoginContext() override;
  void RetrieveOtp(
      tabs::TabHandle tab_handle,
      const std::vector<FieldGlobalId>& trigger_field_ids,
      base::OnceCallback<
          void(base::expected<std::string,
                              one_time_tokens::OneTimeTokenRetrievalError>)>
          callback) override;

  void FillOtp(tabs::TabHandle tab_handle,
               const std::vector<FieldGlobalId>& trigger_field_ids,
               const std::string& otp,
               base::OnceCallback<void(bool)> callback) override;
  FormFillingContextStatus ValidateFormFillingContext(
      tabs::TabHandle tab_handle,
      base::span<const FieldGlobalId> trigger_field_ids) const override;
  base::WeakPtr<ActorOneTimeTokenFillingService> GetWeakPtr() override;

  // content::WebContentsObserver:
  void DidFinishNavigation(content::NavigationHandle* handle) override;

 private:
  void OnOneTimeTokenReceived(
      one_time_tokens::OneTimeTokenSource source,
      base::expected<one_time_tokens::OneTimeToken,
                     one_time_tokens::OneTimeTokenRetrievalError> result);

  raw_ptr<Profile> profile_;
  std::optional<ActorLoginContext> active_login_context_;
  one_time_tokens::ExpiringSubscription subscription_;
  base::OnceCallback<void(
      base::expected<std::string, one_time_tokens::OneTimeTokenRetrievalError>)>
      retrieve_otp_callback_;
  std::unique_ptr<ActorFillingObserver> filling_observer_;

  base::WeakPtrFactory<ActorOneTimeTokenFillingServiceImpl> weak_ptr_factory_{
      this};
};

}  // namespace autofill

#endif  // CHROME_BROWSER_AUTOFILL_ACTOR_ONE_TIME_TOKENS_ACTOR_ONE_TIME_TOKEN_FILLING_SERVICE_IMPL_H_
