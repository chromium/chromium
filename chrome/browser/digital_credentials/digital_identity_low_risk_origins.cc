// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/digital_credentials/digital_identity_low_risk_origins.h"

#include "base/strings/string_util.h"
#include "chrome/browser/digital_credentials/digital_credentials_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/render_frame_host.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace digital_credentials {
namespace {

using content::RenderFrameHost;

/*
 * Temporary list of origins considered lower-risk to facilitate an experimental
 * test of the Digital Credential API while standardized trust signals are being
 * developed (such as in
 * https://github.com/WICG/digital-credentials/issues/136). This list is used
 * only as a heuristic for UI purposes, not to gate any API access. To submit
 * proposals for changes to this list, please file an issue at
 * https://bit.ly/cr-dc-origin-risk
 */
// TODO(https://crbug.com/350946977): Populate.
constexpr const char* const kKnownLowRiskOrigins[] = {};

// Helper function with the core matching logic. Matches the origin to check
// against the list of known origins. The matching is done by normalizing both
// the origin to check and each origin in the list (stripping "www." prefix, if
// present) and comparing the resulting origins.
bool IsLowRiskOriginMatcher(const url::Origin& to_check,
                            const std::vector<std::string>& known_origins) {
  for (const std::string& low_risk_origin_str : known_origins) {
    url::Origin low_risk_origin_url =
        url::Origin::Create(GURL(low_risk_origin_str));

    // Normalize the host of the current low_risk_origin from the list.
    std::string normalized_low_risk_host = low_risk_origin_url.host();
    if (base::StartsWith(normalized_low_risk_host, "www.")) {
      normalized_low_risk_host = normalized_low_risk_host.substr(4);
    }

    // Normalize the host of the origin to check.
    std::string normalized_to_check_host = to_check.host();
    if (base::StartsWith(normalized_to_check_host, "www.")) {
      normalized_to_check_host = normalized_to_check_host.substr(4);
    }

    // Compare schemes, ports, and the normalized hosts.
    if (low_risk_origin_url.scheme() == to_check.scheme() &&
        low_risk_origin_url.port() == to_check.port() &&
        normalized_low_risk_host == normalized_to_check_host) {
      return true;
    }
  }
  return false;
}

bool IsLastCommittedURLLowFriction(RenderFrameHost& render_frame_host) {
  Profile* profile =
      Profile::FromBrowserContext(render_frame_host.GetBrowserContext());
  if (!profile) {
    return false;
  }

  DigitalCredentialsKeyedService* service =
      DigitalCredentialsKeyedServiceFactory::GetForProfile(profile);
  if (!service) {
    return false;
  }

  return service->IsLowFrictionUrl(render_frame_host.GetLastCommittedURL());
}

}  // anonymous namespace

bool IsLastCommittedOriginLowRisk(RenderFrameHost& render_frame_host) {
  // Convert the array of C strings to a vector of strings. This is fine since
  // the list is expected to be small and the strings are all compile-time
  // constants.
  std::vector<std::string> origins_vector;
  for (const char* origin_str : kKnownLowRiskOrigins) {
    origins_vector.emplace_back(origin_str);
  }
  return IsLowRiskOriginMatcher(render_frame_host.GetLastCommittedOrigin(),
                                origins_vector) ||
         IsLastCommittedURLLowFriction(render_frame_host);
}

bool IsLowRiskOriginMatcherForTesting(
    const url::Origin& to_check,
    const std::vector<std::string>& known_origins) {
  return IsLowRiskOriginMatcher(to_check, known_origins);
}

}  // namespace digital_credentials
