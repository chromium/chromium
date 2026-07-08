// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_ACTOR_ONE_TIME_TOKENS_ACTOR_ONE_TIME_TOKEN_FILLING_SERVICE_H_
#define CHROME_BROWSER_AUTOFILL_ACTOR_ONE_TIME_TOKENS_ACTOR_ONE_TIME_TOKEN_FILLING_SERVICE_H_

#include <optional>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/types/expected.h"
#include "chrome/browser/autofill/actor/one_time_tokens/actor_login_context.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/one_time_tokens/core/browser/one_time_token_retrieval_error.h"
#include "components/tabs/public/tab_interface.h"
#include "url/origin.h"

namespace autofill {

// Describes the status of the target form filling context during validation.
enum class FormFillingContextStatus {
  // The tab, form, and security state are valid and cryptographic HTTPS is
  // verified.
  kSecure,
  // The page or form is insecure (e.g., mixed content or HTTP submission).
  kInsecureContext,
  // The form structure or Autofill manager could not be found.
  kFormNotFound,
  // The target tab is null or has no WebContents.
  kTabNotAvailable,
};

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

  // Records the start of a sign-in flow by observing navigations.
  // Called once before filling starts.
  // `tab_handle`: The tab where the login filling attempt is happening.
  // `origin`: The request origin of the credential being filled. This
  //     parameter is pass-through and stored directly in `ActorLoginContext`.
  // `should_use_strong_matching`: Whether strong matching (exact origin match
  //     or strong affiliations) is required when verifying if subsequent OTP
  //     forms belong to this flow. This parameter is pass-through and stored
  //     directly in `ActorLoginContext`.
  // `global_frame_ids`: The global frame IDs of all frames where password
  //     filling will occur. Passed as `int` rather than `FrameTreeNodeId`
  //     because these IDs are extracted in `components/password_manager`,
  //     which cannot depend on `content::FrameTreeNodeId`. Unlike the prior
  //     parameters (`origin` and `should_use_strong_matching`), frames are
  //     actively tracked by observing navigations in this class and do not rely
  //     on the prior parameters. Note that navigation counting for these frames
  //     does not take into account whether a frame's origin changes across
  //     navigations.
  virtual void OnPasswordFillingStarted(
      tabs::TabHandle tab_handle,
      const url::Origin& origin,
      bool should_use_strong_matching,
      base::span<const int> global_frame_ids) = 0;

  // Clears the tracking if the overall login attempt fails or is aborted.
  virtual void AbortLoginTracking() = 0;

  // Retrieves and clears the tracked login context. Clearing the context
  // ensures that OTP tracking is only used once per actor login attempt.
  virtual std::optional<ActorLoginContext> ConsumeLoginContext() = 0;

  // Asynchronously retrieves an OTP for the profile associated with the tab.
  //
  // The `callback` will be invoked with the retrieved OTP string, or an empty
  // string if retrieval fails or no OTP is available.
  virtual void RetrieveOtp(
      tabs::TabHandle tab_handle,
      const std::vector<FieldGlobalId>& trigger_field_ids,
      base::OnceCallback<
          void(base::expected<std::string,
                              one_time_tokens::OneTimeTokenRetrievalError>)>
          callback) = 0;

  // Asynchronously fills the `otp` into the field(s) identified by
  // `trigger_field_ids` for the given `tab`.
  //
  // The `callback` will be invoked with a boolean indicating whether the
  // filling operation was successful.
  virtual void FillOtp(tabs::TabHandle tab_handle,
                       const std::vector<FieldGlobalId>& trigger_field_ids,
                       const std::string& otp,
                       base::OnceCallback<void(bool)> callback) = 0;

  // Validates whether the page in `tab_handle` and its subresources are
  // loaded over secure HTTPS, whether the `BrowserAutofillManager` cache
  // contains a `FormStructure` for `trigger_field_ids`, and whether the form
  // submits to a secure endpoint. Note that the last check is more of an early
  // warning system, rather than a strong security guarantee, since it is not
  // impossible to bypass. The actual security restrictions are enforced via
  // other means (e.g `InsecureFormNavigationThrottle` and
  // `MixedContentChecker`).
  virtual FormFillingContextStatus ValidateFormFillingContext(
      tabs::TabHandle tab_handle,
      base::span<const FieldGlobalId> trigger_field_ids) const = 0;

  // Returns a weak pointer to this service.
  virtual base::WeakPtr<ActorOneTimeTokenFillingService> GetWeakPtr() = 0;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_AUTOFILL_ACTOR_ONE_TIME_TOKENS_ACTOR_ONE_TIME_TOKEN_FILLING_SERVICE_H_
