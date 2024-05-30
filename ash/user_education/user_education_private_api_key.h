// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_USER_EDUCATION_USER_EDUCATION_PRIVATE_API_KEY_H_
#define ASH_USER_EDUCATION_USER_EDUCATION_PRIVATE_API_KEY_H_

namespace ash {

class CaptureModeController;
class WelcomeTourController;

// A `base::PassKey`-like construct used to restrict access to private user
// education APIs to only authorized callers.
class UserEducationPrivateApiKey {
 private:
  UserEducationPrivateApiKey() = default;

  friend CaptureModeController;
  friend WelcomeTourController;
};

}  // namespace ash

#endif  // ASH_USER_EDUCATION_USER_EDUCATION_PRIVATE_API_KEY_H_
