// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_APP_SERVICE_PROXY_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_APP_SERVICE_PROXY_H_

#include "build/chromeos_buildflags.h"
#include "chrome/browser/apps/app_service/app_service_proxy_forward.h"  // IWYU pragma: export

// TODO(crbug.com/40251315): Extract a common AppServiceProxy interface and
// inherit from it instead of swapping out separate (re)definitions of the same
// interface.
#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/apps/app_service/app_service_proxy_ash.h"  // IWYU pragma: export
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/browser/apps/app_service/app_service_proxy_lacros.h"  // IWYU pragma: export
#else
#include "chrome/browser/apps/app_service/app_service_proxy_desktop.h"  // IWYU pragma: export
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_APP_SERVICE_PROXY_H_
