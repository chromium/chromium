// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_K_ANONYMITY_SERVICE_K_ANONYMITY_SERVICE_URLS_H_
#define CHROME_BROWSER_K_ANONYMITY_SERVICE_K_ANONYMITY_SERVICE_URLS_H_

inline constexpr char kGenNonUniqueUserIdPath[] = "/v1/generateShortIdentifier";
inline constexpr char kFetchKeysPathFmt[] =
    "/v1/%d/fetchKeys?key=%s";  // Put the short ID in the path.
inline constexpr char kIssueTrustTokenPathFmt[] =
    "/v1/%d/issueTrustToken";  // Put the short ID in the path.

inline constexpr char kJoinSetPathFmt[] = "/v1/types/%s/sets/%s:join?key=%s";
inline constexpr char kJoinSetOhttpPath[] = "/v1/proxy/keys?key=";

inline constexpr char kQuerySetsPath[] = "/v1:query?key=";
inline constexpr char kQuerySetOhttpPath[] = "/v1/proxy/keys?key=";

#endif  // CHROME_BROWSER_K_ANONYMITY_SERVICE_K_ANONYMITY_SERVICE_URLS_H_
