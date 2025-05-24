// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_DEVELOPER_PRIVATE_EXTENSION_INFO_GENERATOR_H_
#define CHROME_BROWSER_EXTENSIONS_API_DEVELOPER_PRIVATE_EXTENSION_INFO_GENERATOR_H_

#include "build/build_config.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/extensions/api/developer_private/extension_info_generator_android.h"
#else
#include "chrome/browser/extensions/api/developer_private/extension_info_generator_desktop.h"
#endif

#endif  // CHROME_BROWSER_EXTENSIONS_API_DEVELOPER_PRIVATE_EXTENSION_INFO_GENERATOR_H_
