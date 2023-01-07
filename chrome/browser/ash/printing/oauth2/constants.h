// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_PRINTING_OAUTH2_CONSTANTS_H_
#define CHROME_BROWSER_ASH_PRINTING_OAUTH2_CONSTANTS_H_

namespace ash {
namespace printing {
namespace oauth2 {

// This is the (internal) URL which the internet browser must be redirected to
// to complete the authorization procedure. This URI does not point to a real
// page.
constexpr char kRedirectURI[] =
    "https://chromeos.test/printing/oauth2/redirectURI";

// When required, ChromeOS tries to register to Authorization Server with this
// name.
constexpr char kClientName[] = "ChromeOS";

// Max number of parallel OAuth2 sessions with one Authorization Server.
constexpr size_t kMaxNumberOfSessions = 8;

}  // namespace oauth2
}  // namespace printing
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_PRINTING_OAUTH2_CONSTANTS_H_
