// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_SYSTEM_SCOPED_TOAST_PAUSE_H_
#define ASH_PUBLIC_CPP_SYSTEM_SCOPED_TOAST_PAUSE_H_

#include "ash/public/cpp/ash_public_export.h"

namespace ash {

// An object that pauses toasts for its lifetime.
class ASH_PUBLIC_EXPORT ScopedToastPause {
 public:
  ScopedToastPause();
  ScopedToastPause(const ScopedToastPause&) = delete;
  ScopedToastPause& operator=(const ScopedToastPause&) = delete;
  ~ScopedToastPause();
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_SYSTEM_SCOPED_TOAST_PAUSE_H_
