// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_PRINTING_OAUTH2_CONSTANTS_H_
#define CHROME_BROWSER_ASH_PRINTING_OAUTH2_CONSTANTS_H_

namespace ash {
namespace printing {
namespace oauth2 {

// TODO(pawliczek) - this value is not known yet.
// This is the (internal) URL which the internet browser must be redirected to
// to complete the authorization procedure.
constexpr char kRedirectURI[] = "https://TODO.set.redirect.uri/for/ipp/oauth2";

// When required, ChromeOS tries to register to Authorization Server with this
// name.
constexpr char kClientName[] = "ChromeOS";

}  // namespace oauth2
}  // namespace printing
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_PRINTING_OAUTH2_CONSTANTS_H_
