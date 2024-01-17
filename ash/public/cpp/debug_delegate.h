// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_DEBUG_DELEGATE_H_
#define ASH_PUBLIC_CPP_DEBUG_DELEGATE_H_

#include "ash/public/cpp/ash_public_export.h"

namespace ash {

// An interface used to listen to debug related events in Ash.
class ASH_PUBLIC_EXPORT DebugDelegate {
 public:
  // Prints ui::Layer hierarchy for all windows.
  virtual void PrintLayerHierarchy() = 0;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_DEBUG_DELEGATE_H_
