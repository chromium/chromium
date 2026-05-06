// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_ACTOR_ONE_TIME_TOKENS_ACTOR_ONE_TIME_TOKEN_FILLING_SERVICE_H_
#define CHROME_BROWSER_AUTOFILL_ACTOR_ONE_TIME_TOKENS_ACTOR_ONE_TIME_TOKEN_FILLING_SERVICE_H_

#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "components/actor/core/shared_types.h"
#include "components/tabs/public/tab_interface.h"

namespace autofill {

// Interface for the Actor tooling to interact with One-Time Tokens (OTT) or
// one-time passwords (OTP) filling.
//
// A note on terminology: The terms OTT and OTP are not quite interchangeable.
// OTTs are a broader category that could include entire verification urls, for
// example, not just a few digits.
// So far, this service is only intended for OTP filling.
class ActorOneTimeTokenFillingService {
 public:
  virtual ~ActorOneTimeTokenFillingService() = default;

  // Asynchronously retrieves an OTP for the profile associated with the tab.
  //
  // The `callback` will be invoked with the retrieved OTP string, or an empty
  // string if retrieval fails or no OTP is available.
  virtual void RetrieveOtp(
      tabs::TabHandle tab_handle,
      const std::vector<::actor::PageTarget>& trigger_fields,
      base::OnceCallback<void(std::string)> callback) = 0;

  // Asynchronously fills the `otp` into the field(s) identified by
  // `trigger_fields` for the given `tab`.
  //
  // The `callback` will be invoked with a boolean indicating whether the
  // filling operation was successful.
  virtual void FillOtp(tabs::TabHandle tab_handle,
                       const std::vector<::actor::PageTarget>& trigger_fields,
                       const std::string& otp,
                       base::OnceCallback<void(bool)> callback) = 0;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_AUTOFILL_ACTOR_ONE_TIME_TOKENS_ACTOR_ONE_TIME_TOKEN_FILLING_SERVICE_H_
