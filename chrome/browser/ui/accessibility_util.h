// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ACCESSIBILITY_UTIL_H_
#define CHROME_BROWSER_UI_ACCESSIBILITY_UTIL_H_

#include <string>


// Announces |message| as an accessibility alert in the currently active normal
// browser window, if there is one. Otherwise, no announcement is made.
void AnnounceInActiveBrowser(const std::u16string& message);

#endif  // CHROME_BROWSER_UI_ACCESSIBILITY_UTIL_H_
