// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_APP_CHROME_EXE_MAIN_WIN_H_
#define CHROME_APP_CHROME_EXE_MAIN_WIN_H_

#include <stdint.h>

extern "C" {

#if defined(CHROME_EXE_MAIN)
#define CHROME_EXE_MAIN_EXPORT __declspec(dllexport)
#else
#define CHROME_EXE_MAIN_EXPORT
#endif

// Returns the SHA-256 hashes of the specified .pak files.
CHROME_EXE_MAIN_EXPORT __cdecl void GetPakFileHashes(
    const uint8_t** resources_pak,
    const uint8_t** chrome_100_pak,
    const uint8_t** chrome_200_pak);

#undef CHROME_EXE_MAIN_EXPORT

}  // extern "C"

#endif  // CHROME_APP_CHROME_EXE_MAIN_WIN_H_
