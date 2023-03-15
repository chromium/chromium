// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/identity/identity_launch_web_auth_flow_function.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/metrics/histogram_functions.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "chrome/browser/extensions/api/identity/identity_constants.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/identity.h"

namespace extensions {

namespace {

static const char kChromiumDomainRedirectUrlPattern[] =
    "https://%s.chromiumapp.org/";

IdentityLaunchWebAuthFlowFunction::Error WebAuthFlowFailureToError(
    WebAuthFlow::Failure failure) {
  switch (failure) {
    case WebAuthFlow::WINDOW_CLOSED:
    case WebAuthFlow::USER_NAVIGATED_AWAY:
      return IdentityLaunchWebAuthFlowFunction::Error::kUserRejected;
    case WebAuthFlow::INTERACTION_REQUIRED:
      return IdentityLaunchWebAuthFlowFunction::Error::kInteractionRequired;
    case WebAuthFlow::LOAD_FAILED:
      return IdentityLaunchWebAuthFlowFunction::Error::kPageLoadFailure;
    case WebAuthFlow::TIMED_OUT:
      return IdentityLaunchWebAuthFlowFunction::Error::kPageLoadTimedOut;
    default:
      NOTREACHED() << "Unexpected error from web auth flow: " << failure;
      return IdentityLaunchWebAuthFlowFunction::Error::kUnexpectedError;
  }
}

std::string ErrorToString(IdentityLaunchWebAuthFlowFunction::Error error) {
  switch (error) {
    case IdentityLaunchWebAuthFlowFunction::Error::kNone:
      NOTREACHED()
          << "This function is not expected to be called with no error";
      return std::string();
    case IdentityLaunchWebAuthFlowFunction::Error::kOffTheRecord:
      return identity_constants::kOffTheRecord;
    case IdentityLaunchWebAuthFlowFunction::Error::kUserRejected:
      return identity_constants::kUserRejected;
    case IdentityLaunchWebAuthFlowFunction::Error::kInteractionRequired:
      return identity_constants::kInteractionRequired;
    case IdentityLaunchWebAuthFlowFunction::Error::kPageLoadFailure:
      return identity_constants::kPageLoadFailure;
    case IdentityLaunchWebAuthFlowFunction::Error::kUnexpectedError:
      return identity_constants::kInvalidRedirect;
    case IdentityLaunchWebAuthFlowFunction::Error::kPageLoadTimedOut:
      return identity_constants::kPageLoadTimedOut;
  }
}

void RecordHistogramFunctionResult(
    IdentityLaunchWebAuthFlowFunction::Error error) {
  base::UmaHistogramEnumeration("Signin.Extensions.LaunchWebAuthFlowResult",
                                error);
}

}  // namespace

BASE_FEATURE(kNonInteractiveTimeoutForWebAuthFlow,
             "NonInteractiveTimeoutForWebAuthFlow",
             base::FEATURE_ENABLED_BY_DEFAULT);

IdentityLaunchWebAuthFlowFunction::IdentityLaunchWebAuthFlowFunction() =
    default;

IdentityLaunchWebAuthFlowFunction::~IdentityLaunchWebAuthFlowFunction() {
  if (auth_flow_)
    auth_flow_.release()->DetachDelegateAndDelete();
}

ExtensionFunction::ResponseAction IdentityLaunchWebAuthFlowFunction::Run() {
  Profile* profile = Profile::FromBrowserContext(browser_context());
  if (profile->IsOffTheRecord()) {
    Error error = Error::kOffTheRecord;

    RecordHistogramFunctionResult(error);
    return RespondNow(ExtensionFunction::Error(ErrorToString(error)));
  }

  absl::optional<api::identity::LaunchWebAuthFlow::Params> params =
      api::identity::LaunchWebAuthFlow::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  GURL auth_url(params->details.url);
  WebAuthFlow::Mode mode =
      params->details.interactive && *params->details.interactive
          ? WebAuthFlow::INTERACTIVE
          : WebAuthFlow::SILENT;

  auto abort_on_load_for_non_interactive = WebAuthFlow::AbortOnLoad::kYes;
  absl::optional<base::TimeDelta> timeout_for_non_interactive = absl::nullopt;
  if (base::FeatureList::IsEnabled(kNonInteractiveTimeoutForWebAuthFlow)) {
    abort_on_load_for_non_interactive =
        params->details.abort_on_load_for_non_interactive.value_or(true)
            ? WebAuthFlow::AbortOnLoad::kYes
            : WebAuthFlow::AbortOnLoad::kNo;
    if (params->details.timeout_ms_for_non_interactive) {
      timeout_for_non_interactive = std::clamp(
          base::Milliseconds(*params->details.timeout_ms_for_non_interactive),
          base::TimeDelta(), WebAuthFlow::kNonInteractiveMaxTimeout);
    }
  }

  // Set up acceptable target URLs. (Does not include chrome-extension
  // scheme for this version of the API.)
  InitFinalRedirectURLPrefix(extension()->id());

  AddRef();  // Balanced in OnAuthFlowSuccess/Failure.

  auth_flow_ = std::make_unique<WebAuthFlow>(
      this, profile, auth_url, mode, WebAuthFlow::LAUNCH_WEB_AUTH_FLOW,
      abort_on_load_for_non_interactive, timeout_for_non_interactive);
  // An extension might call `launchWebAuthFlow()` with any URL. Add an infobar
  // to attribute displayed URL to the extension.
  auth_flow_->SetShouldShowInfoBar(extension()->name());

  auth_flow_->Start();
  return RespondLater();
}

void IdentityLaunchWebAuthFlowFunction::InitFinalRedirectURLPrefixForTest(
    const std::string& extension_id) {
  InitFinalRedirectURLPrefix(extension_id);
}

void IdentityLaunchWebAuthFlowFunction::InitFinalRedirectURLPrefix(
    const std::string& extension_id) {
  if (final_url_prefix_.is_empty()) {
    final_url_prefix_ = GURL(base::StringPrintf(
        kChromiumDomainRedirectUrlPattern, extension_id.c_str()));
  }
}

void IdentityLaunchWebAuthFlowFunction::OnAuthFlowFailure(
    WebAuthFlow::Failure failure) {
  Error error = WebAuthFlowFailureToError(failure);

  RecordHistogramFunctionResult(error);
  RespondWithError(ErrorToString(error));
  if (auth_flow_)
    auth_flow_.release()->DetachDelegateAndDelete();
  Release();  // Balanced in Run.
}

void IdentityLaunchWebAuthFlowFunction::OnAuthFlowURLChange(
    const GURL& redirect_url) {
  if (redirect_url.GetWithEmptyPath() == final_url_prefix_) {
    RecordHistogramFunctionResult(
        IdentityLaunchWebAuthFlowFunction::Error::kNone);
    Respond(WithArguments(redirect_url.spec()));
    if (auth_flow_)
      auth_flow_.release()->DetachDelegateAndDelete();
    Release();  // Balanced in RunAsync.
  }
}

WebAuthFlow* IdentityLaunchWebAuthFlowFunction::GetWebAuthFlowForTesting() {
  return auth_flow_.get();
}

}  // namespace extensions
