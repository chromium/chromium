// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_ASH_PUBLIC_EXPORT_H_
#define ASH_PUBLIC_CPP_ASH_PUBLIC_EXPORT_H_

#if defined(COMPONENT_BUILD)

#if defined(ASH_PUBLIC_IMPLEMENTATION)
#define ASH_PUBLIC_EXPORT __attribute((visibility("default")))
#else
#define ASH_PUBLIC_EXPORT
#endif

#else  // !defined(COMPONENT_BUILD)

#define ASH_PUBLIC_EXPORT

#endif  // defined(COMPONENT_BUILD)

#endif  // ASH_PUBLIC_CPP_ASH_PUBLIC_EXPORT_H_
