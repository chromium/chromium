// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_ANDROID_FEATURES_STACK_UNWINDER_PUBLIC_FUNCTION_TYPES_H_
#define CHROME_ANDROID_FEATURES_STACK_UNWINDER_PUBLIC_FUNCTION_TYPES_H_

namespace stack_unwinder {

// Type declarations for C++ functions exported by the module.
using DoNothingFunction = void (*)();

}  // namespace stack_unwinder

#endif  // CHROME_ANDROID_FEATURES_STACK_UNWINDER_PUBLIC_FUNCTION_TYPES_H_
