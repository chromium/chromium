// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_CHROME_SIGNIN_PREF_NAMES_H_
#define CHROME_BROWSER_SIGNIN_CHROME_SIGNIN_PREF_NAMES_H_

namespace prefs {

// Dictionary pref to store the number of times a user declined profile creation
// per account. The key is the hash of the email.
inline constexpr char kProfileCreationInterceptionDeclined[] =
    "signin.ProfileCreationInterceptionDeclinedPref";

// Dictionary pref to store the number of times the Chrome Signin Bubble was
// successively declined per account. The key is the hash of the email.
inline constexpr char kChromeSigninInterceptionDeclined[] =
    "signin.ChromeSigninInterceptionDeclinedPref";

// Dictionary pref to store the number of times the Chrome Signin Bubble is
// shown per account. The key is the hash of the email.
inline constexpr char kChromeSigninInterceptionShownCount[] =
    "signin.ChromeSigninInterceptionShownCountPref";

// Dictionary pref to store the user choice for the Chrome Signin Intercept per
// account. The key is the hash of the email.
inline constexpr char kChromeSigninInterceptionUserChoice[] =
    "signin.ChromeSigninInterceptionUserChoice";

// Dictionary pref to store the number of dismisses of the Chrome Signin Bubble
// per account. The key is the hash of the email.
inline constexpr char kChromeSigninInterceptionDismissCount[] =
    "signin.ChromeSigninInterceptionDismissCount";

}  // namespace prefs

#endif  // CHROME_BROWSER_SIGNIN_CHROME_SIGNIN_PREF_NAMES_H_
