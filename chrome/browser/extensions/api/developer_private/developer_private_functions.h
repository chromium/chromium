// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_DEVELOPER_PRIVATE_DEVELOPER_PRIVATE_FUNCTIONS_H_
#define CHROME_BROWSER_EXTENSIONS_API_DEVELOPER_PRIVATE_DEVELOPER_PRIVATE_FUNCTIONS_H_

#include "build/build_config.h"
#include "chrome/browser/extensions/api/developer_private/developer_private_functions_shared.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/extensions/api/developer_private/developer_private_functions_android.h"
#else
#include "chrome/browser/extensions/api/developer_private/developer_private_functions_desktop.h"
#endif

#endif  // CHROME_BROWSER_EXTENSIONS_API_DEVELOPER_PRIVATE_DEVELOPER_PRIVATE_FUNCTIONS_H_
