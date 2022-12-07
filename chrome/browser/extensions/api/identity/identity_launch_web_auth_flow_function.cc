// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/identity/identity_launch_web_auth_flow_function.h"

#include <memory>
#include <utility>

#include "base/strings/stringprintf.h"
#include "chrome/browser/extensions/api/identity/identity_constants.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/identity.h"

namespace extensions {

namespace {

static const char kChromiumDomainRedirectUrlPattern[] =
    "https://%s.chromiumapp.org/";

}  // namespace

IdentityLaunchWebAuthFlowFunction::IdentityLaunchWebAuthFlowFunction() {}

IdentityLaunchWebAuthFlowFunction::~IdentityLaunchWebAuthFlowFunction() {
  if (auth_flow_)
    auth_flow_.release()->DetachDelegateAndDelete();
}

ExtensionFunction::ResponseAction IdentityLaunchWebAuthFlowFunction::Run() {
  Profile* profile = Profile::FromBrowserContext(browser_context());
  if (profile->IsOffTheRecord()) {
    return RespondNow(Error(identity_constants::kOffTheRecord));
  }

  std::unique_ptr<api::identity::LaunchWebAuthFlow::Params> params(
      api::identity::LaunchWebAuthFlow::Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  GURL auth_url(params->details.url);
  WebAuthFlow::Mode mode =
      params->details.interactive && *params->details.interactive ?
      WebAuthFlow::INTERACTIVE : WebAuthFlow::SILENT;

  // Set up acceptable target URLs. (Does not include chrome-extension
  // scheme for this version of the API.)
  InitFinalRedirectURLPrefix(extension()->id());

  AddRef();  // Balanced in OnAuthFlowSuccess/Failure.

  auth_flow_ = std::make_unique<WebAuthFlow>(this, profile, auth_url, mode,
                                             WebAuthFlow::LAUNCH_WEB_AUTH_FLOW);
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
  std::string error;
  switch (failure) {
    case WebAuthFlow::WINDOW_CLOSED:
    case WebAuthFlow::USER_NAVIGATED_AWAY:
      error = identity_constants::kUserRejected;
      break;
    case WebAuthFlow::INTERACTION_REQUIRED:
      error = identity_constants::kInteractionRequired;
      break;
    case WebAuthFlow::LOAD_FAILED:
      error = identity_constants::kPageLoadFailure;
      break;
    default:
      NOTREACHED() << "Unexpected error from web auth flow: " << failure;
      error = identity_constants::kInvalidRedirect;
      break;
  }
  Respond(Error(std::move(error)));
  if (auth_flow_)
    auth_flow_.release()->DetachDelegateAndDelete();
  Release();  // Balanced in Run.
}

void IdentityLaunchWebAuthFlowFunction::OnAuthFlowURLChange(
    const GURL& redirect_url) {
  if (redirect_url.GetWithEmptyPath() == final_url_prefix_) {
    Respond(WithArguments(redirect_url.spec()));
    if (auth_flow_)
      auth_flow_.release()->DetachDelegateAndDelete();
    Release();  // Balanced in RunAsync.
  }
}

}  // namespace extensions
