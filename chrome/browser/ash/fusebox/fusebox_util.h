// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FUSEBOX_FUSEBOX_UTIL_H_
#define CHROME_BROWSER_ASH_FUSEBOX_FUSEBOX_UTIL_H_

#include <base/files/file.h>

namespace fusebox {

// Returns errno code for a base::File::Error |error| code.
int FileErrorToErrno(base::File::Error error);

// Returns errno code for a net:: |error| code.
int NetErrorToErrno(int error);

}  // namespace fusebox

#endif  // CHROME_BROWSER_ASH_FUSEBOX_FUSEBOX_UTIL_H_
