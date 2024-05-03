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

}  // namespace prefs

#endif  // CHROME_BROWSER_SIGNIN_CHROME_SIGNIN_PREF_NAMES_H_
