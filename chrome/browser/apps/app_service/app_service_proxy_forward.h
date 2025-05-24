// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_APP_SERVICE_PROXY_FORWARD_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_APP_SERVICE_PROXY_FORWARD_H_

#include "build/build_config.h"

namespace apps {

// Include this header to forward-declare AppServiceProxy in a way that is
// compatible across all platforms.
#if BUILDFLAG(IS_CHROMEOS)
class AppServiceProxyAsh;
using AppServiceProxy = AppServiceProxyAsh;
#else
class AppServiceProxy;
#endif  // BUILDFLAG(IS_CHROMEOS)

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_APP_SERVICE_PROXY_FORWARD_H_
