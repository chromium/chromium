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
#include "components/one_time_tokens/core/browser/user_data_processing_consent_states.h"
#include "url/origin.h"

namespace content {
class RenderFrameHost;
class WebContents;
}  // namespace content

namespace autofill {

// LINT.IfChange(FormFillingContextValidationResult)
// Detailed result of a `ValidateFormFillingContext()` call: secure, or the
// first check that failed.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class FormFillingContextValidationResult {
  kSecure = 0,
  kEmptyTriggerFieldIds = 1,
  // The tab, its `WebContents` or its primary main frame is not available.
  kTabNotAvailable = 2,
  // The page is not loaded over HTTPS with a valid certificate and without
  // mixed content.
  kInsecurePage = 3,
  // The tab has no `BrowserAutofillManager` (e.g. Autofill is not available).
  kNoAutofillManager = 4,
  kFormStructureNotFound = 5,
  kTriggerFieldNotFound = 6,
  // The form submits to an insecure (mixed content) action.
  kMixedContentFormAction = 7,
  // The form's OTP fields don't all share the trigger field's origin (e.g. part
  // of a multi-field OTP form is in a cross-origin frame).
  kOtpFieldsFromDifferentOrigins = 8,
  // The frame of a trigger field was not found.
  kFrameNotFound = 9,
  // The frame or origin of a trigger field may not be filled.
  kInvalidFrameOrOrigin = 10,
  kMaxValue = kInvalidFrameOrOrigin,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/one_time_tokens/enums.xml:ActorOtpFormFillingContextValidationResult)

// Interface for the Actor tooling to interact with One-Time Tokens (OTT) or
// one-time passwords (OTP) filling.
//
// A note on terminology: The terms OTT and OTP are not quite interchangeable.
// OTTs are a broader category that could include entire verification urls, for
// example, not just a few digits.
// So far, this service is only intended for OTP filling.
class ActorOneTimeTokenFillingService {
 public:
  using FetchUserDataProcessingConsentCallback = base::OnceCallback<void(
      std::optional<one_time_tokens::UserDataProcessingConsentStates>)>;

  // The OTP type that the model predicted for a `RetrieveOtp()` request. Only
  // used to pick the RetrieveOtp histogram suffix; never logged as a value.
  enum class PredictedOtpType {
    kUnknown,
    kSms,
    kEmail,
    kAuthenticatorApp,
  };

  virtual ~ActorOneTimeTokenFillingService() = default;

  // Records the start of a sign-in flow by observing navigations.
  // Called once before filling starts.
  // `web_contents`: The WebContents where the login filling attempt is
  //     happening. May be null, in which case this is a no-op.
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
      content::WebContents* web_contents,
      const url::Origin& origin,
      bool should_use_strong_matching,
      base::span<const int> global_frame_ids) = 0;

  // Clears the tracking if the overall login attempt fails or is aborted.
  virtual void AbortLoginTracking() = 0;

  // Retrieves and clears the tracked login context. Clearing the context
  // ensures that OTP tracking is only used once per actor login attempt.
  virtual std::optional<ActorLoginContext> ConsumeLoginContext() = 0;

  // Returns the origin of the active login context, if any.
  virtual std::optional<url::Origin> GetLoginContextOrigin() const = 0;

  // Returns whether strong matching should be used for the active login
  // context, or false if no active login context exists.
  virtual bool GetLoginContextShouldUseStrongMatching() const = 0;

  // Asynchronously retrieves an OTP for the profile associated with
  // `web_contents`. `predicted_otp_type` is the OTP type the model predicted
  // for the request. When `allow_psl_matching` is true, OTPs whose sender
  // domain matches `otp_frame_origin` via the Public Suffix List are accepted
  // in addition to exact, `www.` subdomain, and affiliated matches.
  //
  // The `callback` will be invoked with the retrieved OTP string, or an empty
  // string if retrieval fails or no OTP is available.
  virtual void RetrieveOtp(
      content::WebContents* web_contents,
      const url::Origin& otp_frame_origin,
      const std::vector<FieldGlobalId>& trigger_field_ids,
      bool allow_psl_matching,
      PredictedOtpType predicted_otp_type,
      base::OnceCallback<
          void(base::expected<std::string,
                              one_time_tokens::OneTimeTokenRetrievalError>)>
          callback) = 0;

  // Asynchronously fills the `otp` into the field(s) identified by
  // `trigger_field_ids` in the given `web_contents`.
  //
  // The `callback` will be invoked with a boolean indicating whether the
  // filling operation was successful.
  virtual void FillOtp(content::WebContents* web_contents,
                       const std::vector<FieldGlobalId>& trigger_field_ids,
                       const std::string& otp,
                       base::OnceCallback<void(bool)> callback) = 0;

  // Validates whether the page in `web_contents` and its subresources are
  // loaded over secure HTTPS, whether the `BrowserAutofillManager` cache
  // contains a `FormStructure` for `trigger_field_ids`, and whether the form
  // submits to a secure endpoint. Note that the last check is more of an early
  // warning system, rather than a strong security guarantee, since it is not
  // impossible to bypass. The actual security restrictions are enforced via
  // other means (e.g `InsecureFormNavigationThrottle` and
  // `MixedContentChecker`). Returns `kSecure`, or the first check that failed.
  virtual FormFillingContextValidationResult ValidateFormFillingContext(
      content::WebContents* web_contents,
      base::span<const FieldGlobalId> trigger_field_ids) const = 0;

  // Asynchronously fetches user data processing consent states from the
  // backend.
  virtual void FetchUserDataProcessingConsent(
      FetchUserDataProcessingConsentCallback callback) = 0;

  // Returns a weak pointer to this service.
  virtual base::WeakPtr<ActorOneTimeTokenFillingService> GetWeakPtr() = 0;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_AUTOFILL_ACTOR_ONE_TIME_TOKENS_ACTOR_ONE_TIME_TOKEN_FILLING_SERVICE_H_
