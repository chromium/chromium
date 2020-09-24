// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SEARCH_BOX_SEARCH_BOX_EXPORT_H_
#define ASH_SEARCH_BOX_SEARCH_BOX_EXPORT_H_

// Defines SEARCH_BOX_EXPORT so that functionality implemented by the search_box
// module can be exported to consumers.

#if defined(COMPONENT_BUILD)
#if defined(WIN32)

#if defined(SEARCH_BOX_IMPLEMENTATION)
#define SEARCH_BOX_EXPORT __declspec(dllexport)
#else
#define SEARCH_BOX_EXPORT __declspec(dllimport)
#endif  // defined(SEARCH_BOX_IMPLEMENTATION)

#else  // defined(WIN32)
#if defined(SEARCH_BOX_IMPLEMENTATION)
#define SEARCH_BOX_EXPORT __attribute__((visibility("default")))
#else
#define SEARCH_BOX_EXPORT
#endif
#endif

#else  // defined(COMPONENT_BUILD)
#define SEARCH_BOX_EXPORT
#endif

#endif  // ASH_SEARCH_BOX_SEARCH_BOX_EXPORT_H_
