// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_KEYBOARD_UI_KEYBOARD_EXPORT_H_
#define ASH_KEYBOARD_UI_KEYBOARD_EXPORT_H_

// Defines KEYBOARD_EXPORT so that functionality implemented by the
// keyboard module can be exported to consumers.

#if defined(COMPONENT_BUILD)
#if defined(WIN32)

#if defined(KEYBOARD_IMPLEMENTATION)
#define KEYBOARD_EXPORT __declspec(dllexport)
#else
#define KEYBOARD_EXPORT __declspec(dllimport)
#endif  // defined(KEYBOARD_IMPLEMENTATION)

#else  // defined(WIN32)
#if defined(KEYBOARD_IMPLEMENTATION)
#define KEYBOARD_EXPORT __attribute__((visibility("default")))
#else
#define KEYBOARD_EXPORT
#endif
#endif

#else  // defined(COMPONENT_BUILD)
#define KEYBOARD_EXPORT
#endif

#endif  // ASH_KEYBOARD_UI_KEYBOARD_EXPORT_H_
