// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/gemini_enterprise/glic_gemini_enterprise_manager.h"

#include <string>
#include <string_view>
#include <utility>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "chrome/browser/glic/gemini_enterprise/geic_enabling.h"
#include "chrome/browser/glic/host/guest_util.h"
#include "chrome/browser/profiles/profile.h"
#include "google_apis/gaia/gaia_urls.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/url_constants.h"

namespace glic {

namespace {

// Maximum length of a URL accepted by `OpenAuthTab`.
constexpr size_t kMaxAuthTabUrlLength = 65536;

// Path of the GE OAuth redirector (trampoline) page (`REDIRECT_URI_PATH` in
// the GE web client's auth service). See cross-repo contract in README.md.
constexpr std::string_view kConnectorOAuthRedirectorPath = "/oauth-redirect";

// Basic shape checks shared by all purposes: rejects invalid URLs, non-HTTPS
// schemes, URLs with embedded credentials, and overly long URLs.
bool HasAllowedAuthTabUrlShape(const GURL& url) {
  return url.is_valid() && url.SchemeIs(url::kHttpsScheme) &&
         !url.has_username() && !url.has_password() &&
         url.spec().length() <= kMaxAuthTabUrlLength;
}

// Returns the histogram suffix for `purpose`. Invalid purposes are recorded
// under the `Unknown` suffix.
// LINT.IfChange(GeicAuthTabPurposeSuffix)
std::string_view GetPurposeHistogramSuffix(mojom::AuthTabPurpose purpose) {
  switch (purpose) {
    case mojom::AuthTabPurpose::kSignIn:
      return "SignIn";
    case mojom::AuthTabPurpose::kConnectorOauth:
      return "ConnectorOAuth";
    case mojom::AuthTabPurpose::kUnknown:
      return "Unknown";
  }
  NOTREACHED();
}
// LINT.ThenChange(//tools/metrics/histograms/metadata/glic/histograms.xml:GeicAuthTabPurpose)

mojom::OpenSignInTabResult ToOpenSignInTabResult(
    mojom::OpenAuthTabResult result) {
  switch (result) {
    case mojom::OpenAuthTabResult::kSuccess:
      return mojom::OpenSignInTabResult::kSuccess;
    case mojom::OpenAuthTabResult::kErrorNoUrl:
      return mojom::OpenSignInTabResult::kErrorNoUrl;
    case mojom::OpenAuthTabResult::kErrorDisallowedUrl:
      return mojom::OpenSignInTabResult::kErrorDisallowedUrl;
    case mojom::OpenAuthTabResult::kErrorFailure:
    case mojom::OpenAuthTabResult::kErrorInvalidPurpose:
      // kErrorInvalidPurpose is unreachable: the deprecated alias always
      // passes kSignIn, which is a valid purpose.
      return mojom::OpenSignInTabResult::kErrorFailure;
    case mojom::OpenAuthTabResult::kUnknown:
      return mojom::OpenSignInTabResult::kUnknown;
  }
  NOTREACHED();
}

mojom::CloseSignInTabResult ToCloseSignInTabResult(
    mojom::CloseAuthTabResult result) {
  switch (result) {
    case mojom::CloseAuthTabResult::kClosedActive:
    case mojom::CloseAuthTabResult::kClosedInactive:
      return mojom::CloseSignInTabResult::kSuccess;
    case mojom::CloseAuthTabResult::kAlreadyClosed:
    // From the caller's perspective, a tab the user navigated away from is no
    // longer the sign-in tab, which is closest to "already closed".
    case mojom::CloseAuthTabResult::kNavigatedAway:
      return mojom::CloseSignInTabResult::kAlreadyClosed;
    case mojom::CloseAuthTabResult::kNoAuthTab:
      return mojom::CloseSignInTabResult::kNoSignInTab;
    case mojom::CloseAuthTabResult::kErrorInvalidPurpose:
      // Unreachable: the deprecated alias always passes kSignIn, which is a
      // valid purpose. Mapped for switch exhaustiveness.
    case mojom::CloseAuthTabResult::kUnknown:
      return mojom::CloseSignInTabResult::kUnknown;
  }
  NOTREACHED();
}

}  // namespace

GlicGeminiEnterpriseManager::GlicGeminiEnterpriseManager(Profile* profile)
    : GlicGeminiEnterpriseManager(profile, glic::GetGuestOrigin(profile)) {}

GlicGeminiEnterpriseManager::GlicGeminiEnterpriseManager(
    Profile* profile,
    const url::Origin& guest_origin)
    : profile_(profile),
      signin_tab_(profile,
                  mojom::AuthTabPurpose::kSignIn,
                  GaiaUrls::GetInstance()->gaia_origin(),
                  guest_origin),
      connector_oauth_tab_(profile,
                           mojom::AuthTabPurpose::kConnectorOauth,
                           GaiaUrls::GetInstance()->gaia_origin(),
                           guest_origin) {}

GlicGeminiEnterpriseManager::~GlicGeminiEnterpriseManager() = default;

void GlicGeminiEnterpriseManager::Bind(
    mojo::PendingReceiver<mojom::GeminiEnterpriseHandler> receiver) {
  if (!geic::IsGeicEnabled(profile_)) {
    return;
  }
  receiver_.reset();
  receiver_.Bind(std::move(receiver));
}

GeicManagedTab* GlicGeminiEnterpriseManager::GetAuthTab(
    mojom::AuthTabPurpose purpose) {
  switch (purpose) {
    case mojom::AuthTabPurpose::kSignIn:
      return &signin_tab_;
    case mojom::AuthTabPurpose::kConnectorOauth:
      return &connector_oauth_tab_;
    case mojom::AuthTabPurpose::kUnknown:
      return nullptr;
  }
  NOTREACHED();
}

bool GlicGeminiEnterpriseManager::IsAuthTabURLAllowed(
    mojom::AuthTabPurpose purpose,
    const GURL& url) {
  if (!HasAllowedAuthTabUrlShape(url)) {
    return false;
  }
  GeicManagedTab* tab = GetAuthTab(purpose);
  if (!tab || !tab->IsExpectedOrigin(url)) {
    return false;
  }
  // A connector OAuth flow must start on the GE redirector page; it
  // trampolines to the 3P provider via `continue_uri`, which is intentionally
  // not validated here.
  if (purpose == mojom::AuthTabPurpose::kConnectorOauth &&
      url.path() != kConnectorOAuthRedirectorPath) {
    return false;
  }
  return true;
}

mojom::OpenAuthTabResponsePtr GlicGeminiEnterpriseManager::OpenAuthTabImpl(
    mojom::AuthTabPurpose purpose,
    const std::optional<GURL>& url) {
  auto response =
      mojom::OpenAuthTabResponse::New(mojom::OpenAuthTabResult::kUnknown);
  GeicManagedTab* tab = GetAuthTab(purpose);
  if (!tab) {
    response->result = mojom::OpenAuthTabResult::kErrorInvalidPurpose;
  } else if (!url.has_value() || !url->is_valid()) {
    response->result = mojom::OpenAuthTabResult::kErrorNoUrl;
  } else if (!profile_ || profile_->IsOffTheRecord()) {
    response->result = mojom::OpenAuthTabResult::kErrorFailure;
  } else if (!IsAuthTabURLAllowed(purpose, *url)) {
    response->result = mojom::OpenAuthTabResult::kErrorDisallowedUrl;
  } else {
    // An OAuth flow carries a fresh `state` each time, so a reused connector
    // OAuth tab must be navigated. The sign-in tab is only reactivated.
    const GeicManagedTab::ReuseMode reuse_mode =
        purpose == mojom::AuthTabPurpose::kConnectorOauth
            ? GeicManagedTab::ReuseMode::kNavigateAndActivate
            : GeicManagedTab::ReuseMode::kActivate;
    response->result = tab->Open(*url, reuse_mode)
                           ? mojom::OpenAuthTabResult::kSuccess
                           : mojom::OpenAuthTabResult::kErrorFailure;
  }

  // Only log the origin; the full URL may carry OAuth request parameters.
  DVLOG(1) << "[glic_gemini_enterprise] OpenAuthTab purpose=" << purpose
           << " origin="
           << (url.has_value() ? url::Origin::Create(*url) : url::Origin())
           << " result=" << response->result;
  base::UmaHistogramEnumeration(
      base::StrCat(
          {"Geic.AuthTab.OpenResult.", GetPurposeHistogramSuffix(purpose)}),
      response->result);
  return response;
}

mojom::CloseAuthTabResponsePtr GlicGeminiEnterpriseManager::CloseAuthTabImpl(
    mojom::AuthTabPurpose purpose) {
  auto response =
      mojom::CloseAuthTabResponse::New(mojom::CloseAuthTabResult::kUnknown);
  GeicManagedTab* tab = GetAuthTab(purpose);
  if (!tab) {
    response->result = mojom::CloseAuthTabResult::kErrorInvalidPurpose;
  } else {
    switch (tab->Close()) {
      case GeicManagedTab::CloseOutcome::kClosedActive:
        response->result = mojom::CloseAuthTabResult::kClosedActive;
        break;
      case GeicManagedTab::CloseOutcome::kClosedInactive:
        response->result = mojom::CloseAuthTabResult::kClosedInactive;
        break;
      case GeicManagedTab::CloseOutcome::kAlreadyClosed:
        response->result = mojom::CloseAuthTabResult::kAlreadyClosed;
        break;
      case GeicManagedTab::CloseOutcome::kNotOpened:
        response->result = mojom::CloseAuthTabResult::kNoAuthTab;
        break;
      case GeicManagedTab::CloseOutcome::kNavigatedAway:
        response->result = mojom::CloseAuthTabResult::kNavigatedAway;
        break;
    }
  }

  DVLOG(1) << "[glic_gemini_enterprise] CloseAuthTab purpose=" << purpose
           << " result=" << response->result;
  base::UmaHistogramEnumeration(
      base::StrCat(
          {"Geic.AuthTab.CloseResult.", GetPurposeHistogramSuffix(purpose)}),
      response->result);
  return response;
}

void GlicGeminiEnterpriseManager::OpenAuthTab(
    mojom::OpenAuthTabOptionsPtr options,
    OpenAuthTabCallback callback) {
  std::move(callback).Run(OpenAuthTabImpl(options->purpose, options->url));
}

void GlicGeminiEnterpriseManager::CloseAuthTab(
    mojom::CloseAuthTabOptionsPtr options,
    CloseAuthTabCallback callback) {
  std::move(callback).Run(CloseAuthTabImpl(options->purpose));
}

void GlicGeminiEnterpriseManager::OpenSignInTab(
    mojom::OpenSignInTabOptionsPtr options,
    OpenSignInTabCallback callback) {
  std::optional<GURL> url;
  if (options) {
    url = options->signin_url;
  }
  std::move(callback).Run(ToOpenSignInTabResult(
      OpenAuthTabImpl(mojom::AuthTabPurpose::kSignIn, url)->result));
}

void GlicGeminiEnterpriseManager::CloseSignInTab(
    mojom::CloseSignInTabOptionsPtr options,
    CloseSignInTabCallback callback) {
  std::move(callback).Run(ToCloseSignInTabResult(
      CloseAuthTabImpl(mojom::AuthTabPurpose::kSignIn)->result));
}

}  // namespace glic
