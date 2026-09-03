// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/enterprise_webrtc/enterprise_webrtc_api.h"

#include <utility>

#include "base/notreached.h"
#include "chrome/browser/extensions/api/enterprise_webrtc/enterprise_webrtc_api_observer.h"
#include "chrome/common/extensions/api/enterprise_webrtc.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/webrtc_diagnostics.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace extensions {

namespace {

constexpr char kAlreadyCapturingError[] =
    "A capture session is already active for this extension.";
constexpr char kInvalidOriginError[] =
    "The origin filter contains an "
    "entry that is not a valid origin.";
constexpr char kTooManyOriginsError[] =
    "The origin filter contains too many entries.";
constexpr char kFeatureUnavailableError[] =
    "WebRTC diagnostics are unavailable for this profile.";

std::vector<url::Origin> ParseOrigins(std::vector<std::string> origins) {
  std::vector<url::Origin> parsed_origins;
  parsed_origins.reserve(origins.size());
  for (const std::string& origin_str : origins) {
    parsed_origins.push_back(url::Origin::Create(GURL(origin_str)));
  }
  return parsed_origins;
}

}  // namespace

ExtensionFunction::ResponseAction EnterpriseWebrtcStartCaptureFunction::Run() {
  auto params = api::enterprise_webrtc::StartCapture::Params::Create(args());
  // Without this, unparsable arguments would leave `origins` empty, and an
  // empty filter means "capture every origin".
  EXTENSION_FUNCTION_VALIDATE(params);

  std::vector<url::Origin> origins;
  if (params->filter && params->filter->origins) {
    origins = ParseOrigins(std::move(*params->filter->origins));
  }

  switch (content::WebRtcDiagnostics::GetInstance()->StartCaptureForClient(
      browser_context(), extension_id(), std::move(origins))) {
    case content::WebRtcDiagnostics::StartCaptureResult::kSuccess:
      return RespondNow(NoArguments());
    case content::WebRtcDiagnostics::StartCaptureResult::kAlreadyCapturing:
      return RespondNow(Error(kAlreadyCapturingError));
    case content::WebRtcDiagnostics::StartCaptureResult::kInvalidOrigin:
      return RespondNow(Error(kInvalidOriginError));
    case content::WebRtcDiagnostics::StartCaptureResult::kTooManyOrigins:
      return RespondNow(Error(kTooManyOriginsError));
    case content::WebRtcDiagnostics::StartCaptureResult::kUnavailable:
      // The profile is shutting down, so it cannot host a capture session.
      return RespondNow(Error(kFeatureUnavailableError));
  }
  NOTREACHED();
}

ExtensionFunction::ResponseAction
EnterpriseWebrtcGetCaptureStatusFunction::Run() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  api::enterprise_webrtc::StatusResult result;
  // Reports this extension's own session in this profile, not whether some
  // other client is capturing elsewhere in the browser.
  result.active =
      content::WebRtcDiagnostics::GetInstance()->IsCapturingForClient(
          browser_context(), extension_id());

  return RespondNow(WithArguments(result.ToValue()));
}

}  // namespace extensions
