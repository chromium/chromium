// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_OOBE_QUICK_START_CONNECTIVITY_AUTHENTICATED_CONNECTION_H_
#define CHROME_BROWSER_ASH_LOGIN_OOBE_QUICK_START_CONNECTIVITY_AUTHENTICATED_CONNECTION_H_

#include "chrome/browser/ash/login/oobe_quick_start/connectivity/connection.h"

namespace ash::quick_start {

// Represents a connection that's been authenticated by the shapes verification
// or QR code flow.
class AuthenticatedConnection : public Connection {};

}  // namespace ash::quick_start

#endif  // CHROME_BROWSER_ASH_LOGIN_OOBE_QUICK_START_CONNECTIVITY_AUTHENTICATED_CONNECTION_H_
