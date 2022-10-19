// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_K_ANONYMITY_SERVICE_K_ANONYMITY_SERVICE_URLS_H_
#define CHROME_BROWSER_K_ANONYMITY_SERVICE_K_ANONYMITY_SERVICE_URLS_H_

constexpr char kGenNonUniqueUserIdPath[] = "/v1/generateShortIdentifier";
constexpr char kFetchKeysPathFmt[] =
    "/v1/%d/fetchKeys?key=%s";  // Put the short ID in the path.
constexpr char kIssueTrustTokenPathFmt[] =
    "/v1/%d/issueTrustToken";  // Put the short ID in the path.

constexpr char kJoinSetPathFmt[] = "/v1/types/%s/sets/%s:join?key=%s";
constexpr char kJoinSetOhttpPath[] = "/v1/proxy/keys?key=";

constexpr char kQuerySetsPath[] = "/v1:query?key=";
constexpr char kQuerySetOhttpPath[] = "/v1/proxy/keys?key=";

#endif  // CHROME_BROWSER_K_ANONYMITY_SERVICE_K_ANONYMITY_SERVICE_URLS_H_
