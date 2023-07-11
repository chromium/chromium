// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_APP_LIST_CONSTANTS_EXPORT_H_
#define ASH_APP_LIST_APP_LIST_CONSTANTS_EXPORT_H_

#if defined(COMPONENT_BUILD)

#if defined(APP_LIST_CONSTANTS_IMPLEMENTATION)
#define APP_LIST_CONSTANTS_EXPORT __attribute__((visibility("default")))
#else
#define APP_LIST_CONSTANTS_EXPORT
#endif

#else  // defined(COMPONENT_BUILD)
#define APP_LIST_CONSTANTS_EXPORT
#endif

#endif  // ASH_APP_LIST_APP_LIST_CONSTANTS_EXPORT_H_
