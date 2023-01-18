// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_MOJO_SERVICE_MANAGER_CONNECTION_HELPER_H_
#define CHROME_BROWSER_ASH_MOJO_SERVICE_MANAGER_CONNECTION_HELPER_H_

#include "base/functional/callback_helpers.h"

namespace ash {
namespace mojo_service_manager {

// Creates the connection to mojo service manager and returns a
// base::ScopedClosureRunner which can close the connection. This connects to a
// fake implementation if the build target is not going to use real ChromeOS
// services.
base::ScopedClosureRunner CreateConnectionAndPassCloser();

}  // namespace mojo_service_manager
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_MOJO_SERVICE_MANAGER_CONNECTION_HELPER_H_
