// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHORTCUT_VIEWER_KSV_EXPORT_H_
#define ASH_SHORTCUT_VIEWER_KSV_EXPORT_H_

// Defines KSV_EXPORT so that functionality implemented by
// the keyboard shortcut viewer module can be exported to consumers.

#if defined(COMPONENT_BUILD)

#if defined(KSV_IMPLEMENTATION)
#define KSV_EXPORT __attribute__((visibility("default")))
#else
#define KSV_EXPORT
#endif

#else  // defined(COMPONENT_BUILD)
#define KSV_EXPORT
#endif

#endif  // ASH_SHORTCUT_VIEWER_KSV_EXPORT_H_
