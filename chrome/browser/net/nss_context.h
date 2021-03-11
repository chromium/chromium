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
class BrowserContext;
}  // namespace content

// NssCertDatabaseGetter is a callback that MUST only be invoked on the IO
// thread, and will either synchronously return the associated NSSCertDatabase*
// (if available), or nullptr along with a commitment to asynchronously invoke
// the caller-supplied callback once the NSSCertDatabase* has been initialized.
// Ownership of the NSSCertDatabase is not transferred, and the lifetime should
// only be considered valid for the current Task.
//
// TODO(https://crbug.com/1186373): Provide better lifetime guarantees.
using NssCertDatabaseGetter = base::OnceCallback<net::NSSCertDatabase*(
    base::OnceCallback<void(net::NSSCertDatabase*)> callback)>;

// Must be called on the UI thread. Returns a Getter that may only be invoked on
// the IO thread. To avoid UAF, the getter must be immediately posted to the IO
// thread and then invoked.
NssCertDatabaseGetter CreateNSSCertDatabaseGetter(
    content::BrowserContext* browser_context);

// Gets a pointer to the NSSCertDatabase for the user associated with |context|.
// It's a wrapper around |GetNSSCertDatabaseForResourceContext| which makes
// sure it's called on IO thread (with |profile|'s resource context). The
// callback will be called on the originating message loop.
// It's accessing profile, so it should be called on the UI thread.
void GetNSSCertDatabaseForProfile(
    Profile* profile,
    base::OnceCallback<void(net::NSSCertDatabase*)> callback);

#endif  // CHROME_BROWSER_NET_NSS_CONTEXT_H_
