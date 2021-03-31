// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_GUEST_OS_GUEST_OS_EXTERNAL_PROTOCOL_HANDLER_H_
#define CHROME_BROWSER_ASH_GUEST_OS_GUEST_OS_EXTERNAL_PROTOCOL_HANDLER_H_

#include "base/optional.h"
#include "chrome/browser/ash/guest_os/guest_os_registry_service.h"

class Profile;

namespace guest_os {

// Returns handler for |url| if one exists.
base::Optional<GuestOsRegistryService::Registration> GetHandler(
    Profile* profile,
    const GURL& url);

// Launches the app configured to handle |url| if one exists.
void Launch(Profile* profile, const GURL& url);

}  // namespace guest_os

#endif  // CHROME_BROWSER_ASH_GUEST_OS_GUEST_OS_EXTERNAL_PROTOCOL_HANDLER_H_
