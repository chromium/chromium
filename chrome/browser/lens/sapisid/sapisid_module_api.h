// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LENS_SAPISID_SAPISID_MODULE_API_H_
#define CHROME_BROWSER_LENS_SAPISID_SAPISID_MODULE_API_H_

#include <stdint.h>

#if defined(WIN32)
#define SAPISID_EXPORT __declspec(dllexport)
#else
#define SAPISID_EXPORT __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

SAPISID_EXPORT void SAPI_Initialize(void);
SAPISID_EXPORT int GenerateSapisidHash(const char* email,
                                       const char* sapisid,
                                       const char* origin,
                                       int64_t timestamp_millis,
                                       char** out_hash);
SAPISID_EXPORT void FreeSapisidHash(char* out_hash);

#ifdef __cplusplus
}
#endif

#endif  // CHROME_BROWSER_LENS_SAPISID_SAPISID_MODULE_API_H_
