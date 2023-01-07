// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_RESIZE_SHADOW_TYPE_H_
#define ASH_PUBLIC_CPP_RESIZE_SHADOW_TYPE_H_

namespace ash {

// Resize shadow type.
enum class ResizeShadowType {
  // Unlock type of shadow is shown when user drag on resizable window.
  kUnlock,
  // Lock type of shadow is shown on unresizable window.
  kLock,
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_RESIZE_SHADOW_TYPE_H_
