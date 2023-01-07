// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_QUICK_UNLOCK_FINGERPRINT_UTILS_H_
#define CHROME_BROWSER_ASH_LOGIN_QUICK_UNLOCK_FINGERPRINT_UTILS_H_

namespace user_manager {
class User;
}

namespace ash {
enum class FingerprintState;

namespace quick_unlock {
enum class Purpose;

FingerprintState GetFingerprintStateForUser(const user_manager::User* user,
                                            Purpose purpose);

}  // namespace quick_unlock
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_QUICK_UNLOCK_FINGERPRINT_UTILS_H_
