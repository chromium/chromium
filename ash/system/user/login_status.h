// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_USER_LOGIN_STATUS_H_
#define ASH_SYSTEM_USER_LOGIN_STATUS_H_

#include <string>

#include "ash/login_status.h"
#include "base/strings/string16.h"

namespace ash {
namespace user {

base::string16 GetLocalizedSignOutStringForStatus(LoginStatus status,
                                                  bool multiline);

}  // namespace user
}  // namespace ash

#endif  // ASH_SYSTEM_USER_LOGIN_STATUS_H_
