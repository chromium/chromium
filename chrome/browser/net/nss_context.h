// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_NSS_CONTEXT_H_
#define CHROME_BROWSER_NET_NSS_CONTEXT_H_

#include <string>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "crypto/scoped_nss_types.h"

class Profile;

namespace net {
class NSSCertDatabase;
}

namespace content {
class ResourceContext;
}  // namespace content

// Returns a pointer to the NSSCertDatabase for the user associated with
// |context|, if it is ready. If it is not ready and |callback| is non-null, the
// |callback| will be run once the DB is initialized. Ownership is not
// transferred, but the caller may save the pointer, which will remain valid for
// the lifetime of the ResourceContext.
// Must be called only on the IO thread.
net::NSSCertDatabase* GetNSSCertDatabaseForResourceContext(
    content::ResourceContext* context,
    base::OnceCallback<void(net::NSSCertDatabase*)> callback)
    WARN_UNUSED_RESULT;

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Enables the system key slot in the NSSCertDatabase for the user associated
// with |context|.
// Must be called only on the IO thread.
void EnableNSSSystemKeySlotForResourceContext(
    content::ResourceContext* context);
#endif

// Gets a pointer to the NSSCertDatabase for the user associated with |context|.
// It's a wrapper around |GetNSSCertDatabaseForResourceContext| which makes
// sure it's called on IO thread (with |profile|'s resource context). The
// callback will be called on the originating message loop.
// It's accessing profile, so it should be called on the UI thread.
void GetNSSCertDatabaseForProfile(
    Profile* profile,
    base::OnceCallback<void(net::NSSCertDatabase*)> callback);

#endif  // CHROME_BROWSER_NET_NSS_CONTEXT_H_
