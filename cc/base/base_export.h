// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_BASE_BASE_EXPORT_H_
#define CC_BASE_BASE_EXPORT_H_

#if defined(COMPONENT_BUILD)
#if defined(WIN32)

#if defined(CC_BASE_IMPLEMENTATION)
#define CC_BASE_EXPORT __declspec(dllexport)
#else
#define CC_BASE_EXPORT __declspec(dllimport)
#endif  // defined(CC_BASE_IMPLEMENTATION)

#else  // defined(WIN32)
#if defined(CC_BASE_IMPLEMENTATION)
#define CC_BASE_EXPORT __attribute__((visibility("default")))
#else
#define CC_BASE_EXPORT
#endif
#endif

#else  // defined(COMPONENT_BUILD)
#define CC_BASE_EXPORT
#endif

#endif  // CC_BASE_BASE_EXPORT_H_
