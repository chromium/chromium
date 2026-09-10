// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LENS_LENS_IDENTITY_DELEGATION_HELPER_H_
#define CHROME_BROWSER_LENS_LENS_IDENTITY_DELEGATION_HELPER_H_

#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/time/time.h"

class Profile;

namespace signin {
class IdentityManager;
}  // namespace signin

namespace lens {

// Generates the SAPISIDHASH V2 header value.
// Returns std::nullopt if the hash cannot be generated (e.g. empty input).
std::optional<std::string> GenerateSapisidHash(
    const std::string& email,
    const std::string& sapisid_cookie,
    const std::string& origin,
    base::Time timestamp);

// Fetches IDENTITY_DELEGATION headers asynchronously.
// Callback receives headers in [key, value, key, value] format.
// If the user is signed out or cookies are missing, it will return only
// the "Origin" header (or empty if origin is empty).
void FetchIdentityDelegationHeaders(
    Profile* profile,
    signin::IdentityManager* identity_manager,
    const std::string& origin,
    std::optional<size_t> authuser_index,
    base::OnceCallback<void(std::vector<std::string>)> callback);

}  // namespace lens

#endif  // CHROME_BROWSER_LENS_LENS_IDENTITY_DELEGATION_HELPER_H_
