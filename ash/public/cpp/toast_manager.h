// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_TOAST_MANAGER_H_
#define ASH_PUBLIC_CPP_TOAST_MANAGER_H_

#include "ash/public/cpp/ash_public_export.h"

namespace ash {

struct ToastData;

// Public interface to show toasts.
class ASH_PUBLIC_EXPORT ToastManager {
 public:
  static ToastManager* Get();

  // Show a toast. If there are queued toasts, succeeding toasts are queued as
  // well, and are shown one by one.
  virtual void Show(const ToastData& data) = 0;

 protected:
  ToastManager();
  virtual ~ToastManager();
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_TOAST_MANAGER_H_
