// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_APP_SERVICE_PROXY_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_APP_SERVICE_PROXY_H_

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/apps/app_service/app_service_proxy_chromeos.h"
#else
#include "chrome/browser/apps/app_service/app_service_proxy_desktop.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_APP_SERVICE_PROXY_H_
