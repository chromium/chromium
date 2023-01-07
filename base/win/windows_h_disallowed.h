// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is designed to be included if windows.h is included from a source
// file that should not need it. Conditionally including this file from a few
// key source files will help to stop windows.h from creeping back into the
// Chromium build, with the namespace pollution which that implies. Typical
// usage is:
//
// // This should be after all other #includes.
// #if defined(_WINDOWS_)  // Detect whether windows.h was included.
// #include "base/win/windows_h_disallowed.h"
// #endif  // defined(_WINDOWS_)
//
// See https://crbug.com/796644 for more historical context.

#ifndef BASE_WIN_WINDOWS_H_DISALLOWED_H_
#define BASE_WIN_WINDOWS_H_DISALLOWED_H_

#error Windows.h was included unexpectedly. See comment above for details.

#endif  // BASE_WIN_WINDOWS_H_DISALLOWED_H_
