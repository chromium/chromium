// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/types/is_arc_pointer.h"

#import <Foundation/Foundation.h>

namespace base {
namespace {

// Some basic pointers that should not be ARC managed.
static_assert(!IsArcPointer<int*>);
static_assert(!IsArcPointer<void*>);

// Objective-C object pointers
static_assert(IsArcPointer<id>);
static_assert(IsArcPointer<NSString*>);

// Block pointers
static_assert(IsArcPointer<void (^)()>);

}  // namespace
}  // namespace base
