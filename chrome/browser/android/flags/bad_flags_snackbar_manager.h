// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_FLAGS_BAD_FLAGS_SNACKBAR_MANAGER_H_
#define CHROME_BROWSER_ANDROID_FLAGS_BAD_FLAGS_SNACKBAR_MANAGER_H_

#include <string>

#include "content/public/browser/web_contents.h"

// Creates and shows a Bad Flags snackbar.
void ShowBadFlagsSnackbar(content::WebContents* web_contents,
                          const std::u16string& message);

#endif  // CHROME_BROWSER_ANDROID_FLAGS_BAD_FLAGS_SNACKBAR_MANAGER_H_
