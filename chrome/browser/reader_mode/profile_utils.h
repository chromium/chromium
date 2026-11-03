// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_READER_MODE_PROFILE_UTILS_H_
#define CHROME_BROWSER_READER_MODE_PROFILE_UTILS_H_

#include "chrome/browser/profiles/profile.h"

namespace reader_mode {

// Setup URLDataSource for the chrome-distiller:// scheme for the given
// |profile|.
void RegisterViewerSource(Profile* profile);

}  // namespace reader_mode

#endif  // CHROME_BROWSER_READER_MODE_PROFILE_UTILS_H_
