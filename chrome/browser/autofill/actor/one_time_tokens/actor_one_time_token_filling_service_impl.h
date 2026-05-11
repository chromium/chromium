// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_ACTOR_ONE_TIME_TOKENS_ACTOR_ONE_TIME_TOKEN_FILLING_SERVICE_IMPL_H_
#define CHROME_BROWSER_AUTOFILL_ACTOR_ONE_TIME_TOKENS_ACTOR_ONE_TIME_TOKEN_FILLING_SERVICE_IMPL_H_

#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/autofill/actor/one_time_tokens/actor_one_time_token_filling_service.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/tabs/public/tab_interface.h"

class Profile;

namespace autofill {
// Implementation for the ActorOneTimeTokenFillingService. This is owned by
// `actor::ExecutionEngine`. It's called by the AttemptOtpFillingTool and
// interacts with the backend OneTimeTokenService.
class ActorOneTimeTokenFillingServiceImpl
    : public ActorOneTimeTokenFillingService {
 public:
  explicit ActorOneTimeTokenFillingServiceImpl(Profile* profile);
  ~ActorOneTimeTokenFillingServiceImpl() override;

  // ActorOneTimeTokenFillingService:
  void RetrieveOtp(tabs::TabHandle tab_handle,
                   const std::vector<FieldGlobalId>& trigger_field_ids,
                   base::OnceCallback<void(std::string)> callback) override;

  void FillOtp(tabs::TabHandle tab_handle,
               const std::vector<FieldGlobalId>& trigger_field_ids,
               const std::string& otp,
               base::OnceCallback<void(bool)> callback) override;

 private:
  raw_ptr<Profile> profile_;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_AUTOFILL_ACTOR_ONE_TIME_TOKENS_ACTOR_ONE_TIME_TOKEN_FILLING_SERVICE_IMPL_H_
