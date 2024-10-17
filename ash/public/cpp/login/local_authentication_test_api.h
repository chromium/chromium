// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_LOGIN_LOCAL_AUTHENTICATION_TEST_API_H_
#define ASH_PUBLIC_CPP_LOGIN_LOCAL_AUTHENTICATION_TEST_API_H_

#include <string>

#include "ash/public/cpp/ash_public_export.h"

namespace ash {

// LocalAuthenticationTestApi serves as a test api for
// LocalAuthenticationRequestController class.
class ASH_PUBLIC_EXPORT LocalAuthenticationTestApi {
 public:
  LocalAuthenticationTestApi() = default;
  virtual ~LocalAuthenticationTestApi() = default;
  virtual void SubmitPassword(const std::string& password) = 0;
  virtual void SubmitPin(const std::string& password) = 0;
  virtual void Close() = 0;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_LOGIN_LOCAL_AUTHENTICATION_TEST_API_H_
