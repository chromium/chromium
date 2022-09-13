// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_K_ANONYMITY_SERVICE_K_ANONYMITY_SERVICE_URLS_H_
#define CHROME_BROWSER_K_ANONYMITY_SERVICE_K_ANONYMITY_SERVICE_URLS_H_

constexpr char kKAnonymityAuthServer[] =
    "https://chromekanonymityauth-pa.googleapis.com";
constexpr char kGenNonUniqueUserIdPath[] = "/v1/generateShortIdentifier";
constexpr char kFetchKeysPathFmt[] =
    "/v1/%d/fetchKeys?key=%s";  // Put the short ID in the path.
constexpr char kIssueTrustTokenPathFmt[] =
    "/v1/%d/issueTrustToken";  // Put the short ID in the path.

constexpr char kKAnonymityJoinSetServer[] =
    "https://chromekanonymity-pa.googleapis.com";
constexpr char kJoinSetPath[] =
    "/v1/join?key=";  // TODO: Set this when we know the correct path.
constexpr char kJoinSetOhttpPath[] =
    "/v1/proxy/keys?key=";  // TODO: Set this when we know the correct path.

constexpr char kKAnonymityQuerySetServer[] =
    "https://chromekanonymityquery-pa.googleapis.com";
constexpr char kQuerySetPath[] =
    "/v1/query?key=";  // TODO: Set this when we know the correct path.
constexpr char kQuerySetOhttpPath[] =
    "/v1/proxy/keys?key=";  // TODO: Set this when we know the correct path.

#endif  // CHROME_BROWSER_K_ANONYMITY_SERVICE_K_ANONYMITY_SERVICE_URLS_H_
