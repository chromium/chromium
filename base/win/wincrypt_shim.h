// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_WIN_WINCRYPT_SHIM_H_
#define BASE_WIN_WINCRYPT_SHIM_H_

// Deprecated. Historically, it was necessary to use this shim header to include
// <wincrypt.h>.
// TODO(crbug.com/483973077): Replace uses of this header with a normal
// wincrypt.h include.

#include <windows.h>

#include <wincrypt.h>

#endif  // BASE_WIN_WINCRYPT_SHIM_H_
