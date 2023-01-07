// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_MOJO_MEDIA_ROUTE_PROVIDER_UTIL_WIN_H_
#define CHROME_BROWSER_MEDIA_ROUTER_MOJO_MEDIA_ROUTE_PROVIDER_UTIL_WIN_H_

#include "base/functional/callback.h"

namespace media_router {

// Asynchronously checks whether there will be a firewall prompt for using local
// ports on Windows. |callback| will be called with the result where |true|
// means that local ports can be used without triggering a firewall prompt.
void CanFirewallUseLocalPorts(base::OnceCallback<void(bool)> callback);

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_MOJO_MEDIA_ROUTE_PROVIDER_UTIL_WIN_H_
